# Cómo jugar en red (servidor en AWS) — Guía rápida

Esta guía explica cómo conectarse a **jugar una partida 1vs1 con el servidor
desplegado en AWS**, desde cualquier sistema operativo. Para el manual detallado
de la interfaz del cliente, ver [`client/README.md`](../client/README.md).

> Para jugar en **local** (servidor en la misma máquina) se usa `127.0.0.1`
> como IP. Para jugar por **Internet**, se usa la **IP pública** de la instancia
> de AWS.

---

## Requisitos

- **Python 3.11 o 3.12** (recomendado; con 3.13+ pygame puede fallar al instalar).
- El **servidor corriendo** (en AWS o en una máquina accesible).
- La **IP y el puerto** del servidor. Ejemplo de esta guía: `34.229.123.245` y `5000`.

> ⚠️ La IP pública de AWS **puede cambiar cada vez que se reinicia el Lab**.
> Verifica la IP actual en EC2 → Instances → "Public IPv4 address" antes de jugar.

---

## Parte A — Arrancar el servidor en AWS (lo hace el anfitrión)

1. Entrar a **AWS Academy → Start Lab** y abrir la consola de **AWS**.
2. En **EC2 → Instances**, confirmar que la instancia está **Running** y copiar
   su **IP pública**.
3. Conectarse con **Connect → EC2 Instance Connect → Connect**.
4. En la terminal:
   ```bash
   cd proyecto-pong/server
   make                       # solo si no está compilado
   tmux new -s pong           # sesión que sobrevive al cerrar el navegador
   ./server 5000 pong.log
   # Ctrl+B y luego D para dejarlo corriendo en segundo plano
   ```
5. Debe mostrar: `Servidor escuchando. Esperando clientes (modo concurrente)...`

Para volver a ver el servidor: `tmux attach -t pong`.

---

## Parte B — Conectar un jugador (cada jugador en su computador)

Se necesitan **dos jugadores** para una partida. Cada uno repite estos pasos en
su propia máquina, usando la **misma IP y puerto** del servidor.

### 🍎 macOS

```bash
git clone https://github.com/jjpalacioz/proyecto-pong.git   # solo la 1ª vez
cd proyecto-pong/client
python3.12 -m venv venv
source venv/bin/activate
pip install pygame
python src/main.py 34.229.123.245 5000
```

### 🪟 Windows (PowerShell o CMD)

1. Instalar Python desde https://python.org/downloads marcando **"Add Python to PATH"**.
2. Luego:
```powershell
git clone https://github.com/jjpalacioz/proyecto-pong.git
cd proyecto-pong\client
python -m venv venv
venv\Scripts\activate
pip install pygame
python src\main.py 34.229.123.245 5000
```

### 🐧 Linux (Ubuntu/Debian)

```bash
sudo apt update && sudo apt install -y python3 python3-pip python3-venv git
git clone https://github.com/jjpalacioz/proyecto-pong.git
cd proyecto-pong/client
python3 -m venv venv
source venv/bin/activate
pip install pygame
python src/main.py 34.229.123.245 5000
```

### Diferencias entre sistemas

| Acción | macOS / Linux | Windows |
|--------|---------------|---------|
| Activar el entorno virtual | `source venv/bin/activate` | `venv\Scripts\activate` |
| Ejecutar el cliente | `python src/main.py ...` | `python src\main.py ...` |
| Separador de rutas | `/` | `\` |

> El cliente funciona igual en cualquier sistema operativo porque ambos extremos
> respetan el mismo **protocolo binario**; el lenguaje o el SO no importan.

---

## Parte C — Jugar

1. En la ventana del Pong: escribe **nickname** → `TAB` → **email** → `ENTER`.
2. Presiona `ENTER` otra vez para **buscar rival**.
3. El primero queda en "Buscando rival..."; cuando el segundo también busca, se
   emparejan y empieza la partida.
4. Mueve tu paleta con las **flechas ↑ / ↓**.
5. El **marcador se actualiza en tiempo real**. Gana quien llega a 5 puntos.

---

## Probar solo (sin otra persona)

Abre **dos terminales** en tu máquina y corre el cliente en cada una (con el
entorno virtual activo). Regístralas con nicknames distintos y presiona ENTER en
ambas: se emparejan entre ellas y verás una partida completa.

---

## Solución de problemas

| Síntoma | Causa | Solución |
|---------|-------|----------|
| "Connection refused" | El servidor no corre o el Lab se apagó | Vuelve a la Parte A |
| Se queda en "Buscando rival..." | Falta el segundo jugador | Que el otro presione ENTER |
| Falla al instalar pygame | Python 3.13+ (muy nuevo) | Usa Python 3.11 o 3.12 |
| "command not found: python" (Mac) | En Mac es `python3`/`pip3` o el venv | Activa el venv y usa `python` |
| La IP ya no funciona | El Lab reinició y cambió la IP | Verifica la IP nueva en EC2 y vuelve a conectar |
