/* ============================================================================
 *  net.c  —  Implementacion de recv_all / send_all
 * ----------------------------------------------------------------------------
 *  Estas dos funciones son la base de TODA la comunicacion del servidor.
 *  Garantizan que se mueven EXACTAMENTE los bytes pedidos, resolviendo el
 *  comportamiento de "lecturas/escrituras parciales" de TCP.
 * ========================================================================== */
#include "net.h"

#include <sys/socket.h>  /* recv, send */
#include <errno.h>       /* errno, EINTR */

/* Lee exactamente 'len' bytes. Ver contrato en net.h. */
int recv_all(int fd, void *buf, size_t len) {
    /* Tratamos el buffer como bytes para poder avanzar el puntero. */
    char *p = (char *)buf;
    size_t total = 0;  /* cuantos bytes llevamos leidos */

    while (total < len) {
        /* Pedimos los bytes que AUN faltan: (len - total). */
        ssize_t n = recv(fd, p + total, len - total, 0);

        if (n < 0) {
            /* EINTR = la llamada fue interrumpida por una senal; reintentamos. */
            if (errno == EINTR) continue;
            return -1;  /* error real de lectura */
        }
        if (n == 0) {
            /* recv devuelve 0 = el otro extremo cerro la conexion. */
            return 0;
        }
        /* Avanzamos el contador con lo que si se leyo (puede ser parcial). */
        total += (size_t)n;
    }
    return 1;  /* leimos los 'len' bytes completos */
}

/* Envia exactamente 'len' bytes. Ver contrato en net.h. */
int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;  /* cuantos bytes llevamos enviados */

    while (total < len) {
        /* MSG_NOSIGNAL evita que el programa reciba la senal SIGPIPE (que lo
         * mataria) si el cliente cerro el socket; en su lugar send() devuelve
         * error y nosotros lo manejamos ordenadamente. */
        ssize_t n = send(fd, p + total, len - total, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EINTR) continue;  /* interrumpido: reintentar */
            return -1;                     /* error real de envio */
        }
        total += (size_t)n;
    }
    return 1;  /* enviamos los 'len' bytes completos */
}
