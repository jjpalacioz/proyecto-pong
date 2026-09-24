"""
main.py  —  Cliente Pong (programa principal)
============================================================================
Une todo: la red (network.py), el dibujo (ui.py) y el protocolo (protocol.py).

Uso:
    python main.py <IP_SERVIDOR> <PUERTO>
    ejemplo:  python main.py 127.0.0.1 5000

Estados de la aplicacion (maquina de estados del cliente, ver docs/PROTOCOL.md):
    REGISTER  -> el usuario escribe nickname/email y se registra
    WAITING   -> registrado; esperando (matchmaking llega en la Fase 7)
    PLAYING   -> en partida; enviar input y dibujar el estado (Fase 6)
    GAMEOVER  -> fin de partida
    ERROR     -> se muestra un error

NOTA: la parte de juego (PLAYING) se completa en la Fase 6, cuando el servidor
envie MSG_STATE. En la Fase 5 dejamos funcionando el registro y la ventana base.
"""

import sys

import pygame

import protocol as p
import network
import ui


def main():
    host, port = _parse_args()

    # --- Inicializar Pygame ---
    pygame.init()
    screen = pygame.display.set_mode((ui.WINDOW_WIDTH, ui.WINDOW_HEIGHT))
    pygame.display.set_caption("PONG - Cliente")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas", 48)
    small_font = pygame.font.SysFont("consolas", 24)

    # --- Variables de estado de la aplicacion ---
    app = {
        "state": "REGISTER",   # estado actual (ver docstring)
        "nickname": "",
        "email": "",
        "active_field": "nick",  # "nick" o "mail"
        "error_msg": None,
        "player_id": None,
        "game_state": None,      # ultimo MSG_STATE recibido (Fase 6)
    }

    net = network.PongClientNet(host, port)

    running = True
    while running:
        # ==============================================================
        #  1. PROCESAR EVENTOS
        # ==============================================================
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                if app["state"] == "REGISTER":
                    _handle_register_key(event, app, net)
                elif app["state"] == "WAITING" and event.key in (pygame.K_RETURN, pygame.K_KP_ENTER):
                    # Al presionar ENTER en la sala de espera, pedimos jugar.
                    _start_game(app, net)

        # ==============================================================
        #  2. DIBUJAR SEGUN EL ESTADO
        # ==============================================================
        s = app["state"]
        if s == "REGISTER":
            ui.draw_register(screen, font, small_font,
                             app["nickname"], app["email"],
                             app["active_field"], app["error_msg"])

        elif s == "WAITING":
            ui.draw_message(screen, small_font,
                            [f"Registrado como '{app['nickname']}' (id={app['player_id']})",
                             "Presiona ENTER para jugar"])

        elif s == "PLAYING":
            # En cada frame: enviar el input actual y recibir los estados.
            _play_step(app, net)
            if app["game_state"]:
                ui.draw_game(screen, app["game_state"], font)
            else:
                ui.draw_message(screen, small_font, ["Iniciando partida..."])

        elif s == "GAMEOVER":
            ui.draw_message(screen, font, ["FIN DE LA PARTIDA"])

        elif s == "ERROR":
            ui.draw_message(screen, small_font,
                            ["ERROR", app["error_msg"] or "desconocido",
                             "Cierra la ventana para salir."], color=ui.RED)

        pygame.display.flip()
        clock.tick(60)   # limitar a 60 FPS

    net.close()
    pygame.quit()


def _parse_args():
    """Lee y valida <IP> <PUERTO> de la linea de comandos."""
    if len(sys.argv) != 3:
        print("Uso: python main.py <IP_SERVIDOR> <PUERTO>")
        print("Ejemplo: python main.py 127.0.0.1 5000")
        sys.exit(1)
    host = sys.argv[1]
    try:
        port = int(sys.argv[2])
    except ValueError:
        print("ERROR: el puerto debe ser un numero")
        sys.exit(1)
    return host, port


