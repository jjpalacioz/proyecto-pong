/* ============================================================================
 *  net.h  —  Utilidades de red robustas sobre TCP
 * ----------------------------------------------------------------------------
 *  PROBLEMA QUE RESUELVE ESTE MODULO:
 *  En TCP, una sola llamada a recv() puede devolver MENOS bytes de los que
 *  pediste (aunque haya mas en camino), y send() puede enviar MENOS bytes de
 *  los que pediste. TCP es un flujo (stream): no respeta "mensajes".
 *
 *  Por eso NUNCA debemos asumir que un solo recv()/send() mueve todo. Estas
 *  funciones envuelven recv()/send() en un bucle que insiste hasta mover
 *  EXACTAMENTE la cantidad de bytes pedida (o detectar cierre/error).
 * ========================================================================== */
#ifndef NET_H
#define NET_H

#include <stddef.h>  /* size_t */

/* Lee EXACTAMENTE 'len' bytes del socket 'fd' hacia 'buf'.
 * Insiste con recv() en bucle hasta completar 'len' bytes.
 * Devuelve:
 *    1  -> se leyeron los 'len' bytes completos (exito)
 *    0  -> el cliente cerro la conexion antes de completar (desconexion)
 *   -1  -> error de lectura */
int recv_all(int fd, void *buf, size_t len);

/* Envia EXACTAMENTE 'len' bytes desde 'buf' por el socket 'fd'.
 * Insiste con send() en bucle hasta enviar 'len' bytes.
 * Devuelve:
 *    1  -> se enviaron los 'len' bytes completos (exito)
 *   -1  -> error de envio (ej: el cliente cerro el socket) */
int send_all(int fd, const void *buf, size_t len);

#endif /* NET_H */
