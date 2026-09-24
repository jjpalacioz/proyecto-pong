/* ============================================================================
 *  client_handler.c  —  Logica que ejecuta cada hilo de cliente
 * ----------------------------------------------------------------------------
 *  Cada cliente conectado es atendido por un hilo que ejecuta client_thread().
 *  Ese hilo: registra al cliente, y (en fases futuras) lo lleva a matchmaking
 *  y al juego. Al terminar, cierra el socket y libera su memoria.
 * ========================================================================== */
#include "client_handler.h"

#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* memset, strerror */
#include <unistd.h>     /* close */
#include <errno.h>
#include <pthread.h>    /* pthread_mutex_* */
#include <arpa/inet.h>  /* inet_ntoa, htonl, ntohs */

#include <fcntl.h>      /* fcntl -> socket no bloqueante para leer input */

#include "logger.h"
#include "protocol.h"
#include "protocol_io.h"
#include "player.h"
#include "game.h"       /* GameState y fisica del juego */

/* --------------------------------------------------------------------------
 *  Estado GLOBAL compartido por todos los hilos.
 * --------------------------------------------------------------------------
 *  next_player_id lo incrementa CADA hilo cuando registra un cliente. Si dos
 *  hilos lo tocaran a la vez, podrian asignar el MISMO id (condicion de
 *  carrera). Por eso lo protegemos con un mutex: solo un hilo a la vez puede
 *  leer-e-incrementar el contador.
 * -------------------------------------------------------------------------- */
static uint32_t        next_player_id = 1;
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Asigna un id unico de forma segura entre hilos (thread-safe). */
static uint32_t assign_player_id(void) {
    pthread_mutex_lock(&id_mutex);      /* entrar a la zona critica */
    uint32_t id = next_player_id++;     /* leer e incrementar (atomico gracias al lock) */
    pthread_mutex_unlock(&id_mutex);    /* salir de la zona critica */
    return id;
}

/* --------------------------------------------------------------------------
 *  handle_register: procesa un MSG_REGISTER (igual que en Fase 3, ahora aqui).
 *  El payload es: [1 len_nick][nick][1 len_mail][mail]
 *  Devuelve 1 si el registro fue exitoso, 0 si fue rechazado.
 * -------------------------------------------------------------------------- */
