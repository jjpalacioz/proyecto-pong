/* ============================================================================
 *  protocol.h  —  Definiciones del protocolo MyAppGameProtocol (lado servidor)
 * ----------------------------------------------------------------------------
 *  Este archivo es la traduccion a C del documento common/protocol_spec.md.
 *  Contiene TODAS las constantes del protocolo: numero magico, version,
 *  tipos de mensaje, codigos de error y parametros del juego.
 *
 *  El cliente en Python define estas mismas constantes por su lado. Ambos
 *  DEBEN coincidir byte a byte, o no se entenderan.
 *
 *  "include guard": las dos lineas de abajo (#ifndef / #define) evitan que
 *  este archivo se incluya dos veces por error, lo que causaria errores de
 *  "definicion duplicada" al compilar.
 * ========================================================================== */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>  /* Da los tipos de tamano exacto: uint8_t, uint16_t, uint32_t */

/* ----------------------------------------------------------------------------
 *  Header del protocolo
 * -------------------------------------------------------------------------- */

/* Numero magico: 0x5047 = letras "PG" (PonG). Va al inicio de CADA mensaje.
 * Si un mensaje entrante no empieza con este valor, esta corrupto -> se rechaza. */
#define PROTO_MAGIC    0x5047

/* Version actual del protocolo. Permite evolucionarlo en el futuro. */
#define PROTO_VERSION  0x01

/* Tamano fijo del header en bytes: MAGIC(2) + VERSION(1) + TYPE(1) + LENGTH(2) = 6 */
#define HEADER_SIZE    6

/* Tamano maximo permitido para el payload (cabe en el campo LENGTH de 2 bytes). */
#define MAX_PAYLOAD    65535

/* ----------------------------------------------------------------------------
 *  Tipos de mensaje (campo TYPE del header)
 *  Agrupados por familias segun el rango del numero.
 * -------------------------------------------------------------------------- */

/* --- Registro (0x01 - 0x03) --- */
#define MSG_REGISTER      0x01  /* C->S: nickname + email */
#define MSG_REGISTER_OK   0x02  /* S->C: registro aceptado, entrega player_id */
#define MSG_REGISTER_ERR  0x03  /* S->C: registro rechazado (con codigo de error) */

/* --- Emparejamiento (0x10 - 0x11) --- */
#define MSG_QUEUE         0x10  /* C->S: pedir entrar a la cola de emparejamiento */
#define MSG_MATCH_FOUND   0x11  /* S->C: se encontro rival, datos de la partida */

/* --- Juego (0x20 - 0x23) --- */
#define MSG_INPUT         0x20  /* C->S: direccion de la paleta (arriba/abajo/quieto) */
#define MSG_STATE         0x21  /* S->C: estado del juego (pelota, paletas, score) */
#define MSG_GAME_START    0x22  /* S->C: la partida comienza */
#define MSG_GAME_OVER     0x23  /* S->C: la partida termino (ganador + score final) */

/* --- Keepalive (0x30 - 0x31) --- */
#define MSG_PING          0x30  /* C->S: verificar que la conexion sigue viva */
#define MSG_PONG          0x31  /* S->C: respuesta al ping */

/* --- Control (0x40 - 0x41) --- */
#define MSG_ERROR         0x40  /* S->C: error generico del protocolo */
#define MSG_DISCONNECT    0x41  /* C<->S: aviso de desconexion ordenada */

/* ----------------------------------------------------------------------------
 *  Direcciones de input (payload de MSG_INPUT)
 * -------------------------------------------------------------------------- */
#define DIR_NONE  0  /* paleta quieta */
#define DIR_UP    1  /* mover paleta hacia arriba */
#define DIR_DOWN  2  /* mover paleta hacia abajo */

/* ----------------------------------------------------------------------------
 *  Lados del jugador
 * -------------------------------------------------------------------------- */
#define SIDE_LEFT   0  /* jugador de la izquierda */
#define SIDE_RIGHT  1  /* jugador de la derecha */

/* ----------------------------------------------------------------------------
 *  Codigos de error (payload de MSG_ERROR y MSG_REGISTER_ERR)
 * -------------------------------------------------------------------------- */
#define ERR_BAD_MAGIC       0x01  /* header sin el numero magico correcto */
#define ERR_BAD_VERSION     0x02  /* version de protocolo no soportada */
#define ERR_UNKNOWN_TYPE    0x03  /* tipo de mensaje desconocido */
#define ERR_BAD_LENGTH      0x04  /* LENGTH inconsistente con el payload */
#define ERR_NICK_TAKEN      0x05  /* el nickname ya esta en uso */
#define ERR_INVALID_FIELD   0x06  /* un campo del payload es invalido */
#define ERR_NOT_REGISTERED  0x07  /* el cliente intenta jugar sin registrarse */
#define ERR_SERVER_FULL     0x08  /* el servidor no acepta mas clientes */

/* ----------------------------------------------------------------------------
 *  Parametros del juego (definidos por el servidor, unica fuente de verdad)
 * -------------------------------------------------------------------------- */
#define FIELD_WIDTH    800  /* ancho del campo de juego, en unidades */
#define FIELD_HEIGHT   600  /* alto del campo de juego */
#define PADDLE_HEIGHT  100  /* alto de la paleta */
#define WINNING_SCORE  5    /* puntos necesarios para ganar la partida */
#define TICK_RATE      60   /* actualizaciones del estado por segundo */

#endif /* PROTOCOL_H */
