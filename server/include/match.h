/* ============================================================================
 *  match.h  —  Emparejamiento (matchmaking) y partida compartida 1vs1
 * ----------------------------------------------------------------------------
 *  Dos jugadores reales comparten UNA partida. Como cada jugador es atendido
 *  por su propio hilo, ambos hilos comparten el mismo GameState, protegido por
 *  un mutex para evitar condiciones de carrera.
 *
 *  Reparto de responsabilidades entre los dos hilos de una partida:
 *    - El jugador SIDE_LEFT es el "host": su hilo corre la simulacion
 *      (game_tick) y envia MSG_STATE a AMBOS jugadores.
 *    - El jugador SIDE_RIGHT solo lee su input y lo aplica al estado compartido.
 *  Asi la fisica la calcula un solo hilo (consistencia) pero ambos ven lo mismo.
 * ========================================================================== */
#ifndef MATCH_H
#define MATCH_H

#include <pthread.h>
#include "game.h"
#include "player.h"

/* Estado de una partida compartida por dos jugadores. */
typedef struct {
    uint32_t match_id;

    int fd_left;    /* socket del jugador izquierdo (host) */
    int fd_right;   /* socket del jugador derecho */

    Player player_left;
    Player player_right;

    GameState game;         /* estado del juego COMPARTIDO */
    pthread_mutex_t lock;   /* protege 'game' y 'refs' entre los dos hilos */

    int active;             /* 1 mientras la partida sigue viva */
    int refs;               /* cuantos hilos siguen usando este Match */
    int ready;              /* 1 cuando los DOS jugadores ya estan presentes */
} Match;

/* Libera el Match de forma segura: descuenta una referencia y solo cuando el
 * ULTIMO hilo sale, destruye el mutex y libera la memoria. Evita use-after-free
 * cuando los dos hilos terminan en momentos distintos. */
void match_release(Match *m);

/* Encola a un jugador buscando rival. Bloquea (de forma cooperativa) hasta que
 * haya un rival disponible y se forme una partida.
 *
 * Parametros:
 *   client_fd : socket del jugador que busca partida
 *   player    : perfil del jugador (ya registrado)
 *   out_match : (salida) puntero al Match formado
 *   out_side  : (salida) lado asignado a este jugador (SIDE_LEFT/RIGHT)
 *
 * Devuelve 1 si se formo la partida, 0 si hubo error/desconexion. */
int matchmaking_join(int client_fd, const Player *player,
                     Match **out_match, int *out_side);

/* Corre la partida para el hilo de este jugador (segun su 'side').
 * El host (SIDE_LEFT) simula y difunde; el otro solo aplica su input.
 * Se encarga de enviar MSG_GAME_START al inicio y MSG_GAME_OVER al final. */
void match_run(Match *m, int side);

#endif /* MATCH_H */