static int handle_register(int client_fd, const Message *msg, Player *player) {
    const uint8_t *p = msg->payload;
    uint16_t remaining = msg->length;

    /* --- nickname --- */
    if (remaining < 1) { send_error(client_fd, ERR_INVALID_FIELD); return 0; }
    uint8_t len_nick = *p; p++; remaining--;
    if (len_nick == 0 || len_nick > MAX_NICK_LEN || len_nick > remaining) {
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    memcpy(player->nickname, p, len_nick);
    player->nickname[len_nick] = '\0';
    p += len_nick; remaining -= len_nick;

    /* --- email --- */
    if (remaining < 1) { send_error(client_fd, ERR_INVALID_FIELD); return 0; }
    uint8_t len_mail = *p; p++; remaining--;
    if (len_mail == 0 || len_mail > MAX_EMAIL_LEN || len_mail > remaining) {
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    memcpy(player->email, p, len_mail);
    player->email[len_mail] = '\0';

    /* --- registro valido: asignar id (thread-safe) y responder --- */
    player->id = assign_player_id();

    uint32_t id_net = htonl(player->id);
    send_message(client_fd, MSG_REGISTER_OK, &id_net, sizeof(id_net));

    log_msg(LOG_INFO, "Registro OK: id=%u nick='%s' email='%s'",
            player->id, player->nickname, player->email);
    return 1;
}

/* --------------------------------------------------------------------------
 *  send_state: empaqueta el GameState en un MSG_STATE y lo envia al cliente.
 *  Payload (10 bytes): ball_x(2) ball_y(2) paddle_left(2) paddle_right(2)
 *                      score_left(1) score_right(1). Todo en orden de red.
 * -------------------------------------------------------------------------- */
/* Recorta un valor float al rango [0, max] y lo devuelve como uint16_t.
 * Evita que una posicion negativa (pelota saliendo del campo) se "envuelva"
 * a un numero enorme al convertir a entero sin signo. */
static uint16_t clamp_u16(float v, float max) {
    if (v < 0) v = 0;
    if (v > max) v = max;
    return (uint16_t)v;
}

static int send_state(int client_fd, const GameState *g) {
    uint8_t payload[10];
    uint16_t ball_x = htons(clamp_u16(g->ball_x, FIELD_WIDTH));
    uint16_t ball_y = htons(clamp_u16(g->ball_y, FIELD_HEIGHT));
    uint16_t pl     = htons(clamp_u16(g->paddle_left, FIELD_HEIGHT));
    uint16_t pr     = htons(clamp_u16(g->paddle_right, FIELD_HEIGHT));

    memcpy(&payload[0], &ball_x, 2);
    memcpy(&payload[2], &ball_y, 2);
    memcpy(&payload[4], &pl, 2);
    memcpy(&payload[6], &pr, 2);
    payload[8] = g->score_left;
    payload[9] = g->score_right;

    return send_message(client_fd, MSG_STATE, payload, sizeof(payload));
}

/* --------------------------------------------------------------------------
 *  run_practice_game: partida de PRACTICA de un solo cliente (Fase 6).
 * --------------------------------------------------------------------------
 *  Permite ver el juego funcionando ANTES de tener matchmaking (Fase 7).
 *  El cliente controla la paleta izquierda; la derecha la controla una IA
 *  simple que sigue la pelota. El servidor simula la fisica ~60 veces/seg y
 *  envia MSG_STATE (con el score) en cada tick -> score en tiempo real.
 *
 *  Para no bloquear el juego esperando input, el socket se pone en modo NO
 *  bloqueante: se leen los MSG_INPUT que haya llegado y si no hay, se sigue.
 * -------------------------------------------------------------------------- */
static void run_practice_game(int client_fd, const Player *player) {
    log_msg(LOG_INFO, "[fd=%d] Iniciando partida de practica para '%s'",
            client_fd, player->nickname);

    /* Avisar al cliente que la partida arranca. */
    send_message(client_fd, MSG_GAME_START, NULL, 0);

    GameState g;
    game_init(&g);

    /* Poner el socket en modo no bloqueante para leer input sin frenar. */
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    while (!g.game_over) {
        /* --- 1. Leer TODOS los MSG_INPUT pendientes (sin bloquear) --- */
        Message msg;
        RecvResult res = recv_message(client_fd, &msg);
        if (res == MSG_OK) {
            if (msg.type == MSG_INPUT && msg.length >= 1) {
                game_move_paddle(&g, SIDE_LEFT, msg.payload[0]);
            } else if (msg.type == MSG_DISCONNECT) {
                log_msg(LOG_INFO, "[fd=%d] Cliente pidio desconectar", client_fd);
                return;
            }
        } else if (res == MSG_CLOSED) {
            log_msg(LOG_INFO, "[fd=%d] Cliente desconectado durante la partida", client_fd);
            return;
        } else if (res == MSG_ERR_IO) {
            /* En modo no bloqueante, "no hay datos" llega como error EAGAIN:
             * eso NO es un problema, solo significa "sin input este tick". */
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log_msg(LOG_ERROR, "[fd=%d] Error de E/S en partida", client_fd);
                return;
            }
        }

        /* --- 2. IA simple y JUGABLE: la paleta derecha sigue la pelota, pero
         * solo cuando la pelota viene hacia ella y con una "zona muerta" amplia,
         * de modo que a veces falle y el jugador pueda anotar. --- */
        if (g.ball_vx > 0) {  /* solo reacciona si la pelota va hacia la derecha */
            float paddle_center = g.paddle_right + PADDLE_HEIGHT / 2.0f;
            if (g.ball_y < paddle_center - 25) {
                game_move_paddle(&g, SIDE_RIGHT, DIR_UP);
            } else if (g.ball_y > paddle_center + 25) {
                game_move_paddle(&g, SIDE_RIGHT, DIR_DOWN);
            }
        }

        /* --- 3. Avanzar la fisica un tick --- */
        game_tick(&g);

        /* --- 4. Enviar el estado (incluye el score) al cliente --- */
        if (send_state(client_fd, &g) < 0) {
            log_msg(LOG_INFO, "[fd=%d] No se pudo enviar estado (cliente cerro?)", client_fd);
            return;
        }

        /* --- 5. Dormir ~1/60 s para ir a ~60 ticks por segundo --- */
        usleep(1000000 / TICK_RATE);
    }

    /* --- Fin de partida: avisar ganador y score final --- */
    uint8_t payload[3] = { g.winner_side, g.score_left, g.score_right };
    send_message(client_fd, MSG_GAME_OVER, payload, sizeof(payload));
    log_msg(LOG_INFO, "[fd=%d] Partida terminada. Ganador=%s  score %d-%d",
            client_fd, g.winner_side == SIDE_LEFT ? "izquierda" : "derecha",
            g.score_left, g.score_right);
}

/* --------------------------------------------------------------------------
 *  client_thread: punto de entrada del hilo. Atiende un cliente completo.
 * -------------------------------------------------------------------------- */
void *client_thread(void *arg) {
    /* Recuperar el contexto que nos paso el hilo principal. */
    ClientContext *ctx = (ClientContext *)arg;
    int client_fd = ctx->client_fd;

    /* Texto de la direccion del cliente para los logs. inet_ntoa usa un buffer
     * estatico, asi que copiamos el resultado a una variable local propia. */
    char ip[INET_ADDRSTRLEN];
    snprintf(ip, sizeof(ip), "%s", inet_ntoa(ctx->client_addr.sin_addr));
    int cport = ntohs(ctx->client_addr.sin_port);

    log_msg(LOG_INFO, "[fd=%d] Cliente conectado desde %s:%d", client_fd, ip, cport);

    /* --- Fase de registro --- */
    Player player;
    memset(&player, 0, sizeof(player));
    int registered = 0;

    while (!registered) {
        Message msg;
        RecvResult res = recv_message(client_fd, &msg);

        if (res == MSG_CLOSED) {
            log_msg(LOG_INFO, "[fd=%d] Cliente desconectado antes de registrarse", client_fd);
            break;
        } else if (res == MSG_ERR_IO) {
            log_msg(LOG_ERROR, "[fd=%d] Error de E/S: %s", client_fd, strerror(errno));
            break;
        } else if (res == MSG_ERR_MAGIC) {
            log_msg(LOG_WARN, "[fd=%d] MAGIC invalido -> se rechaza", client_fd);
            send_error(client_fd, ERR_BAD_MAGIC);
            break;
        } else if (res == MSG_ERR_VERSION) {
            log_msg(LOG_WARN, "[fd=%d] Version no soportada", client_fd);
            send_error(client_fd, ERR_BAD_VERSION);
            break;
        } else if (res == MSG_ERR_LENGTH) {
            log_msg(LOG_WARN, "[fd=%d] LENGTH invalido", client_fd);
            send_error(client_fd, ERR_BAD_LENGTH);
            break;
        }

        log_msg(LOG_INFO, "[fd=%d] Mensaje: type=0x%02X length=%u",
                client_fd, msg.type, msg.length);

        if (msg.type == MSG_REGISTER) {
            registered = handle_register(client_fd, &msg, &player);
        } else {
            log_msg(LOG_WARN, "[fd=%d] Se esperaba MSG_REGISTER pero llego 0x%02X",
                    client_fd, msg.type);
            send_error(client_fd, ERR_NOT_REGISTERED);
        }
    }

    if (registered) {
        log_msg(LOG_INFO, "[fd=%d] Jugador '%s' (id=%u) registrado y listo",
                client_fd, player.nickname, player.id);

        /* Esperar a que el cliente pida jugar (MSG_QUEUE) y arrancar la
         * partida de practica (Fase 6). El matchmaking real 1vs1 llega en la
         * Fase 7; por ahora, MSG_QUEUE inicia una partida contra la IA. */
        Message msg;
        RecvResult res = recv_message(client_fd, &msg);
        if (res == MSG_OK && msg.type == MSG_QUEUE) {
            run_practice_game(client_fd, &player);
        } else if (res == MSG_OK) {
            log_msg(LOG_WARN, "[fd=%d] Se esperaba MSG_QUEUE pero llego 0x%02X",
                    client_fd, msg.type);
        }
    }

    /* --- Limpieza: cerrar socket y liberar el contexto --- */
    close(client_fd);
    free(ctx);  /* liberamos la memoria que reservo el hilo principal con malloc */
    log_msg(LOG_INFO, "[fd=%d] Hilo de cliente finalizado", client_fd);

    return NULL;
}
