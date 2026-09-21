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

#include "logger.h"
#include "protocol.h"
#include "protocol_io.h"
#include "player.h"

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
        /* En fases siguientes: aqui iria a matchmaking y al bucle de juego. */
    }

    /* --- Limpieza: cerrar socket y liberar el contexto --- */
    close(client_fd);
    free(ctx);  /* liberamos la memoria que reservo el hilo principal con malloc */
    log_msg(LOG_INFO, "[fd=%d] Hilo de cliente finalizado", client_fd);

    return NULL;
}
