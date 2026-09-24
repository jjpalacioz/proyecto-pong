/* ============================================================================
 *  game.c  —  Implementacion de la fisica del Pong
 * ----------------------------------------------------------------------------
 *  Toda la logica del juego vive aqui, en el servidor. Cada "tick" (1/60 de
 *  segundo) se recalcula la posicion de la pelota, se revisan rebotes y
 *  colisiones, y se actualiza el score.
 * ========================================================================== */
#include "game.h"
#include "protocol.h"   /* FIELD_WIDTH, FIELD_HEIGHT, PADDLE_HEIGHT, WINNING_SCORE, SIDE_* */

/* --- Parametros de la fisica (se pueden ajustar para dificultad) --- */
#define BALL_SPEED_X   6.0f   /* velocidad horizontal base de la pelota */
#define BALL_SPEED_Y   4.0f   /* velocidad vertical inicial */
#define PADDLE_SPEED   9.0f   /* cuanto se mueve la paleta por tick */
#define PADDLE_X_LEFT  20.0f  /* posicion X de la cara de la paleta izquierda */
#define PADDLE_X_RIGHT (FIELD_WIDTH - 20.0f)  /* cara de la paleta derecha */
#define BALL_RADIUS    8.0f

/* Coloca la pelota en el centro y la lanza hacia un lado. */
void game_serve(GameState *g, int to_right) {
    g->ball_x = FIELD_WIDTH / 2.0f;
    g->ball_y = FIELD_HEIGHT / 2.0f;
    g->ball_vx = to_right ? BALL_SPEED_X : -BALL_SPEED_X;
    g->ball_vy = BALL_SPEED_Y;   /* siempre arranca bajando; rebota luego */
}

/* Inicializa la partida completa. */
void game_init(GameState *g) {
    /* Paletas centradas verticalmente. */
    g->paddle_left  = (FIELD_HEIGHT - PADDLE_HEIGHT) / 2.0f;
    g->paddle_right = (FIELD_HEIGHT - PADDLE_HEIGHT) / 2.0f;

    g->score_left  = 0;
    g->score_right = 0;
    g->game_over   = 0;
    g->winner_side = 0;

    game_serve(g, 1);  /* primer saque hacia la derecha */
}

/* Mueve una paleta hacia arriba o abajo, sin salirse del campo. */
void game_move_paddle(GameState *g, int side, int direction) {
    float *paddle = (side == SIDE_LEFT) ? &g->paddle_left : &g->paddle_right;

    if (direction == DIR_UP) {
        *paddle -= PADDLE_SPEED;
    } else if (direction == DIR_DOWN) {
        *paddle += PADDLE_SPEED;
    }

    /* Limitar: la paleta no puede salirse por arriba (y<0) ni por abajo. */
    if (*paddle < 0) {
        *paddle = 0;
    }
    float max_y = FIELD_HEIGHT - PADDLE_HEIGHT;
    if (*paddle > max_y) {
        *paddle = max_y;
    }
}

/* Comprueba si la pelota (a la altura ball_y) toca una paleta cuyo borde
 * superior esta en paddle_y. Devuelve 1 si hay contacto vertical. */
static int hits_paddle(float ball_y, float paddle_y) {
    return (ball_y >= paddle_y) && (ball_y <= paddle_y + PADDLE_HEIGHT);
}

/* Anota un punto para un lado y reinicia el saque (o termina la partida). */
static void score_point(GameState *g, int side) {
    if (side == SIDE_LEFT) {
        g->score_left++;
    } else {
        g->score_right++;
    }

    /* Revisar si alguien gano. */
    if (g->score_left >= WINNING_SCORE) {
        g->game_over = 1;
        g->winner_side = SIDE_LEFT;
    } else if (g->score_right >= WINNING_SCORE) {
        g->game_over = 1;
        g->winner_side = SIDE_RIGHT;
    } else {
        /* Saque hacia quien acaba de recibir el punto en contra:
         * si anoto la izquierda, saca hacia la derecha, y viceversa. */
        game_serve(g, side == SIDE_LEFT ? 1 : 0);
    }
}

/* Avanza la simulacion un tick. */
void game_tick(GameState *g) {
    if (g->game_over) {
        return;  /* si ya termino, no se simula mas */
    }

    /* --- 1. Mover la pelota --- */
    g->ball_x += g->ball_vx;
    g->ball_y += g->ball_vy;

    /* --- 2. Rebote contra pared superior e inferior --- */
    if (g->ball_y <= BALL_RADIUS) {
        g->ball_y = BALL_RADIUS;
        g->ball_vy = -g->ball_vy;   /* invertir direccion vertical */
    } else if (g->ball_y >= FIELD_HEIGHT - BALL_RADIUS) {
        g->ball_y = FIELD_HEIGHT - BALL_RADIUS;
        g->ball_vy = -g->ball_vy;
    }

    /* --- 3. Colision con la paleta IZQUIERDA --- */
    if (g->ball_vx < 0 && g->ball_x <= PADDLE_X_LEFT + BALL_RADIUS) {
        if (hits_paddle(g->ball_y, g->paddle_left)) {
            g->ball_x = PADDLE_X_LEFT + BALL_RADIUS;  /* sacarla del borde */
            g->ball_vx = -g->ball_vx;                 /* rebote horizontal */
        } else if (g->ball_x < 0) {
            /* La pelota paso la paleta izquierda: punto para la DERECHA. */
            score_point(g, SIDE_RIGHT);
        }
    }

    /* --- 4. Colision con la paleta DERECHA --- */
    if (g->ball_vx > 0 && g->ball_x >= PADDLE_X_RIGHT - BALL_RADIUS) {
        if (hits_paddle(g->ball_y, g->paddle_right)) {
            g->ball_x = PADDLE_X_RIGHT - BALL_RADIUS;
            g->ball_vx = -g->ball_vx;
        } else if (g->ball_x > FIELD_WIDTH) {
            /* La pelota paso la paleta derecha: punto para la IZQUIERDA. */
            score_point(g, SIDE_LEFT);
        }
    }
}
