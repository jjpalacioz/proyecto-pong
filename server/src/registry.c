/* ============================================================================
 *  registry.c  —  Implementación del registro global de clientes
 * ----------------------------------------------------------------------------
 *  Guarda una tabla simple de nicknames activos. Como varios hilos la tocan a
 *  la vez (cada cliente en su hilo), TODA operación va bajo un mutex.
 * ========================================================================== */
#include "registry.h"
#include "player.h"   /* MAX_NICK_LEN */

#include <string.h>
#include <pthread.h>

/* Tabla de nicknames activos. Una entrada vacía es una cadena de longitud 0. */
static char active_nicks[MAX_CLIENTS][MAX_NICK_LEN + 1];
static int  client_count = 0;
static pthread_mutex_t reg_mutex = PTHREAD_MUTEX_INITIALIZER;

int registry_add(const char *nickname) {
    pthread_mutex_lock(&reg_mutex);

    /* P07: ¿servidor lleno? */
    if (client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&reg_mutex);
        return -1;  /* ERR_SERVER_FULL */
    }

    /* P02: ¿nickname ya en uso? Recorremos la tabla buscando uno igual. */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_nicks[i][0] != '\0' && strcmp(active_nicks[i], nickname) == 0) {
            pthread_mutex_unlock(&reg_mutex);
            return -2;  /* ERR_NICK_TAKEN */
        }
    }

    /* Buscar un hueco libre y guardar el nickname. */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_nicks[i][0] == '\0') {
            strncpy(active_nicks[i], nickname, MAX_NICK_LEN);
            active_nicks[i][MAX_NICK_LEN] = '\0';
            client_count++;
            break;
        }
    }

    pthread_mutex_unlock(&reg_mutex);
    return 0;  /* OK */
}

void registry_remove(const char *nickname) {
    pthread_mutex_lock(&reg_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_nicks[i][0] != '\0' && strcmp(active_nicks[i], nickname) == 0) {
            active_nicks[i][0] = '\0';  /* liberar la entrada */
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&reg_mutex);
}
