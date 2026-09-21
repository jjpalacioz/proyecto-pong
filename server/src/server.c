/* ============================================================================
 *  server.c  —  Servidor Pong (Fase 4: concurrencia con hilos)
 * ----------------------------------------------------------------------------
 *  En esta fase el servidor:
 *    1. Lee los argumentos:   ./server <PORT> <LogFile>
 *    2. Crea un socket TCP (API de Sockets Berkeley) y hace bind/listen.
 *    3. Entra en un BUCLE INFINITO de accept(): por cada cliente que llega,
 *       crea un HILO (pthread) que lo atiende de forma independiente.
 *    4. El hilo principal vuelve enseguida a accept() para el siguiente
 *       cliente, de modo que N clientes son atendidos EN PARALELO.
 *
 *  Arquitectura: "thread-per-client" (un hilo por cliente). Esto permite
 *  soportar multiples parejas de jugadores de forma concurrente, como exige
 *  el enunciado.
 *
 *  La logica concreta de atender a cada cliente vive en client_handler.c.
 * ========================================================================== */

/* --- Librerias estandar de C --- */
#include <stdio.h>
#include <stdlib.h>     /* exit, atoi, malloc, EXIT_FAILURE */
#include <string.h>     /* memset, strerror */
#include <unistd.h>     /* close */
#include <errno.h>      /* errno */
#include <signal.h>     /* signal, SIGINT -> apagado ordenado */

/* --- Librerias de red (API de Sockets Berkeley) --- */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* --- Hilos --- */
#include <pthread.h>

/* --- Nuestros modulos --- */
#include "logger.h"
#include "protocol.h"
#include "client_handler.h"

/* Cuantas conexiones pueden esperar en cola mientras atendemos (backlog). */
#define BACKLOG 16

/* Socket de escucha global, para poder cerrarlo desde el manejador de senal. */
static int g_server_fd = -1;

/* --------------------------------------------------------------------------
 *  Manejador de la senal SIGINT (Ctrl+C).
 *  Permite apagar el servidor de forma ordenada: cierra el socket de escucha,
 *  registra el evento y termina. Sin esto, Ctrl+C mataria el proceso "en seco".
 * -------------------------------------------------------------------------- */
static void handle_sigint(int sig) {
    (void)sig;  /* no usamos el numero de senal; el (void) evita el warning */
    log_msg(LOG_INFO, "SIGINT recibido: cerrando el servidor...");
    if (g_server_fd >= 0) close(g_server_fd);
    logger_close();
    _exit(0);   /* _exit es seguro dentro de un manejador de senal */
}

int main(int argc, char *argv[]) {
    /* ------------------------------------------------------------------
     *  PASO 0: Validar argumentos:  ./server <PORT> <LogFile>
     * ------------------------------------------------------------------ */
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 5000 pong.log\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    const char *log_path = argv[2];

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "ERROR: puerto invalido '%s' (debe ser 1..65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------------
     *  PASO 1: Inicializar el logger.
     * ------------------------------------------------------------------ */
    if (logger_init(log_path) != 0) {
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "=== PongServer iniciando ===");
    log_msg(LOG_INFO, "Puerto: %d | Archivo de log: %s", port, log_path);

    /* Registrar el manejador de Ctrl+C para un apagado ordenado. */
    signal(SIGINT, handle_sigint);

    /* ------------------------------------------------------------------
     *  PASO 2: Crear el socket TCP.
     * ------------------------------------------------------------------ */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_msg(LOG_ERROR, "Fallo socket(): %s", strerror(errno));
        logger_close();
        return EXIT_FAILURE;
    }
    g_server_fd = server_fd;
    log_msg(LOG_INFO, "Socket creado (fd=%d)", server_fd);

    /* SO_REUSEADDR: reutilizar el puerto al reiniciar rapido. */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_msg(LOG_WARN, "No se pudo activar SO_REUSEADDR: %s", strerror(errno));
    }

    /* ------------------------------------------------------------------
     *  PASO 3: bind() -> asociar el socket al puerto.
     * ------------------------------------------------------------------ */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_msg(LOG_ERROR, "Fallo bind() en puerto %d: %s", port, strerror(errno));
        close(server_fd);
        logger_close();
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "bind() OK en puerto %d", port);

    /* ------------------------------------------------------------------
     *  PASO 4: listen() -> modo escucha.
     * ------------------------------------------------------------------ */
    if (listen(server_fd, BACKLOG) < 0) {
        log_msg(LOG_ERROR, "Fallo listen(): %s", strerror(errno));
        close(server_fd);
        logger_close();
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "Servidor escuchando. Esperando clientes (modo concurrente)...");

    /* ------------------------------------------------------------------
     *  PASO 5: BUCLE PRINCIPAL. Aceptar clientes y lanzar un hilo por cada uno.
     * ------------------------------------------------------------------ */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        /* accept() bloquea hasta que llega un cliente. */
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;  /* interrumpido por senal: reintentar */
            log_msg(LOG_ERROR, "Fallo accept(): %s", strerror(errno));
            continue;  /* un accept fallido no debe tumbar el servidor entero */
        }

        /* Reservar (con malloc) el contexto que recibira el hilo. Cada hilo
         * necesita SU PROPIA copia; no podemos pasar variables de la pila del
         * bucle porque cambian en la siguiente iteracion. El hilo hace free(). */
        ClientContext *ctx = malloc(sizeof(ClientContext));
        if (ctx == NULL) {
            log_msg(LOG_ERROR, "Sin memoria para atender al cliente");
            close(client_fd);
            continue;
        }
        ctx->client_fd   = client_fd;
        ctx->client_addr = client_addr;

        /* Crear el hilo que atendera a este cliente. */
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
            log_msg(LOG_ERROR, "No se pudo crear el hilo del cliente: %s", strerror(errno));
            close(client_fd);
            free(ctx);
            continue;
        }

        /* "detach": el hilo libera sus recursos SOLO al terminar. Asi el hilo
         * principal no tiene que esperarlo (pthread_join) y puede volver ya
         * mismo a aceptar el siguiente cliente. */
        pthread_detach(tid);
    }

    /* (Este punto no se alcanza en operacion normal; el apagado es via SIGINT.) */
    close(server_fd);
    logger_close();
    return EXIT_SUCCESS;
}
