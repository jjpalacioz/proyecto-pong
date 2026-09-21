/* ============================================================================
 *  protocol_io.h  —  Lectura y escritura de mensajes MyAppGameProtocol
 * ----------------------------------------------------------------------------
 *  Este modulo implementa el "framing" descrito en docs/PROTOCOL.md:
 *    - Un mensaje = HEADER (6 bytes) + PAYLOAD (LENGTH bytes).
 *    - recv_message() lee un mensaje COMPLETO y valida el header.
 *    - send_message() construye y envia un mensaje bien formado.
 *
 *  Asi, el resto del servidor trabaja con "mensajes" (tipo + payload) sin
 *  preocuparse por bytes sueltos ni por el flujo de TCP.
 * ========================================================================== */
#ifndef PROTOCOL_IO_H
#define PROTOCOL_IO_H

#include <stdint.h>
#include "protocol.h"

/* Estructura en memoria de un mensaje ya "desempaquetado".
 * El header se descompone en sus campos y el payload queda en un buffer. */
typedef struct {
    uint8_t  type;                 /* tipo de mensaje (MSG_REGISTER, etc.) */
    uint16_t length;               /* longitud del payload */
    uint8_t  payload[MAX_PAYLOAD]; /* contenido del mensaje */
} Message;

/* Codigos de retorno de recv_message (para distinguir cada situacion). */
typedef enum {
    MSG_OK          =  1,  /* mensaje leido y validado correctamente */
    MSG_CLOSED      =  0,  /* el cliente cerro la conexion */
    MSG_ERR_IO      = -1,  /* error de lectura del socket */
    MSG_ERR_MAGIC   = -2,  /* el numero magico no coincide */
    MSG_ERR_VERSION = -3,  /* version de protocolo no soportada */
    MSG_ERR_LENGTH  = -4   /* longitud del payload fuera de rango */
} RecvResult;

/* Lee un mensaje completo desde el socket 'fd' y lo guarda en 'msg'.
 * Hace: leer header (6 bytes) -> validar MAGIC/VERSION/LENGTH -> leer payload.
 * Devuelve un valor de RecvResult. */
RecvResult recv_message(int fd, Message *msg);

/* Construye y envia un mensaje con el 'type', 'payload' y 'length' dados.
 * Se encarga de armar el header (MAGIC, VERSION, TYPE, LENGTH) en orden de red.
 * Devuelve 1 si se envio bien, -1 si hubo error. */
int send_message(int fd, uint8_t type, const void *payload, uint16_t length);

/* Atajo para enviar un mensaje de error (MSG_ERROR) con un codigo. */
int send_error(int fd, uint8_t error_code);

#endif /* PROTOCOL_IO_H */
