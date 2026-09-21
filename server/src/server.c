/* ============================================================================
 *  server.c  —  Servidor Pong (Fase 3: registro de clientes)
 * ----------------------------------------------------------------------------
 *  En esta fase el servidor:
 *    1. Lee los argumentos:   ./server <PORT> <LogFile>
 *    2. Crea un socket TCP usando la API de Sockets Berkeley (SIN librerias
 *       de alto nivel, tal como exige el proyecto).
 *    3. Hace bind() al puerto, listen() y accept() de UN cliente.
 *    4. Registra todo en el log (consola + archivo).
 *    5. Procesa el REGISTRO del cliente segun MyAppGameProtocol:
 *         - lee un mensaje MSG_REGISTER (header + payload)
 *         - parsea nickname y email del payload
 *         - guarda el perfil y responde MSG_REGISTER_OK (o un error)
 *
 *  Todavia NO hay concurrencia (varios clientes) ni logica de juego: eso llega
 *  en fases posteriores. Aqui el protocolo binario empieza a cobrar vida.
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
#include "protocol_io.h"  /* recv_message, send_message, send_error */
#include "player.h"       /* struct Player */

/* Cuantas conexiones pueden esperar en cola mientras atendemos una (backlog). */
#define BACKLOG 10

/* Contador global para asignar IDs unicos a los jugadores.
 * (En la Fase 4, con hilos, esto se protegera con un mutex.) */
static uint32_t next_player_id = 1;

/* --------------------------------------------------------------------------
 *  handle_register: procesa un mensaje MSG_REGISTER ya recibido.
 * --------------------------------------------------------------------------
 *  El payload de MSG_REGISTER es (ver docs/PROTOCOL.md):
 *      [1 byte len_nick][len_nick bytes nickname]
 *      [1 byte len_mail][len_mail bytes email]
 *
 *  Esta funcion PARSEA ese payload con mucho cuidado (el profe manda cosas
 *  raras), llena el struct Player y responde al cliente.
 *
 *  Devuelve 1 si el registro fue exitoso, 0 si fue rechazado.
 * -------------------------------------------------------------------------- */
static int handle_register(int client_fd, const Message *msg, Player *player) {
    const uint8_t *p = msg->payload;   /* puntero para recorrer el payload */
    uint16_t remaining = msg->length;  /* bytes que quedan por leer */

    /* --- Leer el nickname (1 byte de longitud + los caracteres) --- */
    if (remaining < 1) {  /* ni siquiera cabe el byte de longitud */
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    uint8_t len_nick = *p;   /* primer byte = longitud del nickname */
    p++;
    remaining--;

    /* Validar: el nickname no puede estar vacio, exceder el limite, ni
     * declarar mas bytes de los que realmente llegaron. */
    if (len_nick == 0 || len_nick > MAX_NICK_LEN || len_nick > remaining) {
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    memcpy(player->nickname, p, len_nick);
    player->nickname[len_nick] = '\0';  /* cerrar la cadena estilo C */
    p += len_nick;
    remaining -= len_nick;

    /* --- Leer el email (1 byte de longitud + los caracteres) --- */
    if (remaining < 1) {
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    uint8_t len_mail = *p;
    p++;
    remaining--;

    if (len_mail == 0 || len_mail > MAX_EMAIL_LEN || len_mail > remaining) {
        send_error(client_fd, ERR_INVALID_FIELD);
        return 0;
    }
    memcpy(player->email, p, len_mail);
    player->email[len_mail] = '\0';

    /* --- Registro valido: asignar ID y responder MSG_REGISTER_OK --- */
    player->id = next_player_id++;

    /* El payload de MSG_REGISTER_OK es el player_id (4 bytes, orden de red). */
    uint32_t id_net = htonl(player->id);
    send_message(client_fd, MSG_REGISTER_OK, &id_net, sizeof(id_net));

    log_msg(LOG_INFO, "Registro OK: id=%u nick='%s' email='%s'",
            player->id, player->nickname, player->email);
    return 1;
}

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
     *  PASO 6: Procesar el REGISTRO del cliente segun el protocolo.
     *  Leemos mensajes completos con recv_message() y esperamos un
     *  MSG_REGISTER. Cualquier otra cosa se responde con un error.
     * ------------------------------------------------------------------ */
    Player player;           /* aqui se guardara el perfil del cliente */
    memset(&player, 0, sizeof(player));
    int registered = 0;      /* bandera: ya se registro? */

    while (!registered) {
        Message msg;
        RecvResult res = recv_message(client_fd, &msg);

        /* --- Manejo de todas las situaciones posibles de lectura --- */
        if (res == MSG_CLOSED) {
            log_msg(LOG_INFO, "Cliente desconectado antes de registrarse");
            break;
        } else if (res == MSG_ERR_IO) {
            log_msg(LOG_ERROR, "Error de E/S leyendo del cliente: %s", strerror(errno));
            break;
        } else if (res == MSG_ERR_MAGIC) {
            log_msg(LOG_WARN, "Mensaje con MAGIC invalido -> se rechaza");
            send_error(client_fd, ERR_BAD_MAGIC);
            break;  /* magic malo = flujo desincronizado: cerramos */
        } else if (res == MSG_ERR_VERSION) {
            log_msg(LOG_WARN, "Version de protocolo no soportada");
            send_error(client_fd, ERR_BAD_VERSION);
            break;
        } else if (res == MSG_ERR_LENGTH) {
            log_msg(LOG_WARN, "LENGTH invalido en el header");
            send_error(client_fd, ERR_BAD_LENGTH);
            break;
        }

        /* --- Mensaje bien formado: revisar que sea el que esperamos --- */
        log_msg(LOG_INFO, "Mensaje recibido: type=0x%02X length=%u", msg.type, msg.length);

        if (msg.type == MSG_REGISTER) {
            registered = handle_register(client_fd, &msg, &player);
            /* Si el registro falla (registered=0), el bucle pide otro intento. */
        } else {
            /* El cliente mando algo distinto a REGISTER estando sin registrar. */
            log_msg(LOG_WARN, "Se esperaba MSG_REGISTER pero llego type=0x%02X", msg.type);
            send_error(client_fd, ERR_NOT_REGISTERED);
        }
    }

    if (registered) {
        log_msg(LOG_INFO, "Jugador '%s' (id=%u) registrado y listo",
                player.nickname, player.id);
        /* En fases siguientes: aqui continuaria hacia matchmaking y juego. */
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