def _handle_register_key(event, app, net):
    """
    Maneja el teclado en la pantalla de registro:
      - TAB: cambia entre el campo nickname y el campo email
      - BACKSPACE: borra un caracter del campo activo
      - ENTER: intenta registrarse contra el servidor
      - cualquier otra tecla imprimible: se agrega al campo activo
    """
    field = "nickname" if app["active_field"] == "nick" else "email"

    if event.key == pygame.K_TAB:
        app["active_field"] = "mail" if app["active_field"] == "nick" else "nick"

    elif event.key == pygame.K_BACKSPACE:
        app[field] = app[field][:-1]

    elif event.key in (pygame.K_RETURN, pygame.K_KP_ENTER):
        _try_register(app, net)

    else:
        # event.unicode es el caracter escrito (respeta mayus/minus, etc.)
        ch = event.unicode
        if ch and ch.isprintable() and len(app[field]) < 30:
            app[field] += ch


def _try_register(app, net):
    """Se conecta (si falta) y envia el registro, procesando la respuesta."""
    if not app["nickname"] or not app["email"]:
        app["error_msg"] = "Nickname y email no pueden estar vacios"
        return

    try:
        if net.sock is None:
            net.connect()
        net.register(app["nickname"], app["email"])

        msg_type, payload = net.recv_message()
        if msg_type == p.MSG_REGISTER_OK:
            app["player_id"] = network.parse_register_ok(payload)
            app["error_msg"] = None
            app["state"] = "WAITING"
        elif msg_type in (p.MSG_REGISTER_ERR, p.MSG_ERROR):
            code = network.parse_error(payload)
            app["error_msg"] = p.ERROR_MESSAGES.get(code, f"Error {code}")
        else:
            app["error_msg"] = f"Respuesta inesperada (0x{msg_type:02X})"

    except network.ConnectionClosed:
        app["error_msg"] = "El servidor cerro la conexion"
        app["state"] = "ERROR"
    except (OSError, ValueError) as e:
        app["error_msg"] = f"No se pudo conectar/registrar: {e}"
        app["state"] = "ERROR"


def _start_game(app, net):
    """Pide jugar (MSG_QUEUE) y pone el socket en modo no bloqueante para el
    bucle de juego, de modo que recibir estados no congele la ventana."""
    try:
        net.send_queue()
        net.sock.setblocking(False)   # no bloquear al recibir estados
        app["state"] = "PLAYING"
    except (OSError, network.ConnectionClosed) as e:
        app["error_msg"] = f"No se pudo iniciar la partida: {e}"
        app["state"] = "ERROR"


def _play_step(app, net):
    """
    Un paso del bucle de juego (se llama cada frame):
      1. Lee las teclas de flecha y envia MSG_INPUT (arriba/abajo/quieto).
      2. Recibe TODOS los MSG_STATE que hayan llegado (sin bloquear) y guarda
         el ultimo para dibujarlo. Tambien maneja MSG_GAME_OVER.
    """
    # --- 1. Enviar input segun las teclas presionadas ahora mismo ---
    keys = pygame.key.get_pressed()
    if keys[pygame.K_UP]:
        direction = p.DIR_UP
    elif keys[pygame.K_DOWN]:
        direction = p.DIR_DOWN
    else:
        direction = p.DIR_NONE
    try:
        net.send_input(direction)
    except (OSError, network.ConnectionClosed):
        app["error_msg"] = "Conexion perdida"
        app["state"] = "ERROR"
        return

    # --- 2. Recibir los estados disponibles (modo no bloqueante) ---
    try:
        for _ in range(20):   # procesar hasta 20 mensajes por frame
            result = net.recv_message_nonblocking()
            if result is None:
                break   # no hay mas mensajes completos por ahora
            msg_type, payload = result
            if msg_type == p.MSG_STATE:
                app["game_state"] = network.parse_state(payload)
            elif msg_type == p.MSG_GAME_OVER:
                app["state"] = "GAMEOVER"
                return
    except network.ConnectionClosed:
        app["error_msg"] = "El servidor cerro la conexion"
        app["state"] = "ERROR"


if __name__ == "__main__":
    main()
