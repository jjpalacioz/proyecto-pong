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
#include <netinet/tcp.h> /* TCP_NODELAY: no demorar paquetes chicos */

#include "logger.h"
#include "protocol.h"
#include "protocol_io.h"
#include "player.h"
#include "game.h"       /* GameState y fisica del juego */
#include "match.h"      /* matchmaking 1vs1 */
#include "registry.h"   /* registro global: nicks en uso y cupo (P02, P07) */

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
 *  is_known_type: dice si un TYPE pertenece al vocabulario del protocolo.
 *  Sirve para P01: distinguir un tipo DESCONOCIDO (ERR_UNKNOWN_TYPE) de uno
 *  conocido pero usado fuera de lugar (ERR_NOT_REGISTERED).
 * -------------------------------------------------------------------------- */
static int is_known_type(uint8_t type) {
    switch (type) {
        case MSG_REGISTER: case MSG_REGISTER_OK: case MSG_REGISTER_ERR:
        case MSG_QUEUE:    case MSG_MATCH_FOUND:
        case MSG_INPUT:    case MSG_STATE: case MSG_GAME_START: case MSG_GAME_OVER:
        case MSG_PING:     case MSG_PONG:
        case MSG_ERROR:    case MSG_DISCONNECT:
            return 1;
        default:
            return 0;
    }
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
    remaining -= len_mail;

    /* P04: tras el nick y el email NO puede sobrar ni faltar ni un byte.
     * Si quedan bytes sin consumir, el mensaje esta mal formado. */
    if (remaining != 0) {
        send_error(client_fd, ERR_BAD_LENGTH);
        return 0;
    }

    /* P02/P07: intentar reservar el nickname en el registro global.
     * Puede fallar si el servidor esta lleno o el nick ya esta en uso. */
    int r = registry_add(player->nickname);
    if (r == -1) {
        send_error(client_fd, ERR_SERVER_FULL);
        log_msg(LOG_WARN, "[fd=%d] Registro rechazado: servidor lleno", client_fd);
        return 0;
    } else if (r == -2) {
        send_error(client_fd, ERR_NICK_TAKEN);
        log_msg(LOG_WARN, "[fd=%d] Registro rechazado: nick '%s' en uso",
                client_fd, player->nickname);
        return 0;
    }

    /* --- registro valido: asignar id (thread-safe) y responder --- */
    player->id = assign_player_id();

    uint32_t id_net = htonl(player->id);
    send_message(client_fd, MSG_REGISTER_OK, &id_net, sizeof(id_net));

    log_msg(LOG_INFO, "Registro OK: id=%u nick='%s' email='%s'",
            player->id, player->nickname, player->email);
    return 1;
}

