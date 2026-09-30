/* ============================================================================
 *  match.c  —  Implementacion del matchmaking y la partida 1vs1
 * ========================================================================== */
#include "match.h"

#include <stdlib.h>     /* malloc, free */
#include <string.h>     /* memcpy */
#include <unistd.h>     /* usleep */
#include <fcntl.h>      /* fcntl -> socket no bloqueante */
#include <errno.h>
#include <arpa/inet.h>  /* htons, htonl */

#include "logger.h"
#include "protocol.h"
#include "protocol_io.h"

/* --------------------------------------------------------------------------
 *  Sala de espera (cola de matchmaking).
 * --------------------------------------------------------------------------
 *  Guardamos UN jugador en espera a la vez. Cuando llega el segundo, se forma
 *  la pareja. Todo protegido por un mutex global porque varios hilos (uno por
 *  cliente) pueden intentar emparejarse al mismo tiempo.
 * -------------------------------------------------------------------------- */
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Datos del jugador que quedo esperando rival. */
static int    waiting_fd = -1;
static Player waiting_player;
static Match *pending_match = NULL;   /* match a medio formar para el que espera */

static uint32_t next_match_id = 1;

/* Envia MSG_MATCH_FOUND a un cliente: [match_id(4)][side(1)][len_nick(1)][nick] */
static void send_match_found(int fd, uint32_t match_id, uint8_t side,
                             const char *rival_nick) {
    uint8_t payload[6 + MAX_NICK_LEN + 1];
    uint32_t id_net = htonl(match_id);
    uint8_t nick_len = (uint8_t)strlen(rival_nick);

    memcpy(&payload[0], &id_net, 4);
    payload[4] = side;
    payload[5] = nick_len;
    memcpy(&payload[6], rival_nick, nick_len);

    send_message(fd, MSG_MATCH_FOUND, payload, 6 + nick_len);
}

/* --------------------------------------------------------------------------
 *  matchmaking_join: encola al jugador y forma la pareja cuando hay dos.
 * -------------------------------------------------------------------------- */
int matchmaking_join(int client_fd, const Player *player,
                     Match **out_match, int *out_side) {
    pthread_mutex_lock(&queue_mutex);

    if (waiting_fd < 0) {
        /* --- No hay nadie esperando: este jugador queda en espera (host, izq). --- */
        waiting_fd = client_fd;
        waiting_player = *player;

        /* Creamos el Match ya, para que el segundo jugador lo complete. */
        Match *m = malloc(sizeof(Match));
        if (!m) {
            pthread_mutex_unlock(&queue_mutex);
            return 0;
        }
        memset(m, 0, sizeof(Match));
        m->match_id = next_match_id++;
        m->fd_left = client_fd;
        m->player_left = *player;
        m->active = 1;
        m->refs = 2;   /* dos hilos usaran este Match (izquierda y derecha) */
        pthread_mutex_init(&m->lock, NULL);
        game_init(&m->game);
        pending_match = m;

        pthread_mutex_unlock(&queue_mutex);

        *out_match = m;
        *out_side = SIDE_LEFT;
        log_msg(LOG_INFO, "[fd=%d] '%s' en cola de espera (match %u, lado IZQ)",
                client_fd, player->nickname, m->match_id);
        return 1;

    } else {
        /* --- Ya hay alguien esperando: formamos la pareja (este es der). --- */
        Match *m = pending_match;
        m->fd_right = client_fd;
        m->player_right = *player;
        m->ready = 1;   /* ya estan los dos jugadores: el host puede simular */

        /* Limpiar la sala de espera para futuros jugadores. */
        int rival_fd = waiting_fd;
        Player rival = waiting_player;
        waiting_fd = -1;
        pending_match = NULL;

        pthread_mutex_unlock(&queue_mutex);

        log_msg(LOG_INFO, "[fd=%d] '%s' emparejado con '%s' (match %u)",
                client_fd, player->nickname, rival.nickname, m->match_id);

        /* Avisar a AMBOS que se encontro rival, con el lado y el nick contrario. */
        send_match_found(rival_fd, m->match_id, SIDE_LEFT, player->nickname);
        send_match_found(client_fd, m->match_id, SIDE_RIGHT, rival.nickname);

        *out_match = m;
        *out_side = SIDE_RIGHT;
        return 1;
    }
}

/* Empaqueta el GameState en MSG_STATE (con clamp a rango valido). */
static uint16_t clamp_u16(float v, float max) {
    if (v < 0) v = 0;
    if (v > max) v = max;
    return (uint16_t)v;
}

