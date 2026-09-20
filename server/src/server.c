/* ============================================================================
 *  server.c  —  Servidor Pong (esqueleto de la Fase 2)
 * ----------------------------------------------------------------------------
 *  En esta fase el servidor:
 *    1. Lee los argumentos:   ./server <PORT> <LogFile>
 *    2. Crea un socket TCP usando la API de Sockets Berkeley (SIN librerias
 *       de alto nivel, tal como exige el proyecto).
 *    3. Hace bind() al puerto, listen() y accept() de UN cliente.
 *    4. Registra todo en el log (consola + archivo).
 *    5. Lee lo que el cliente envie y lo muestra (eco basico de prueba).
 *
 *  Todavia NO hay concurrencia (varios clientes) ni logica de juego: eso llega
 *  en fases posteriores. Aqui construimos la base solida.
 *
 *  Los 5 pasos clasicos de un servidor TCP Berkeley:
 *      socket()  -> crear el "enchufe" de red
 *      bind()    -> asociarlo a una direccion (IP) y puerto
 *      listen()  -> ponerlo a escuchar conexiones entrantes
 *      accept()  -> aceptar una conexion (bloquea hasta que llega un cliente)
 *      recv()/send() -> intercambiar datos
 *      close()   -> cerrar
 * ========================================================================== */

/* --- Librerias estandar de C --- */
#include <stdio.h>      /* printf, perror */
#include <stdlib.h>     /* exit, atoi, EXIT_FAILURE */
#include <string.h>     /* memset, strlen, strerror */
#include <unistd.h>     /* close, read, write */
#include <errno.h>      /* errno -> codigo del ultimo error del sistema */

/* --- Librerias de red (API de Sockets Berkeley) --- */
#include <sys/types.h>  /* tipos de datos usados por las llamadas de socket */
#include <sys/socket.h> /* socket, bind, listen, accept, send, recv */
#include <netinet/in.h> /* struct sockaddr_in, htons, INADDR_ANY */
#include <arpa/inet.h>  /* inet_ntoa -> convertir IP a texto para el log */

/* --- Nuestros modulos --- */
#include "logger.h"
#include "protocol.h"

/* Cuantas conexiones pueden esperar en cola mientras atendemos una (backlog). */
#define BACKLOG 10

