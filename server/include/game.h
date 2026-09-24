/* ============================================================================
 *  game.h  —  Estado y fisica del juego Pong (lado servidor)
 * ----------------------------------------------------------------------------
 *  El servidor es la UNICA fuente de verdad del juego. Este modulo mantiene el
 *  estado (pelota, paletas, score) y lo hace avanzar en el tiempo ("tick").
 *  El cliente solo dibuja lo que este estado produce; nunca calcula fisica.
 *
 *  Sistema de coordenadas (en unidades del servidor, ver protocol.h):
 *    - Campo de FIELD_WIDTH x FIELD_HEIGHT (800 x 600).
 *    - x crece hacia la derecha, y crece hacia abajo.
 *    - La paleta izquierda esta en x pequeno; la derecha en x grande.
 *    - 'paddle_left'/'paddle_right' son la coordenada Y del BORDE SUPERIOR de
 *      cada paleta (la paleta ocupa de y a y+PADDLE_HEIGHT).
 * ========================================================================== */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* Estado completo de una partida. */
typedef struct {
    /* Pelota: posicion y velocidad. Usamos float internamente para que el
     * movimiento sea suave; al enviar al cliente se redondea a entero. */
    float ball_x;
    float ball_y;
    float ball_vx;   /* velocidad en x (unidades por tick) */
    float ball_vy;   /* velocidad en y */

    /* Paletas: coordenada Y del borde superior de cada una. */
    float paddle_left;
    float paddle_right;

    /* Puntaje de cada jugador. */
    uint8_t score_left;
    uint8_t score_right;

    /* Bandera: 1 si la partida termino (alguien llego a WINNING_SCORE). */
    int game_over;
    uint8_t winner_side;  /* SIDE_LEFT o SIDE_RIGHT cuando game_over = 1 */
} GameState;

/* Inicializa una partida: pelota al centro, paletas centradas, score 0-0. */
void game_init(GameState *g);

/* Lanza la pelota desde el centro hacia un lado (se llama al inicio y tras
 * cada punto). 'to_right' = 1 lanza hacia la derecha, 0 hacia la izquierda. */
void game_serve(GameState *g, int to_right);

/* Mueve la paleta de un lado segun una direccion de input (DIR_UP/DOWN/NONE).
 * side = SIDE_LEFT o SIDE_RIGHT. Respeta los limites del campo. */
void game_move_paddle(GameState *g, int side, int direction);

/* Avanza la simulacion UN tick: mueve la pelota, aplica rebotes contra las
 * paredes superior/inferior, detecta colision con las paletas, y detecta si
 * alguien anoto un punto (la pelota salio por un lado). Actualiza el score y,
 * si corresponde, marca game_over. */
void game_tick(GameState *g);

#endif /* GAME_H */
