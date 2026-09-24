"""
ui.py  —  Funciones de dibujo con Pygame
============================================================================
Esta capa se encarga SOLO de dibujar en pantalla. No sabe de red ni de
protocolo. Recibe datos (posiciones, score, textos) y los pinta.

Escala: el servidor trabaja en un campo de FIELD_WIDTH x FIELD_HEIGHT unidades.
La ventana puede tener otro tamano, asi que convertimos ("escalamos") las
coordenadas del servidor a pixeles de la ventana.
"""

import pygame
import protocol as p

# --- Colores (R, G, B) ---
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY  = (120, 120, 120)
GREEN = (0, 200, 0)
RED   = (200, 0, 0)

# --- Tamano de la ventana (pixeles) ---
WINDOW_WIDTH = 800
WINDOW_HEIGHT = 600

# Factores de escala: de unidades del servidor a pixeles de la ventana.
SCALE_X = WINDOW_WIDTH / p.FIELD_WIDTH
SCALE_Y = WINDOW_HEIGHT / p.FIELD_HEIGHT

# Ancho de las paletas en pixeles.
PADDLE_WIDTH = 12


def draw_field(screen):
    """Dibuja el fondo negro y la linea punteada central."""
    screen.fill(BLACK)
    # Linea central punteada.
    mid_x = WINDOW_WIDTH // 2
    dash_h = 18
    y = 0
    while y < WINDOW_HEIGHT:
        pygame.draw.rect(screen, GRAY, (mid_x - 2, y, 4, dash_h))
        y += dash_h * 2


def draw_game(screen, state, font):
    """
    Dibuja el estado del juego que llego del servidor (dict de parse_state):
    pelota, paletas y el SCORE en tiempo real.
    """
    draw_field(screen)

    # --- Paletas (escaladas a pixeles) ---
    paddle_h_px = int(p.PADDLE_HEIGHT * SCALE_Y)

    left_y = int(state["paddle_left"] * SCALE_Y)
    right_y = int(state["paddle_right"] * SCALE_Y)

    # Paleta izquierda pegada al borde izquierdo.
    pygame.draw.rect(screen, WHITE, (10, left_y, PADDLE_WIDTH, paddle_h_px))
    # Paleta derecha pegada al borde derecho.
    pygame.draw.rect(screen, WHITE,
                     (WINDOW_WIDTH - 10 - PADDLE_WIDTH, right_y, PADDLE_WIDTH, paddle_h_px))

    # --- Pelota ---
    ball_x = int(state["ball_x"] * SCALE_X)
    ball_y = int(state["ball_y"] * SCALE_Y)
    pygame.draw.circle(screen, WHITE, (ball_x, ball_y), 8)

    # --- Score en tiempo real (arriba, centrado a cada lado) ---
    score_text = font.render(
        f'{state["score_left"]}   {state["score_right"]}', True, WHITE)
    rect = score_text.get_rect(center=(WINDOW_WIDTH // 2, 40))
    screen.blit(score_text, rect)


def draw_message(screen, font, lines, color=WHITE):
    """Dibuja una o varias lineas de texto centradas (para pantallas de aviso)."""
    screen.fill(BLACK)
    total_h = len(lines) * 40
    start_y = (WINDOW_HEIGHT - total_h) // 2
    for i, line in enumerate(lines):
        surf = font.render(line, True, color)
        rect = surf.get_rect(center=(WINDOW_WIDTH // 2, start_y + i * 40))
        screen.blit(surf, rect)


def draw_register(screen, font, small_font, nickname, email, active_field, error=None):
    """
    Dibuja la pantalla de registro: dos campos (nickname, email) y el campo
    activo resaltado. 'active_field' es "nick" o "mail".
    """
    screen.fill(BLACK)

    title = font.render("PONG - Registro", True, WHITE)
    screen.blit(title, title.get_rect(center=(WINDOW_WIDTH // 2, 80)))

    # Campo nickname
    _draw_input_box(screen, small_font, "Nickname:", nickname,
                    200, active_field == "nick")
    # Campo email
    _draw_input_box(screen, small_font, "Email:", email,
                    280, active_field == "mail")

    hint = small_font.render("TAB: cambiar campo   ENTER: registrarse", True, GRAY)
    screen.blit(hint, hint.get_rect(center=(WINDOW_WIDTH // 2, 400)))

    if error:
        err = small_font.render(error, True, RED)
        screen.blit(err, err.get_rect(center=(WINDOW_WIDTH // 2, 450)))


def _draw_input_box(screen, font, label, value, y, active):
    """Ayudante: dibuja una etiqueta + una caja de texto."""
    label_surf = font.render(label, True, WHITE)
    screen.blit(label_surf, (200, y - 30))

    box_color = GREEN if active else GRAY
    pygame.draw.rect(screen, box_color, (200, y, 400, 36), 2)

    # Cursor "_" cuando el campo esta activo.
    shown = value + ("_" if active else "")
    val_surf = font.render(shown, True, WHITE)
    screen.blit(val_surf, (210, y + 6))
