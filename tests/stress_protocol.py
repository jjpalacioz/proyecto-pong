#!/usr/bin/env python3
"""
Pruebas de estres del protocolo MyAppGameProtocol.

Manda al servidor los casos de docs/EDGE_CASES.md y comprueba dos cosas:
  1. El proceso del servidor sigue vivo despues de cada ataque.
  2. Cuando el caso ya esta implementado, la respuesta binaria es la exigida.

Uso (desde la raiz del repo, en Linux o WSL):

    make -C server
    python3 tests/stress_protocol.py

Codigos de salida:
    0  no hubo fallos. Puede haber casos [pendiente]: el servidor aun no
       implementa esa parte del contrato (ver la seccion 4 del documento).
    1  un caso que el servidor ya debia cumplir fallo, o el proceso murio.
"""

import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "client" / "src"))

import protocol as p  # noqa: E402  constantes compartidas con el cliente

HOST = "127.0.0.1"
TIMEOUT = 2.0
MAX_CLIENTS = 64  # cupo que el servidor debe imponer (caso P07)

# Resultados acumulados para el resumen final.
ok_cases = []
pending_cases = []
failed_cases = []

_nick_lock = threading.Lock()
_nick_seq = 0
server_proc = None
server_port = None


class Pending(Exception):
    """El servidor respondio algo distinto de lo exigido en un caso P."""


class CaseError(Exception):
    """Un caso que ya debia cumplirse no se cumplio."""


def unique_nick():
    """Nickname corto y unico. Cabe en el maximo de 32 bytes."""
    global _nick_seq
    with _nick_lock:
        _nick_seq += 1
        return f"u{_nick_seq:04d}"


def header(msg_type, length, magic=p.PROTO_MAGIC, version=p.PROTO_VERSION):
    """Arma los 6 bytes del header. '!' es big-endian, igual que htons en C."""
    return struct.pack("!HBBH", magic, version, msg_type, length)


def register_payload(nick, email):
    """Payload de MSG_REGISTER: [len nick][nick][len email][email]."""
    nick_b = nick.encode("utf-8")
    mail_b = email.encode("utf-8")
    return bytes([len(nick_b)]) + nick_b + bytes([len(mail_b)]) + mail_b


def connect():
    sock = socket.create_connection((HOST, server_port), timeout=TIMEOUT)
    sock.settimeout(TIMEOUT)
    return sock


def recvall(sock, n):
    """Lee exactamente n bytes. None si el peer cerro antes de completarlos."""
    buf = b""
    while len(buf) < n:
        try:
            chunk = sock.recv(n - len(buf))
        except socket.timeout:
            raise
        if not chunk:
            return None
        buf += chunk
    return buf


def read_message(sock):
    """
    Lee un mensaje completo.
    Devuelve (type, payload) o None si la conexion se cerro sin un mensaje.
    """
    hdr = recvall(sock, p.HEADER_SIZE)
    if hdr is None:
        return None
    magic, version, msg_type, length = struct.unpack("!HBBH", hdr)
    if magic != p.PROTO_MAGIC or version != p.PROTO_VERSION:
        raise CaseError(
            f"respuesta con header invalido magic=0x{magic:04X} ver={version}"
        )
    payload = b"" if length == 0 else recvall(sock, length)
    if payload is None:
        return None
    return msg_type, payload


def send_bytes(sock, data):
    sock.sendall(data)


def send_register(sock, nick, email):
    payload = register_payload(nick, email)
    send_bytes(sock, header(p.MSG_REGISTER, len(payload)) + payload)


def expect_error(sock, code):
    msg = read_message(sock)
    if msg is None:
        raise CaseError(f"se esperaba MSG_ERROR 0x{code:02X} y el servidor cerro")
    msg_type, payload = msg
    if msg_type != p.MSG_ERROR or payload != bytes([code]):
        got = payload.hex() if payload else "vacio"
        raise CaseError(
            f"se esperaba MSG_ERROR 0x{code:02X}, llego type=0x{msg_type:02X} payload={got}"
        )


