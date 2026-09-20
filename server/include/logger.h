/* ============================================================================
 *  logger.h  —  Sistema de registro de eventos (logging)
 * ----------------------------------------------------------------------------
 *  El profe EXIGE un "logger" que:
 *    1. Imprima por la terminal (consola) todo lo que pasa.
 *    2. Escriba lo mismo en un archivo de log (el <LogFile> del comando).
 *
 *  Este modulo ofrece una API simple para registrar mensajes con distintos
 *  niveles de severidad (INFO, WARN, ERROR, DEBUG).
 * ========================================================================== */
#ifndef LOGGER_H
#define LOGGER_H

/* Niveles de severidad de un mensaje de log.
 * Sirven para clasificar: informacion normal, advertencia, error, o depuracion. */
typedef enum {
    LOG_INFO,   /* eventos normales: cliente conectado, mensaje recibido, etc. */
    LOG_WARN,   /* algo inusual pero no fatal: mensaje raro, reintento, etc. */
    LOG_ERROR,  /* errores: fallo al leer socket, cliente invalido, etc. */
    LOG_DEBUG   /* detalle tecnico util para depurar */
} LogLevel;

/* Inicializa el logger abriendo el archivo indicado.
 * - path: ruta del archivo de log (el <LogFile> del comando).
 * Devuelve 0 si todo bien, -1 si no pudo abrir el archivo. */
int  logger_init(const char *path);

/* Escribe un mensaje de log. Funciona como printf (formato + argumentos).
 * El mensaje sale POR CONSOLA y AL ARCHIVO al mismo tiempo, con timestamp y nivel.
 * Ejemplo: log_msg(LOG_INFO, "Cliente conectado desde %s", ip); */
void log_msg(LogLevel level, const char *fmt, ...);

/* Cierra el archivo de log de forma ordenada. Se llama al terminar el servidor. */
void logger_close(void);

#endif /* LOGGER_H */