/* Tamano del buffer temporal para leer datos entrantes. */
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    /* ------------------------------------------------------------------
     *  PASO 0: Validar los argumentos de linea de comandos.
     *  El profe exige EXACTAMENTE:  ./server <PORT> <LogFile>
     *  argc = cantidad de argumentos (incluye el nombre del programa).
     *  Deben ser 3: [0]=./server  [1]=<PORT>  [2]=<LogFile>
     * ------------------------------------------------------------------ */
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 5000 pong.log\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);          /* convierte el texto del puerto a numero */
    const char *log_path = argv[2];    /* ruta del archivo de log */

    /* Validar que el puerto este en el rango valido (1..65535). */
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "ERROR: puerto invalido '%s' (debe ser 1..65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------------
     *  PASO 1: Inicializar el logger (abre el archivo de log).
     * ------------------------------------------------------------------ */
    if (logger_init(log_path) != 0) {
        return EXIT_FAILURE;  /* si no se pudo abrir el log, no seguimos */
    }
    log_msg(LOG_INFO, "=== PongServer iniciando ===");
    log_msg(LOG_INFO, "Puerto: %d | Archivo de log: %s", port, log_path);

    /* ------------------------------------------------------------------
     *  PASO 2: Crear el socket.
     *  socket(dominio, tipo, protocolo)
     *    AF_INET      -> IPv4
     *    SOCK_STREAM  -> TCP (flujo de bytes, confiable y ordenado)
     *    0            -> protocolo por defecto para SOCK_STREAM = TCP
     *  Devuelve un "descriptor" (numero entero) que identifica el socket,
     *  o -1 si hubo error.
     * ------------------------------------------------------------------ */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_msg(LOG_ERROR, "Fallo socket(): %s", strerror(errno));
        logger_close();
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "Socket creado (fd=%d)", server_fd);

    /* ------------------------------------------------------------------
     *  PASO 2.5: Permitir reutilizar el puerto (SO_REUSEADDR).
     *  Sin esto, si reinicias el servidor rapido, bind() falla con
     *  "Address already in use" porque el SO deja el puerto ocupado un rato.
     *  Esto nos evita dolores de cabeza al desarrollar/probar.
     * ------------------------------------------------------------------ */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_msg(LOG_WARN, "No se pudo activar SO_REUSEADDR: %s", strerror(errno));
    }

    /* ------------------------------------------------------------------
     *  PASO 3: Preparar la direccion y hacer bind().
     *  struct sockaddr_in describe una direccion IPv4 (IP + puerto).
     * ------------------------------------------------------------------ */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); /* limpiar la estructura a ceros */
    server_addr.sin_family      = AF_INET;        /* IPv4 */
    server_addr.sin_addr.s_addr = INADDR_ANY;     /* aceptar conexiones en cualquier IP local */
    server_addr.sin_port        = htons(port);    /* puerto en network byte order (!) */

    /* bind() asocia el socket con la direccion/puerto. */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_msg(LOG_ERROR, "Fallo bind() en puerto %d: %s", port, strerror(errno));
        close(server_fd);
        logger_close();
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "bind() OK en puerto %d", port);

    /* ------------------------------------------------------------------
     *  PASO 4: listen() -> poner el socket en modo escucha.
     *  BACKLOG = cuantas conexiones pueden esperar en cola.
     * ------------------------------------------------------------------ */
    if (listen(server_fd, BACKLOG) < 0) {
        log_msg(LOG_ERROR, "Fallo listen(): %s", strerror(errno));
        close(server_fd);
        logger_close();
        return EXIT_FAILURE;
    }
    log_msg(LOG_INFO, "Servidor escuchando. Esperando clientes...");

    /* ------------------------------------------------------------------
     *  PASO 5: accept() -> aceptar UN cliente (bloquea hasta que llegue).
     *  Nos devuelve un NUEVO socket (client_fd) para hablar con ese cliente.
     *  El server_fd sigue escuchando; client_fd es la conversacion concreta.
     * ------------------------------------------------------------------ */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        log_msg(LOG_ERROR, "Fallo accept(): %s", strerror(errno));
        close(server_fd);
        logger_close();
        return EXIT_FAILURE;
    }

    /* inet_ntoa convierte la IP del cliente (binaria) a texto legible. */
    log_msg(LOG_INFO, "Cliente conectado desde %s:%d",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* ------------------------------------------------------------------
     *  PASO 6: Leer datos del cliente y hacer eco (prueba de la Fase 2).
     *  recv() lee bytes del socket. Devuelve:
     *    > 0 -> cantidad de bytes leidos
     *    = 0 -> el cliente cerro la conexion ordenadamente
     *    < 0 -> error
     * ------------------------------------------------------------------ */
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        log_msg(LOG_INFO, "Recibidos %zd bytes del cliente", bytes_read);

        /* Eco: devolvemos al cliente lo mismo que envio (solo prueba). */
        if (send(client_fd, buffer, bytes_read, 0) < 0) {
            log_msg(LOG_ERROR, "Fallo send(): %s", strerror(errno));
            break;
        }
    }

    if (bytes_read == 0) {
        log_msg(LOG_INFO, "Cliente desconectado ordenadamente");
    } else if (bytes_read < 0) {
        log_msg(LOG_ERROR, "Fallo recv(): %s", strerror(errno));
    }

    /* ------------------------------------------------------------------
     *  PASO 7: Cerrar todo ordenadamente.
     * ------------------------------------------------------------------ */
    close(client_fd);
    close(server_fd);
    log_msg(LOG_INFO, "=== PongServer finalizado ===");
    logger_close();

    return EXIT_SUCCESS;
}
