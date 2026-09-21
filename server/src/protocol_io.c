/* ============================================================================
 *  protocol_io.c  —  Implementacion del framing del protocolo
 * ----------------------------------------------------------------------------
 *  Aqui se materializa el diseno de docs/PROTOCOL.md:
 *
 *  Header (6 bytes):
 *      offset 0-1 : MAGIC   (uint16, big-endian) = 0x5047
 *      offset 2   : VERSION (uint8)              = 0x01
 *      offset 3   : TYPE    (uint8)
 *      offset 4-5 : LENGTH  (uint16, big-endian) = tamano del payload
 *  Payload: LENGTH bytes.
 * ========================================================================== */
#include "protocol_io.h"
#include "net.h"

#include <string.h>      /* memcpy */
#include <arpa/inet.h>   /* htons, ntohs -> conversion de orden de bytes */

/* --------------------------------------------------------------------------
 *  recv_message: lee un mensaje completo y valida su header.
 * -------------------------------------------------------------------------- */
RecvResult recv_message(int fd, Message *msg) {
    uint8_t header[HEADER_SIZE];  /* buffer para los 6 bytes del header */

    /* --- 1. Leer EXACTAMENTE los 6 bytes del header --- */
    int r = recv_all(fd, header, HEADER_SIZE);
    if (r == 0) return MSG_CLOSED;   /* el cliente cerro */
    if (r < 0) return MSG_ERR_IO;    /* error de lectura */

    /* --- 2. Desempaquetar los campos del header ---
     * Los 2 bytes de MAGIC y LENGTH vienen en big-endian (orden de red).
     * ntohs() los convierte al orden nativo de esta maquina. */
    uint16_t magic;
    memcpy(&magic, &header[0], 2);
    magic = ntohs(magic);

    uint8_t version = header[2];
    uint8_t type    = header[3];

    uint16_t length;
    memcpy(&length, &header[4], 2);
    length = ntohs(length);

    /* --- 3. Validar el header (defensa contra mensajes corruptos) --- */
    if (magic != PROTO_MAGIC)     return MSG_ERR_MAGIC;    /* no es nuestro protocolo */
    if (version != PROTO_VERSION) return MSG_ERR_VERSION;  /* version incompatible */
    /* Nota: 'length' es uint16_t (max 65535 = MAX_PAYLOAD), por lo que SIEMPRE
     * cabe en msg->payload. No hace falta comparar contra MAX_PAYLOAD (el
     * compilador avisaria que la comparacion nunca es verdadera). El tipo del
     * campo ya garantiza el limite. */

    /* --- 4. Leer EXACTAMENTE 'length' bytes de payload (si hay) --- */
    if (length > 0) {
        r = recv_all(fd, msg->payload, length);
        if (r == 0) return MSG_CLOSED;
        if (r < 0) return MSG_ERR_IO;
    }

    /* --- 5. Entregar el mensaje ya desempaquetado --- */
    msg->type   = type;
    msg->length = length;
    return MSG_OK;
}

/* --------------------------------------------------------------------------
 *  send_message: arma el header + payload y lo envia completo.
 * -------------------------------------------------------------------------- */
int send_message(int fd, uint8_t type, const void *payload, uint16_t length) {
    /* Buffer para header + payload en un solo bloque contiguo.
     * Asi lo enviamos de una sola vez (mas eficiente y sin mezclas). */
    uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];

    /* --- Armar el header --- */
    uint16_t magic_n  = htons(PROTO_MAGIC);  /* a orden de red */
    uint16_t length_n = htons(length);

    memcpy(&buf[0], &magic_n, 2);    /* MAGIC   (bytes 0-1) */
    buf[2] = PROTO_VERSION;          /* VERSION (byte 2) */
    buf[3] = type;                   /* TYPE    (byte 3) */
    memcpy(&buf[4], &length_n, 2);   /* LENGTH  (bytes 4-5) */

    /* --- Copiar el payload despues del header (si hay) --- */
    if (length > 0 && payload != NULL) {
        memcpy(&buf[HEADER_SIZE], payload, length);
    }

    /* --- Enviar todo de una vez (header + payload) --- */
    return send_all(fd, buf, HEADER_SIZE + length);
}

/* --------------------------------------------------------------------------
 *  send_error: atajo para enviar MSG_ERROR con un codigo de 1 byte.
 * -------------------------------------------------------------------------- */
int send_error(int fd, uint8_t error_code) {
    return send_message(fd, MSG_ERROR, &error_code, 1);
}
