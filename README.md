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
*(Ver [`docs/PROTOCOL.md`](docs/PROTOCOL.md))*

### 2.3 Servidor (C)
*(Pendiente)*

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
