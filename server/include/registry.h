/* ============================================================================
 *  registry.h  —  Registro global de clientes (thread-safe)
 * ----------------------------------------------------------------------------
 *  Lleva la cuenta de cuántos clientes están registrados y qué nicknames están
 *  en uso, para poder cumplir dos casos límite de la Fase 8:
 *    - P02: rechazar un nickname repetido (ERR_NICK_TAKEN)
 *    - P07: rechazar al cliente que excede el máximo (ERR_SERVER_FULL)
 *
 *  Todo el estado es compartido por los hilos de todos los clientes, así que
 *  se protege con un mutex.
 * ========================================================================== */
#ifndef REGISTRY_H
#define REGISTRY_H

/* Máximo de clientes registrados a la vez (contrato P07: 64). */
#define MAX_CLIENTS 64

/* Intenta registrar un nickname.
 * Devuelve:
 *    0  -> registrado con éxito (queda ocupado el cupo y el nick)
 *   -1  -> servidor lleno (ERR_SERVER_FULL)
 *   -2  -> nickname ya en uso (ERR_NICK_TAKEN)
 * El nickname se copia internamente; el llamador conserva el suyo. */
int registry_add(const char *nickname);

/* Libera un nickname y su cupo cuando el cliente se va. */
void registry_remove(const char *nickname);

#endif /* REGISTRY_H */