/* --------------------------------------------------------------------------
 *  client_thread: punto de entrada del hilo. Atiende un cliente completo.
 * --------------------------------------------------------------------------
 *  Nota: la partida de practica (1 jugador vs IA) de la Fase 6 se retiro al
 *  entrar el matchmaking real 1vs1 de la Fase 7. La logica de juego y el envio
 *  de estados viven ahora en match.c.
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

    /* Sin esto, TCP junta los MSG_STATE (16 bytes) hasta ~40 ms antes de
     * mandarlos. En un juego se siente como delay. TCP_NODELAY los envia ya. */
    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    /* Timeout de lectura (SO_RCVTIMEO): un cliente que anuncia un LENGTH grande
     * y no manda los bytes no puede ocupar un hilo para siempre. A los 15 s sin
     * datos, recv falla con EAGAIN/EWOULDBLOCK y cerramos esa conexion. Solo se
     * afecta a ese cliente; los demas siguen. (Contrato de la Fase 8.) */
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

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
        } else if (msg.type == MSG_PING) {
            /* P03: PING es legal en cualquier estado -> responder PONG. */
            if (msg.length != 0) {
                send_error(client_fd, ERR_BAD_LENGTH);
            } else {
                send_message(client_fd, MSG_PONG, NULL, 0);
            }
        } else if (msg.type == MSG_QUEUE && msg.length != 0) {
            /* P06: la FORMA del mensaje se valida antes que el estado. Un
             * MSG_QUEUE bien tipado pero con LENGTH != 0 es ERR_BAD_LENGTH. */
            send_error(client_fd, ERR_BAD_LENGTH);
        } else if (!is_known_type(msg.type)) {
            /* P01: TYPE que no existe en el protocolo -> ERR_UNKNOWN_TYPE. */
            log_msg(LOG_WARN, "[fd=%d] Tipo desconocido 0x%02X", client_fd, msg.type);
            send_error(client_fd, ERR_UNKNOWN_TYPE);
        } else {
            /* Tipo conocido pero ilegal sin registrarse (ej. QUEUE, INPUT). */
            log_msg(LOG_WARN, "[fd=%d] Se esperaba MSG_REGISTER pero llego 0x%02X",
                    client_fd, msg.type);
            send_error(client_fd, ERR_NOT_REGISTERED);
        }
    }

    if (registered) {
        log_msg(LOG_INFO, "[fd=%d] Jugador '%s' (id=%u) registrado y listo",
                client_fd, player.nickname, player.id);

        /* El cliente ya registrado puede: pedir jugar (MSG_QUEUE), hacer
         * ping (MSG_PING) o desconectar. Manejamos cada caso y validamos la
         * forma de los mensajes antes que el estado (P05, P06). */
        int keep = 1;
        while (keep) {
            Message msg;
            RecvResult res = recv_message(client_fd, &msg);

            if (res == MSG_CLOSED) {
                log_msg(LOG_INFO, "[fd=%d] Cliente desconectado tras registrarse", client_fd);
                break;
            } else if (res == MSG_ERR_MAGIC) {
                send_error(client_fd, ERR_BAD_MAGIC); break;
            } else if (res == MSG_ERR_VERSION) {
                send_error(client_fd, ERR_BAD_VERSION); break;
            } else if (res != MSG_OK) {
                break;  /* error de E/S */
            }

            if (msg.type == MSG_PING) {
                /* P03: ping/pong (keepalive). */
                if (msg.length != 0) send_error(client_fd, ERR_BAD_LENGTH);
                else                 send_message(client_fd, MSG_PONG, NULL, 0);

            } else if (msg.type == MSG_DISCONNECT) {
                log_msg(LOG_INFO, "[fd=%d] Cliente pidio desconectar", client_fd);
                break;

            } else if (msg.type == MSG_QUEUE) {
                /* P06: MSG_QUEUE debe tener LENGTH = 0. La forma se valida
                 * antes que el estado. */
                if (msg.length != 0) {
                    send_error(client_fd, ERR_BAD_LENGTH);
                    continue;
                }
                /* Matchmaking real 1vs1: queda en cola hasta que llegue rival. */
                Match *m = NULL;
                int side = SIDE_LEFT;
                if (matchmaking_join(client_fd, &player, &m, &side)) {
                    match_run(m, side);
                    match_release(m);
                }
                keep = 0;  /* tras la partida, este cliente termina */

            } else if (msg.type == MSG_INPUT) {
                /* P05: INPUT fuera de una partida -> avisar con MSG_ERROR
                 * (antes se cerraba el socket en silencio). */
                send_error(client_fd, ERR_NOT_REGISTERED);

            } else if (!is_known_type(msg.type)) {
                send_error(client_fd, ERR_UNKNOWN_TYPE);  /* P01 */

            } else {
                send_error(client_fd, ERR_NOT_REGISTERED);
            }
        }

        /* Al salir, liberar el nickname del registro global. */
        registry_remove(player.nickname);
    }

    /* --- Limpieza: cerrar socket y liberar el contexto --- */
    close(client_fd);
    free(ctx);  /* liberamos la memoria que reservo el hilo principal con malloc */
    log_msg(LOG_INFO, "[fd=%d] Hilo de cliente finalizado", client_fd);

    return NULL;
}
