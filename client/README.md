# Cliente Pong (Python + Pygame)

Cliente del juego Pong. Se conecta al servidor por TCP, se registra con el
protocolo binario `MyAppGameProtocol` y muestra el juego con una interfaz
gráfica hecha con **Pygame**.

## Estructura

```
client/
├── requirements.txt      # dependencias (pygame)
└── src/
    ├── protocol.py       # constantes del protocolo (coinciden con el servidor C)
    ├── network.py        # conexión TCP + envío/recepción de mensajes binarios
    ├── ui.py             # funciones de dibujo con Pygame
    └── main.py           # programa principal (registro + ventana del juego)
```

**Separación de capas (buen diseño):** `network.py`/`protocol.py` NO dependen de
Pygame (solo red); `ui.py`/`main.py` se encargan de la parte gráfica. Así la
lógica de red se puede probar sin interfaz.

## Requisitos

- Python 3.8 o superior
- Pygame (se instala con requirements.txt)

## Instalación

```bash
cd client
pip install -r requirements.txt
```

## Ejecución

```bash
cd client
python src/main.py <IP_SERVIDOR> <PUERTO>
# Ejemplo (servidor en la misma máquina):
python src/main.py 127.0.0.1 5000
```

Al iniciar se abre una ventana con la **pantalla de registro**:
- Escribe tu **nickname**, presiona **TAB** y escribe tu **email**.
- Presiona **ENTER** para registrarte contra el servidor.
- Si el registro es correcto, pasas a la pantalla de espera.

> Controles del juego (activos desde la Fase 6): **flecha arriba / flecha abajo**
> para mover la paleta.

## Estado actual (Fase 5)

- ✅ Conexión TCP al servidor.
- ✅ Registro (nickname + email) con el protocolo binario.
- ✅ Ventana base de Pygame (campo, paletas, pelota, marcador).
- ⏳ Movimiento y estado del juego en tiempo real: se implementa en la Fase 6.
