/* ============================================================================
 *  client_handler.h  —  Atencion de un cliente en su propio hilo
 * ----------------------------------------------------------------------------
 *  Cada cliente que se conecta es atendido por un HILO independiente. Este
 *  modulo define:
 *    - La estructura de datos que se le pasa a cada hilo (ClientContext).
 *    - La funcion que ejecuta cada hilo (client_thread).
 *
 *  Gracias a esto, el servidor atiende varios clientes de forma concurrente.
 * ========================================================================== */
#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <netinet/in.h>  /* struct sockaddr_in */

/* Informacion que el hilo principal le entrega a cada hilo de cliente.
 * Se reserva con malloc por cada cliente y el hilo la libera al terminar. */
typedef struct {
    int                client_fd;    /* socket para hablar con este cliente */
    struct sockaddr_in client_addr;  /* direccion (IP:puerto) del cliente */
} ClientContext;

/* Funcion que ejecuta cada hilo. La firma (void* -> void*) es la que exige
 * pthread_create. Recibe un ClientContext* (reservado con malloc).
 * Atiende al cliente de principio a fin y libera los recursos al salir. */
void *client_thread(void *arg);

#endif /* CLIENT_HANDLER_H */