def expect_register_ok(sock):
    msg = read_message(sock)
    if msg is None:
        raise CaseError("se esperaba MSG_REGISTER_OK y el servidor cerro")
    msg_type, payload = msg
    if msg_type != p.MSG_REGISTER_OK or len(payload) != 4:
        raise CaseError(
            f"se esperaba MSG_REGISTER_OK de 4 bytes, llego type=0x{msg_type:02X} "
            f"len={0 if payload is None else len(payload)}"
        )
    return struct.unpack("!I", payload)[0]


def server_alive():
    if server_proc.poll() is not None:
        raise CaseError(f"el servidor murio (codigo {server_proc.returncode})")


def assert_still_serving():
    """Otro cliente, en otra conexion, tiene que poder registrarse."""
    server_alive()
    sock = connect()
    try:
        send_register(sock, unique_nick(), "a@b.co")
        expect_register_ok(sock)
    finally:
        sock.close()
    server_alive()


def expect_contract_error(sock, code, porque):
    """
    Para los casos P. Si el codigo ya coincide, el caso pasa.
    Si el servidor responde otra cosa, queda pendiente (no tumba la suite).
    """
    try:
        msg = read_message(sock)
    except socket.timeout:
        raise Pending(f"{porque}. El servidor no respondio a tiempo")
    if msg is None:
        raise Pending(f"{porque}. El servidor cerro el socket sin MSG_ERROR")
    msg_type, payload = msg
    if msg_type == p.MSG_ERROR and payload == bytes([code]):
        return
    got = f"type=0x{msg_type:02X} payload={payload.hex() if payload else 'vacio'}"
    raise Pending(f"{porque}. Llego {got}")


# --------------------------------------------------------------------------
#  Casos C — el servidor actual tiene que pasarlos
# --------------------------------------------------------------------------

def c01_bad_magic():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 0, magic=0x0000))
        expect_error(sock, p.ERR_BAD_MAGIC)
    finally:
        sock.close()
    assert_still_serving()


def c02_bad_version():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 0, version=0x99))
        expect_error(sock, p.ERR_BAD_VERSION)
    finally:
        sock.close()
    assert_still_serving()


def c03_magic_antes_que_version():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 0, magic=0x1111, version=0x99))
        expect_error(sock, p.ERR_BAD_MAGIC)
    finally:
        sock.close()


def c04_basura():
    sock = connect()
    try:
        send_bytes(sock, b"BASURA")
        expect_error(sock, p.ERR_BAD_MAGIC)
    finally:
        sock.close()


def c05_mensaje_partido():
    nick, email = unique_nick(), "a@b.co"
    payload = register_payload(nick, email)
    blob = header(p.MSG_REGISTER, len(payload)) + payload
    sock = connect()
    try:
        send_bytes(sock, blob[:2])
        time.sleep(0.05)
        send_bytes(sock, blob[2:])
        expect_register_ok(sock)
    finally:
        sock.close()


def c06_header_incompleto_y_cierre():
    sock = connect()
    try:
        send_bytes(sock, b"\x50\x47\x01")
        sock.close()
    finally:
        pass
    time.sleep(0.05)
    assert_still_serving()


def c07_dos_mensajes_pegados():
    payload = register_payload(unique_nick(), "a@b.co")
    blob = header(p.MSG_REGISTER, len(payload)) + payload
    blob += header(p.MSG_QUEUE, 0)
    sock = connect()
    try:
        send_bytes(sock, blob)
        player_id = expect_register_ok(sock)
        if player_id == 0:
            raise CaseError("player_id 0 no es un id asignado")
    finally:
        sock.close()


def c08_length_mentiroso_y_cierre():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 200) + b"\x00" * 10)
        sock.close()
    finally:
        pass
    time.sleep(0.05)
    assert_still_serving()


