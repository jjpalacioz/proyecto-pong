"""
network.py  —  Capa de red del cliente (conexion TCP + protocolo binario)
============================================================================
Aqui vive TODA la comunicacion con el servidor. Esta capa NO sabe nada de
Pygame ni de graficos: solo abre el socket, arma los mensajes binarios y los
envia/recibe. Asi separamos "red" de "presentacion" (buen diseno).

CLAVE: usamos el modulo `struct` con el formato '!' (network byte order =
big-endian) para que los bytes queden EXACTAMENTE como los espera el
servidor en C (que usa htons/ntohs). Si no coincidieran, no se entenderian.

Formato del header (6 bytes), identico al de docs/PROTOCOL.md:
    MAGIC (2) | VERSION (1) | TYPE (1) | LENGTH (2)
El codigo de formato de struct para eso es "!HBBH":
    !  -> big-endian (network order)
    H  -> unsigned short  (2 bytes)  -> MAGIC
    B  -> unsigned char   (1 byte)   -> VERSION
    B  -> unsigned char   (1 byte)   -> TYPE
    H  -> unsigned short  (2 bytes)  -> LENGTH
"""

import socket
import struct

import protocol as p


class ConnectionClosed(Exception):
    """Se lanza cuando el servidor cierra la conexion."""
    pass


class PongClientNet:
    """Maneja la conexion TCP con el servidor Pong y el protocolo binario."""

    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None

    # ------------------------------------------------------------------
    #  Conexion / desconexion
    # ------------------------------------------------------------------
    def connect(self):
        """Abre el socket TCP y se conecta al servidor."""
        # AF_INET = IPv4, SOCK_STREAM = TCP (igual que el servidor en C).
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))

    def close(self):
        """Cierra el socket ordenadamente."""
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    # ------------------------------------------------------------------
    #  Utilidades de bajo nivel: enviar/recibir EXACTAMENTE N bytes
    # ------------------------------------------------------------------
    def _send_all(self, data):
        """Envia todos los bytes de 'data'. sendall ya reintenta por dentro."""
        self.sock.sendall(data)

    def _recv_exact(self, n):
        """
        Lee EXACTAMENTE n bytes del socket. Igual que recv_all() del servidor C:
        en TCP un solo recv() puede devolver menos bytes de los pedidos, asi que
        insistimos en un bucle hasta juntar los n bytes (o detectar cierre).
        """
        chunks = []
        received = 0
        while received < n:
            chunk = self.sock.recv(n - received)
            if not chunk:                      # recv devuelve b"" = servidor cerro
                raise ConnectionClosed()
            chunks.append(chunk)
            received += len(chunk)
        return b"".join(chunks)

    # ------------------------------------------------------------------
    #  Enviar un mensaje del protocolo (header + payload)
    # ------------------------------------------------------------------
    def send_message(self, msg_type, payload=b""):
        """
        Arma un mensaje bien formado y lo envia:
          [MAGIC][VERSION][TYPE][LENGTH] + payload
        """
        if len(payload) > p.MAX_PAYLOAD:
            raise ValueError("payload demasiado grande")

        # Empaquetar el header en 6 bytes, big-endian.
        header = struct.pack("!HBBH",
                             p.PROTO_MAGIC,
                             p.PROTO_VERSION,
                             msg_type,
                             len(payload))
        self._send_all(header + payload)

    # ------------------------------------------------------------------
    #  Recibir un mensaje completo del protocolo (framing)
    # ------------------------------------------------------------------
    def recv_message(self):
        """
        Lee un mensaje completo y valida el header. Devuelve (type, payload).
        Aplica el mismo framing del servidor:
          1. leer 6 bytes de header
          2. desempaquetar y validar MAGIC/VERSION
          3. leer exactamente LENGTH bytes de payload
        """
        header = self._recv_exact(p.HEADER_SIZE)
        magic, version, msg_type, length = struct.unpack("!HBBH", header)

        # Validaciones (defensa ante datos corruptos, igual que el servidor).
        if magic != p.PROTO_MAGIC:
            raise ValueError("MAGIC invalido en la respuesta del servidor")
        if version != p.PROTO_VERSION:
            raise ValueError("Version de protocolo no soportada")

        payload = self._recv_exact(length) if length > 0 else b""
        return msg_type, payload

    # ------------------------------------------------------------------
    #  Mensajes especificos (comodos para el resto del cliente)
    # ------------------------------------------------------------------
    def register(self, nickname, email):
        """
        Envia MSG_REGISTER con nickname y email.
        Payload: [1 len_nick][nick][1 len_mail][mail]  (strings length-prefixed).
        """
        nick_b = nickname.encode("utf-8")
        mail_b = email.encode("utf-8")
        if not (1 <= len(nick_b) <= 255) or not (1 <= len(mail_b) <= 255):
            raise ValueError("nickname/email vacios o demasiado largos")

        payload = bytes([len(nick_b)]) + nick_b + bytes([len(mail_b)]) + mail_b
        self.send_message(p.MSG_REGISTER, payload)

    def send_input(self, direction):
        """Envia MSG_INPUT con 1 byte de direccion (DIR_NONE/UP/DOWN)."""
        self.send_message(p.MSG_INPUT, bytes([direction]))

    def send_queue(self):
        """Pide entrar a la cola de emparejamiento (payload vacio)."""
        self.send_message(p.MSG_QUEUE)


# ----------------------------------------------------------------------------
#  Funciones para DESEMPAQUETAR payloads que llegan del servidor.
#  Se ponen aparte para poder reusarlas y probarlas facilmente.
# ----------------------------------------------------------------------------
def parse_register_ok(payload):
    """MSG_REGISTER_OK: [4 bytes player_id] -> devuelve el id (int)."""
    (player_id,) = struct.unpack("!I", payload)
    return player_id


def parse_state(payload):
    """
    MSG_STATE: devuelve un diccionario con el estado del juego.
    Formato: ball_x(2) ball_y(2) paddle_left(2) paddle_right(2)
             score_left(1) score_right(1)  = 10 bytes.
    """
    (ball_x, ball_y, paddle_left, paddle_right,
     score_left, score_right) = struct.unpack("!HHHHBB", payload)
    return {
        "ball_x": ball_x,
        "ball_y": ball_y,
        "paddle_left": paddle_left,
        "paddle_right": paddle_right,
        "score_left": score_left,
        "score_right": score_right,
    }


def parse_error(payload):
    """MSG_ERROR / MSG_REGISTER_ERR: [1 byte error_code] -> devuelve el codigo."""
    return payload[0] if payload else 0