static int send_state(int fd, const GameState *g) {
    uint8_t payload[10];
    uint16_t bx = htons(clamp_u16(g->ball_x, FIELD_WIDTH));
    uint16_t by = htons(clamp_u16(g->ball_y, FIELD_HEIGHT));
    uint16_t pl = htons(clamp_u16(g->paddle_left, FIELD_HEIGHT));
    uint16_t pr = htons(clamp_u16(g->paddle_right, FIELD_HEIGHT));
    memcpy(&payload[0], &bx, 2);
    memcpy(&payload[2], &by, 2);
    memcpy(&payload[4], &pl, 2);
    memcpy(&payload[6], &pr, 2);
    payload[8] = g->score_left;
    payload[9] = g->score_right;
    return send_message(fd, MSG_STATE, payload, sizeof(payload));
}

/* Lee (sin bloquear) un MSG_INPUT de un socket y aplica el movimiento a la
 * paleta del lado indicado. Devuelve 0 si el cliente se desconecto. */
static int poll_input(Match *m, int fd, int side) {
    Message msg;
    RecvResult res = recv_message(fd, &msg);
    if (res == MSG_OK) {
        if (msg.type == MSG_INPUT && msg.length >= 1) {
            pthread_mutex_lock(&m->lock);
            game_move_paddle(&m->game, side, msg.payload[0]);
            pthread_mutex_unlock(&m->lock);
        } else if (msg.type == MSG_DISCONNECT) {
            return 0;
        }
    } else if (res == MSG_CLOSED) {
        return 0;
    } else if (res == MSG_ERR_IO) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) return 0;
    }
    return 1;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* --------------------------------------------------------------------------
 *  match_run: ejecuta la partida para el hilo de este jugador.
 * -------------------------------------------------------------------------- */
void match_run(Match *m, int side) {
    int my_fd = (side == SIDE_LEFT) ? m->fd_left : m->fd_right;

    if (side == SIDE_LEFT) {
        /* -------- HOST: primero ESPERA a que el rival se una -------- */
        /* Sin esto, el host empezaria a enviar estados a fd_right cuando aun
         * no hay rival (fd invalido) y la partida terminaria al instante. */
        int waited_ms = 0;
        while (m->active && !m->ready) {
            usleep(10000);          /* 10 ms */
            waited_ms += 10;
            if (waited_ms > 120000) {  /* 2 minutos sin rival: abandonar */
                log_msg(LOG_INFO, "Match %u: nadie llego, se cancela", m->match_id);
                m->active = 0;
                return;
            }
        }

        /* Ya hay rival: avisar el inicio de la partida y pasar a no bloqueante. */
        send_message(my_fd, MSG_GAME_START, NULL, 0);
        set_nonblocking(my_fd);

        /* -------- HOST: simula la fisica y difunde el estado -------- */
        while (m->active && !m->game.game_over) {
            /* Leer mi propio input (paleta izquierda). */
            if (!poll_input(m, m->fd_left, SIDE_LEFT)) {
                m->active = 0;
                break;
            }

            /* Avanzar la fisica (bajo el lock, por si el otro hilo mueve su paleta). */
            pthread_mutex_lock(&m->lock);
            game_tick(&m->game);
            GameState snapshot = m->game;  /* copia para enviar sin tener el lock */
            pthread_mutex_unlock(&m->lock);

            /* Enviar el estado a AMBOS jugadores. */
            if (send_state(m->fd_left, &snapshot) < 0)  { m->active = 0; break; }
            if (send_state(m->fd_right, &snapshot) < 0) { m->active = 0; break; }

            usleep(1000000 / TICK_RATE);
        }

        /* Fin de partida: avisar a ambos con ganador y score final. */
        GameState *g = &m->game;
        uint8_t payload[3] = { g->winner_side, g->score_left, g->score_right };
        send_message(m->fd_left,  MSG_GAME_OVER, payload, sizeof(payload));
        send_message(m->fd_right, MSG_GAME_OVER, payload, sizeof(payload));
        m->active = 0;
        log_msg(LOG_INFO, "Match %u terminado: score %d-%d",
                m->match_id, g->score_left, g->score_right);

    } else {
        /* -------- INVITADO: avisar inicio y solo leer su input -------- */
        send_message(my_fd, MSG_GAME_START, NULL, 0);
        set_nonblocking(my_fd);
        while (m->active && !m->game.game_over) {
            if (!poll_input(m, m->fd_right, SIDE_RIGHT)) {
                m->active = 0;
                break;
            }
            usleep(1000000 / TICK_RATE);
        }
    }
}

/* --------------------------------------------------------------------------
 *  match_release: cuenta de referencias para liberar el Match sin riesgo.
 * --------------------------------------------------------------------------
 *  Cada hilo llama a esta funcion al terminar. El ultimo en salir (refs llega
 *  a 0) destruye el mutex y libera la memoria. Asi ningun hilo usa un Match
 *  que otro ya liberó (evita use-after-free).
 * -------------------------------------------------------------------------- */
void match_release(Match *m) {
    if (!m) return;

    pthread_mutex_lock(&m->lock);
    int remaining = --m->refs;
    pthread_mutex_unlock(&m->lock);

    if (remaining == 0) {
        pthread_mutex_destroy(&m->lock);
        free(m);
    }
}