def c09_registro_valido():
    sock = connect()
    try:
        send_register(sock, unique_nick(), "jugador@correo.com")
        expect_register_ok(sock)
    finally:
        sock.close()


def c10_register_vacio():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 0))
        expect_error(sock, p.ERR_INVALID_FIELD)
        send_register(sock, unique_nick(), "a@b.co")
        expect_register_ok(sock)
    finally:
        sock.close()


def c11_nick_vacio():
    sock = connect()
    try:
        payload = bytes([0])
        send_bytes(sock, header(p.MSG_REGISTER, len(payload)) + payload)
        expect_error(sock, p.ERR_INVALID_FIELD)
    finally:
        sock.close()


def c12_email_vacio():
    sock = connect()
    try:
        payload = bytes([2]) + b"ab" + bytes([0])
        send_bytes(sock, header(p.MSG_REGISTER, len(payload)) + payload)
        expect_error(sock, p.ERR_INVALID_FIELD)
    finally:
        sock.close()


def c13_nick_de_32():
    sock = connect()
    try:
        send_register(sock, "n" * 32, "a@b.co")
        expect_register_ok(sock)
    finally:
        sock.close()


def c14_nick_de_33():
    sock = connect()
    try:
        send_register(sock, "n" * 33, "a@b.co")
        expect_error(sock, p.ERR_INVALID_FIELD)
    finally:
        sock.close()


def c15_email_de_64():
    sock = connect()
    try:
        send_register(sock, unique_nick(), "e" * 64)
        expect_register_ok(sock)
    finally:
        sock.close()


def c16_email_de_65():
    sock = connect()
    try:
        send_register(sock, unique_nick(), "e" * 65)
        expect_error(sock, p.ERR_INVALID_FIELD)
    finally:
        sock.close()


def c17_longitud_de_nick_mentirosa():
    sock = connect()
    try:
        payload = bytes([5]) + b"a"  # dice 5 y solo manda 1
        send_bytes(sock, header(p.MSG_REGISTER, len(payload)) + payload)
        expect_error(sock, p.ERR_INVALID_FIELD)
    finally:
        sock.close()


def c18_reintento_en_la_misma_conexion():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_REGISTER, 0))
        expect_error(sock, p.ERR_INVALID_FIELD)
        send_register(sock, unique_nick(), "a@b.co")
        expect_register_ok(sock)
    finally:
        sock.close()


def c19_queue_sin_registro():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_QUEUE, 0))
        expect_error(sock, p.ERR_NOT_REGISTERED)
    finally:
        sock.close()


def c20_input_sin_registro():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_INPUT, 1) + bytes([p.DIR_UP]))
        expect_error(sock, p.ERR_NOT_REGISTERED)
    finally:
        sock.close()


def c21_cierre_sin_datos():
    sock = connect()
    sock.close()
    time.sleep(0.05)
    assert_still_serving()


def c22_cierre_a_mitad_de_partida():
    # Con matchmaking 1vs1 (Fase 7) se necesitan DOS jugadores para que arranque
    # la partida. Emparejamos dos, confirmamos MSG_GAME_START en ambos y cerramos
    # uno a mitad; el servidor no debe caerse.
    a = connect()
    b = connect()
    try:
        send_register(a, unique_nick(), "a@b.co"); expect_register_ok(a)
        send_register(b, unique_nick(), "b@b.co"); expect_register_ok(b)
        send_bytes(a, header(p.MSG_QUEUE, 0))
        send_bytes(b, header(p.MSG_QUEUE, 0))
        # Ambos deben recibir MATCH_FOUND y luego GAME_START (en algún orden).
        got_start = False
        for sock in (a, b):
            for _ in range(4):
                msg = read_message(sock)
                if msg and msg[0] == p.MSG_GAME_START:
                    got_start = True
                    break
        if not got_start:
            raise CaseError("no llego MSG_GAME_START tras emparejar")
    finally:
        a.close()   # un jugador se cae a mitad
        b.close()
    time.sleep(0.05)
    assert_still_serving()


