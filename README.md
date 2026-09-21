# 🏓 Proyecto Pong — Juego en Red (Cliente/Servidor)

**Curso:** Internet: Arquitectura y Protocolos (Telemática)
**Proyecto N°1 — Pong**
**Fecha de entrega:** 22 de octubre de 2026

## 👥 Integrantes

| Nombre | Rol principal |
|--------|---------------|
| Juan José Palacio | Líder técnico · Servidor C (núcleo, concurrencia, lógica del juego) |
| Alejandro Correa | Protocolo binario · Serialización · Robustez · UML |
| Samuel Granados | Cliente Python/Pygame · GUI · Score en tiempo real · Logger |

---

## 1. Introducción

Este proyecto implementa el clásico juego **Pong** en una arquitectura
**cliente/servidor** sobre la pila de protocolos **TCP/IP**, usando la
**API de Sockets Berkeley** directamente (sin librerías de sockets de alto nivel).

- El **servidor** está escrito en **C** y es el responsable de mantener el
  *estado* del juego: física de la pelota, colisiones, puntaje y validación
  de todos los movimientos. Soporta **múltiples parejas de jugadores de forma
  concurrente** mediante hilos.
- El **cliente** está escrito en **Python** con **Pygame**. Solo se encarga de
  capturar la entrada del usuario (teclado), enviarla al servidor y **dibujar**
  el estado que el servidor le envía (incluyendo el **score en tiempo real**).
- La comunicación se realiza mediante un **protocolo propio de aplicación,
  codificado de forma binaria** (no texto), llamado `MyAppGameProtocol`.

> **Regla clave de la arquitectura:** ningún cliente se comunica directamente
> con otro cliente. **Todo pasa por el servidor.**

## 2. Desarrollo

> Esta sección se irá completando fase por fase a medida que avanza el proyecto.

### 2.1 Arquitectura general
*(Pendiente — se documenta en la Fase 1)*

### 2.2 Diseño del protocolo binario (`MyAppGameProtocol`)

El protocolo de aplicación propio, **codificado en binario**, está completamente
especificado en [`docs/PROTOCOL.md`](docs/PROTOCOL.md). Incluye:

- **Header de tamaño fijo (6 bytes)** con `MAGIC`, `VERSION`, `TYPE` y `LENGTH`,
  que resuelve el *framing* sobre el flujo de bytes de TCP.
- **Vocabulario de 13 tipos de mensaje** (registro, emparejamiento, input, estado,
  eventos de juego, ping/pong y errores).
- **Formato byte a byte del payload** de cada mensaje.
- **Máquina de estados** del cliente (reglas de procedimiento).
- **Tabla de códigos de error** para casos anómalos.

Los valores numéricos (constantes) están centralizados en
[`common/protocol_spec.md`](common/protocol_spec.md) como fuente única de verdad
para el servidor (C) y el cliente (Python).

### 2.3 Servidor (C)

El servidor está escrito en C usando la **API de Sockets Berkeley** directamente
(sin librerías de sockets de alto nivel). Estructura:

```
server/
├── include/
│   ├── protocol.h   # constantes del protocolo (magic, tipos, errores, params)
│   └── logger.h     # API del sistema de logging
├── src/
│   ├── server.c     # programa principal: socket → bind → listen → accept
│   └── logger.c     # implementación del logger (consola + archivo)
└── Makefile         # compilación con gcc
```

**Compilar y ejecutar:**
```bash
cd server
make
./server <PORT> <LogFile>     # ej: ./server 5000 pong.log
```

**Flujo del servidor:** valida los argumentos → inicializa el logger →
crea el socket TCP (`socket`) → lo asocia al puerto (`bind`) → escucha
(`listen`) → acepta un cliente (`accept`) → **procesa el registro del cliente
según el protocolo** → cierra ordenadamente. Cada evento se registra en consola
y en el archivo de log.

**Módulos del servidor:**
- `net.c/.h` — `recv_all`/`send_all`: leen/escriben exactamente N bytes sobre
  TCP (resuelven las lecturas/escrituras parciales del stream).
- `protocol_io.c/.h` — `recv_message`/`send_message`: implementan el *framing*
  (header de 6 bytes + payload), validando `MAGIC`, `VERSION` y `LENGTH`.
- `player.h` — estructura del perfil del jugador (id, nickname, email).

**Registro (Fase 3):** el servidor lee un `MSG_REGISTER`, valida y parsea el
nickname y el email del payload binario, asigna un `player_id` y responde
`MSG_REGISTER_OK`. Ante cualquier anomalía (MAGIC/versión/longitud inválidos,
campos vacíos, tipo inesperado o desconexión abrupta) responde con `MSG_ERROR`
y el código adecuado, **sin caerse**.

> El **logger** cumple el requisito del enunciado: imprime por la terminal todas
> las peticiones/respuestas y las escribe también en el `<LogFile>`, con
> timestamp y nivel de severidad. Su escritura está protegida con un *mutex*
> para soportar la concurrencia que se añade en la Fase 4.

### 2.4 Cliente (Python + Pygame)
*(Pendiente)*

### 2.5 Concurrencia
*(Pendiente)*

### 2.6 Manejo de errores y casos límite
*(Pendiente)*

### 2.7 Despliegue en AWS Academy
*(Pendiente)*

## 3. Conclusiones
*(Pendiente — se completa al final del proyecto)*

## 4. Referencias

- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
- Beej's Guide to C — https://beej.us/guide/bgc/
- TCP Server-Client en C (GeeksforGeeks) — https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
- Pong (Wikipedia) — https://en.wikipedia.org/wiki/Pong

---

## 🚀 Cómo ejecutar (referencia rápida)

### Servidor (C)
```bash
cd server
make
./server <PORT> <LogFile>
# Ejemplo:
./server 5000 pong.log
```

### Cliente (Python)
```bash
cd client
pip install -r requirements.txt
python src/main.py <IP_SERVIDOR> <PORT>
# Ejemplo:
python src/main.py 127.0.0.1 5000
```
