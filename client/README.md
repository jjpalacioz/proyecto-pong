# Cómo jugar — cliente Pong

El cliente es la ventana del juego. Se conecta al servidor por TCP, te registra
y dibuja la partida. No calcula la pelota ni el marcador: eso lo hace el servidor.

Hacen falta **dos ventanas** (dos jugadores) apuntando al mismo servidor para
una partida 1 contra 1. Con una sola, la ventana se queda en "Buscando rival...".

## Qué necesitas

- Python 3.8 o superior
- El servidor ya corriendo (`./server <PUERTO> <archivo.log>` desde `server/`)

## Instalar

En PowerShell, desde la carpeta del proyecto:

```powershell
cd client
pip install -r requirements.txt
```

## Abrir la ventana

```powershell
python src/main.py <IP_SERVIDOR> <PUERTO>
```

Si el servidor está en esta misma máquina:

```powershell
python src/main.py 127.0.0.1 5000
```

El servidor en C se arranca en WSL. La ventana se abre en Windows y se conecta
a `127.0.0.1` y al puerto que hayas puesto al lanzar `./server`.

## Cómo entrar a la partida

1. Escribe un nickname.
2. Pulsa **TAB** y escribe un email.
3. Pulsa **ENTER** para registrarte.
4. Cuando diga que ya estás registrado, pulsa **ENTER** otra vez.
5. La ventana muestra **Buscando rival...** hasta que otra persona hace lo mismo.
6. Al encontrar rival verás su nickname y si juegas a la izquierda o a la derecha.
7. La partida empieza sola.

Cada jugador debe usar un nickname distinto. Si el segundo repite el primero,
el servidor lo rechaza.

## Controles

| Tecla | Qué hace |
|-------|----------|
| TAB | Cambia entre nickname y email |
| ENTER | Registrarse, y luego buscar rival |
| Flecha arriba | Sube tu paleta |
| Flecha abajo | Baja tu paleta |

La paleta izquierda es del primer jugador que entró a la cola. La derecha es
del segundo. Durante la partida, arriba a la izquierda dice contra quién juegas
y en el centro está el marcador.

## Cuando termina

La ventana deja de mostrar el campo y dice **Ganaste** o **Perdiste**, con el
marcador final (`izquierda - derecha`). Gana quien llega primero a 5 puntos.
Si el rival cierra su ventana a mitad, el servidor te da la victoria y ves
esa misma pantalla.

Si se cae la conexión o el servidor manda un error (nickname ocupado, servidor
lleno, mensaje inválido), la ventana muestra el texto en rojo en lugar de
quedarse congelada. Para salir, cierra la ventana.