def c23_varios_clientes_malos():
    errors = []

    def atacar():
        try:
            sock = connect()
            try:
                send_bytes(sock, header(p.MSG_REGISTER, 0, magic=0xDEAD))
                expect_error(sock, p.ERR_BAD_MAGIC)
            finally:
                sock.close()
        except Exception as exc:  # un hilo no debe tumbar a los otros
            errors.append(exc)

    hilos = [threading.Thread(target=atacar) for _ in range(8)]
    for h in hilos:
        h.start()
    for h in hilos:
        h.join()
    if errors:
        raise CaseError(f"un cliente paralelo fallo: {errors[0]}")
    assert_still_serving()


def c24_direccion_invalida_en_partida():
    # Igual que C22: se necesitan dos jugadores. Uno manda una direccion de input
    # invalida (9) y el servidor debe seguir enviando MSG_STATE sin caerse.
    a = connect()
    b = connect()
    try:
        send_register(a, unique_nick(), "a@b.co"); expect_register_ok(a)
        send_register(b, unique_nick(), "b@b.co"); expect_register_ok(b)
        send_bytes(a, header(p.MSG_QUEUE, 0))
        send_bytes(b, header(p.MSG_QUEUE, 0))
        # Esperar a que 'a' este en partida (llego GAME_START).
        started = False
        deadline = time.time() + 2.0
        while time.time() < deadline and not started:
            msg = read_message(a)
            if msg and msg[0] == p.MSG_GAME_START:
                started = True
        if not started:
            raise CaseError("no llego MSG_GAME_START")
        # Direccion invalida (9): no debe mover la paleta ni caer el servidor.
        send_bytes(a, header(p.MSG_INPUT, 1) + bytes([9]))
        vio_estado = False
        deadline = time.time() + 1.5
        while time.time() < deadline:
            msg = read_message(a)
            if msg is None:
                break
            if msg[0] == p.MSG_STATE and len(msg[1]) == 10:
                vio_estado = True
                break
        if not vio_estado:
            raise CaseError("despues de un input invalido dejo de llegar MSG_STATE")
    finally:
        a.close()
        b.close()
    assert_still_serving()


def c25_ids_unicos_en_paralelo():
    ids = []
    errors = []
    lock = threading.Lock()

    def registrar():
        try:
            sock = connect()
            try:
                send_register(sock, unique_nick(), "a@b.co")
                player_id = expect_register_ok(sock)
            finally:
                sock.close()
            with lock:
                ids.append(player_id)
        except Exception as exc:
            errors.append(exc)

    hilos = [threading.Thread(target=registrar) for _ in range(5)]
    for h in hilos:
        h.start()
    for h in hilos:
        h.join()
    if errors:
        raise CaseError(f"un registro paralelo fallo: {errors[0]}")
    if len(ids) != 5 or len(set(ids)) != 5:
        raise CaseError(f"ids no unicos: {ids}")


def c26_length_colgado_no_bloquea_a_los_demas():
    colgado = connect()
    try:
        send_bytes(colgado, header(p.MSG_REGISTER, 500))
        time.sleep(0.1)
        assert_still_serving()
    finally:
        colgado.close()
    time.sleep(0.05)
    assert_still_serving()


# --------------------------------------------------------------------------
#  Casos P — contrato de la seccion 4. Si falta, queda [pendiente]
# --------------------------------------------------------------------------

def p01_tipo_desconocido():
    sock = connect()
    try:
        send_bytes(sock, header(0xAB, 0))
        expect_contract_error(
            sock, p.ERR_UNKNOWN_TYPE, "un TYPE que no existe debe ser ERR_UNKNOWN_TYPE"
        )
    finally:
        sock.close()
    assert_still_serving()


def p02_nick_repetido():
    nick = unique_nick()
    primero = connect()
    segundo = connect()
    try:
        send_register(primero, nick, "a@b.co")
        expect_register_ok(primero)
        send_register(segundo, nick, "b@b.co")
        expect_contract_error(
            segundo, p.ERR_NICK_TAKEN, "el segundo nickname igual debe ser ERR_NICK_TAKEN"
        )
    finally:
        primero.close()
        segundo.close()


