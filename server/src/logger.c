/* ============================================================================
 *  logger.c  —  Implementacion del sistema de logging
 * ----------------------------------------------------------------------------
 *  Cada mensaje de log se escribe con este formato:
 *      [2026-09-16 14:30:05] [INFO] Cliente conectado desde 127.0.0.1
 *  ...y sale por DOS destinos a la vez: la consola (stdout) y el archivo de log.
 * ========================================================================== */
#include "logger.h"

#include <stdio.h>    /* fprintf, vfprintf, fopen, fclose, stdout */
#include <stdlib.h>   /* NULL */
#include <stdarg.h>   /* va_list, va_start, va_end -> argumentos variables (...) */
#include <time.h>     /* time, localtime, strftime -> timestamp */
#include <pthread.h>  /* mutex -> proteger el log cuando haya varios hilos */

/* Archivo de log abierto. 'static' = solo visible dentro de este archivo. */
static FILE *log_file = NULL;

/* Un "mutex" (candado). En la Fase 4 el servidor tendra varios hilos que
 * podrian intentar escribir en el log AL MISMO TIEMPO, mezclando las lineas.
 * El mutex garantiza que solo un hilo escriba a la vez. Lo dejamos listo desde ya. */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Convierte el nivel (enum) a un texto legible para imprimir. */
static const char *level_str(LogLevel level) {
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_DEBUG: return "DEBUG";
        default:        return "?????";
    }
}

/* Abre el archivo de log en modo "append" (a): si ya existe, escribe al final;
 * si no existe, lo crea. Devuelve 0 si ok, -1 si fallo. */
int logger_init(const char *path) {
    log_file = fopen(path, "a");
    if (log_file == NULL) {
        /* Si no se pudo abrir, avisamos por consola y devolvemos error. */
        fprintf(stderr, "ERROR: no se pudo abrir el archivo de log '%s'\n", path);
        return -1;
    }
    return 0;
}

/* Escribe un mensaje de log en consola y archivo, con timestamp y nivel. */
void log_msg(LogLevel level, const char *fmt, ...) {
    /* --- 1. Construir el timestamp "YYYY-MM-DD HH:MM:SS" --- */
    char timestamp[32];
    time_t now = time(NULL);              /* segundos desde 1970 */
    struct tm *tm_info = localtime(&now); /* descompone en fecha/hora local */
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    /* --- 2. Tomar el candado (nadie mas escribe mientras tanto) --- */
    pthread_mutex_lock(&log_mutex);

    /* --- 3. Escribir el prefijo "[fecha] [NIVEL] " en consola y archivo --- */
    fprintf(stdout, "[%s] [%s] ", timestamp, level_str(level));
    if (log_file) fprintf(log_file, "[%s] [%s] ", timestamp, level_str(level));

    /* --- 4. Escribir el mensaje del usuario (formato tipo printf) ---
     * 'va_list' recoge los argumentos "..." . Hay que recorrerlos dos veces
     * (una para consola, otra para archivo), asi que usamos va_start dos veces. */
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);   /* al consola */
    va_end(args);

    if (log_file) {
        va_start(args, fmt);
        vfprintf(log_file, fmt, args);  /* al archivo */
        va_end(args);
    }

    /* --- 5. Salto de linea y "flush" (forzar que se escriba ya, no en cache) --- */
    fprintf(stdout, "\n");
    fflush(stdout);
    if (log_file) {
        fprintf(log_file, "\n");
        fflush(log_file);  /* importante: si el server crashea, el log ya quedo escrito */
    }

    /* --- 6. Soltar el candado --- */
    pthread_mutex_unlock(&log_mutex);
}

/* Cierra el archivo de log si esta abierto. */
void logger_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