def p03_ping():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_PING, 0))
        try:
            msg = read_message(sock)
        except socket.timeout:
            raise Pending("MSG_PING debe responder MSG_PONG. No hubo respuesta")
        if msg is None or msg[0] != p.MSG_PONG or msg[1] != b"":
            llego = "cierre" if msg is None else f"type=0x{msg[0]:02X}"
            raise Pending(f"MSG_PING debe responder MSG_PONG. Llego {llego}")
    finally:
        sock.close()


def p04_byte_sobrante():
    sock = connect()
    try:
        payload = register_payload(unique_nick(), "a@b.co") + b"\xff"
        send_bytes(sock, header(p.MSG_REGISTER, len(payload)) + payload)
        expect_contract_error(
            sock, p.ERR_BAD_LENGTH, "un registro con bytes de sobra debe ser ERR_BAD_LENGTH"
        )
    finally:
        sock.close()


def p05_input_fuera_de_turno():
    sock = connect()
    try:
        send_register(sock, unique_nick(), "a@b.co")
        expect_register_ok(sock)
        send_bytes(sock, header(p.MSG_INPUT, 1) + bytes([p.DIR_UP]))
        expect_contract_error(
            sock,
            p.ERR_NOT_REGISTERED,
            "MSG_INPUT antes de jugar debe avisar con MSG_ERROR",
        )
    finally:
        sock.close()


def p06_queue_con_length_malo():
    sock = connect()
    try:
        send_bytes(sock, header(p.MSG_QUEUE, 3) + b"\x00\x00\x00")
        expect_contract_error(
            sock, p.ERR_BAD_LENGTH, "MSG_QUEUE con LENGTH distinto de 0 debe ser ERR_BAD_LENGTH"
        )
    finally:
        sock.close()


def p07_servidor_lleno():
    sockets = []
    try:
        for _ in range(MAX_CLIENTS):
            sock = connect()
            sockets.append(sock)
            send_register(sock, unique_nick(), "a@b.co")
            expect_register_ok(sock)
        extra = connect()
        sockets.append(extra)
        send_register(extra, unique_nick(), "a@b.co")
        expect_contract_error(
            extra,
            p.ERR_SERVER_FULL,
            f"el cliente {MAX_CLIENTS + 1} debe recibir ERR_SERVER_FULL",
        )
    finally:
        for sock in sockets:
            try:
                sock.close()
            except OSError:
                pass
    time.sleep(0.1)
    assert_still_serving()


CASOS = [
    ("C01", "MAGIC invalido", c01_bad_magic),
    ("C02", "VERSION invalida", c02_bad_version),
    ("C03", "MAGIC se valida antes que VERSION", c03_magic_antes_que_version),
    ("C04", "seis bytes de basura", c04_basura),
    ("C05", "mensaje partido en dos envios", c05_mensaje_partido),
    ("C06", "header incompleto y cierre", c06_header_incompleto_y_cierre),
    ("C07", "dos mensajes pegados", c07_dos_mensajes_pegados),
    ("C08", "LENGTH mentiroso y cierre", c08_length_mentiroso_y_cierre),
    ("C09", "registro valido", c09_registro_valido),
    ("C10", "REGISTER con LENGTH 0", c10_register_vacio),
    ("C11", "nickname vacio", c11_nick_vacio),
    ("C12", "email vacio", c12_email_vacio),
    ("C13", "nickname de 32 bytes", c13_nick_de_32),
    ("C14", "nickname de 33 bytes", c14_nick_de_33),
    ("C15", "email de 64 bytes", c15_email_de_64),
    ("C16", "email de 65 bytes", c16_email_de_65),
    ("C17", "longitud interna del nick mentirosa", c17_longitud_de_nick_mentirosa),
    ("C18", "reintento de registro en el mismo socket", c18_reintento_en_la_misma_conexion),
    ("C19", "QUEUE sin haberse registrado", c19_queue_sin_registro),
    ("C20", "INPUT sin haberse registrado", c20_input_sin_registro),
    ("C21", "conectar y cerrar sin datos", c21_cierre_sin_datos),
    ("C22", "cierre a mitad de la partida", c22_cierre_a_mitad_de_partida),
    ("C23", "varios clientes malos a la vez", c23_varios_clientes_malos),
    ("C24", "direccion de input invalida", c24_direccion_invalida_en_partida),
    ("C25", "cinco ids distintos en paralelo", c25_ids_unicos_en_paralelo),
    ("C26", "LENGTH colgado no bloquea al resto", c26_length_colgado_no_bloquea_a_los_demas),
    ("P01", "tipo desconocido", p01_tipo_desconocido),
    ("P02", "nickname repetido", p02_nick_repetido),
    ("P03", "ping / pong", p03_ping),
    ("P04", "bytes de sobra en el registro", p04_byte_sobrante),
    ("P05", "INPUT cuando todavia no se esta jugando", p05_input_fuera_de_turno),
    ("P06", "QUEUE con LENGTH distinto de 0", p06_queue_con_length_malo),
    ("P07", "cliente 65 con el servidor lleno", p07_servidor_lleno),
]


def start_server():
    global server_proc, server_port
    binary = ROOT / "server" / "server"
    if not binary.is_file():
        print("No esta compilado el servidor. Corre primero: make -C server", file=sys.stderr)
        return False

    # Puerto efimero: el 0 no sirve porque el servidor recibe el puerto por
    # argumento. Probamos uno alto y, si esta ocupado, el bind del servidor falla.
    server_port = 17651
    log_path = Path(tempfile.gettempdir()) / "pong_stress.log"
    server_proc = subprocess.Popen(
        [str(binary), str(server_port), str(log_path)],
        cwd=str(ROOT / "server"),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    deadline = time.time() + 3
    while time.time() < deadline:
        if server_proc.poll() is not None:
            err = server_proc.stderr.read().decode("utf-8", errors="replace")
            print(f"El servidor no arranco:\n{err}", file=sys.stderr)
            return False
        try:
            probe = connect()
            probe.close()
            return True
        except OSError:
            time.sleep(0.05)
    print("El servidor no abrio el puerto a tiempo", file=sys.stderr)
    return False


def stop_server():
    if server_proc is None or server_proc.poll() is not None:
        return
    server_proc.send_signal(signal.SIGINT)
    try:
        server_proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        server_proc.kill()
        server_proc.wait(timeout=2)


def run_cases():
    for case_id, title, fn in CASOS:
        server_alive()
        try:
            fn()
        except Pending as exc:
            pending_cases.append((case_id, title, str(exc)))
            print(f"[pendiente] {case_id}  {title}")
        except (CaseError, OSError, socket.timeout, AssertionError) as exc:
            failed_cases.append((case_id, title, str(exc)))
            print(f"[FALLO]     {case_id}  {title}: {exc}")
            if server_proc.poll() is not None:
                print("El servidor murio. Se detienen las pruebas.")
                return
        else:
            ok_cases.append((case_id, title))
            print(f"[ok]        {case_id}  {title}")


def main():
    print("Arrancando el servidor...")
    if not start_server():
        return 1
    try:
        run_cases()
    finally:
        stop_server()

    print()
    print(f"ok: {len(ok_cases)}   pendiente: {len(pending_cases)}   fallo: {len(failed_cases)}")
    if pending_cases:
        print()
        print("Pendientes (hay que implementarlos en el servidor, estan descritos en docs/EDGE_CASES.md):")
        for case_id, title, detail in pending_cases:
            print(f"  - {case_id} {title}: {detail}")
    return 1 if failed_cases else 0


if __name__ == "__main__":
    sys.exit(main())
