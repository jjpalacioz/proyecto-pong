# Guía de despliegue en AWS Academy — PongServer

**Fase 9 · Infraestructura**
**Responsable:** Juan José Palacio

Esta guía explica cómo poner el servidor de Pong a correr en una instancia
EC2 de AWS Academy, para que los clientes se conecten por Internet (no solo
en la red local).

> Requisito del enunciado: "La aplicación PongServer se debe desplegar y
> ejecutar en un servidor en nube. Para esto utilice la cuenta de AWS academy."

---

## 1. Crear la instancia EC2

1. Entra a **AWS Academy → Learner Lab → Start Lab** (espera a que el punto
   quede verde) y abre la consola de **AWS**.
2. Ve al servicio **EC2 → Launch instance**.
3. Configura:
   - **Nombre:** `pong-server`
   - **AMI (sistema operativo):** Amazon Linux 2023 (o Ubuntu Server).
   - **Tipo de instancia:** `t2.micro` (suele estar en capa gratuita).
   - **Key pair:** crea una nueva (ej. `pong-key`) y **descarga el archivo
     `.pem`** — lo necesitas para conectarte por SSH.
4. En **Network settings → Security group**, crea reglas para permitir:
   - **SSH (puerto 22)** desde tu IP, para administrar.
   - **Custom TCP (el puerto de tu servidor, ej. 5000)** desde `0.0.0.0/0`,
     para que los clientes se conecten.
5. **Launch instance.**

> El **security group** es el "firewall" de AWS. Si no abres el puerto del
> servidor (5000), nadie de afuera podrá conectarse aunque el servidor esté
> corriendo.

---

## 2. Conectarse a la instancia por SSH

Copia la **IP pública** de la instancia desde la consola de EC2. Luego, desde
tu terminal (Linux/Mac/WSL):

```bash
chmod 400 pong-key.pem                       # permisos de la llave (una vez)
ssh -i pong-key.pem ec2-user@<IP_PUBLICA>    # Amazon Linux usa 'ec2-user'
# (en Ubuntu el usuario es 'ubuntu')
```

---

## 3. Instalar las herramientas y subir el código

En la instancia, instala el compilador y git:

```bash
sudo yum install -y gcc make git     # Amazon Linux
# (Ubuntu: sudo apt update && sudo apt install -y gcc make git)
```

Trae el código del repositorio:

```bash
git clone https://github.com/jjpalacioz/proyecto-pong.git
cd proyecto-pong/server
make
```

> Solo se compila el **servidor** en la nube. El cliente (Python) lo ejecuta
> cada jugador en su propio computador.

---

## 4. Ejecutar el servidor

```bash
./server 5000 pong.log
```

El servidor queda escuchando en el puerto 5000. Para que siga corriendo aunque
cierres la terminal SSH, puedes usar:

```bash
nohup ./server 5000 pong.log &      # queda en segundo plano
# o, mejor, una sesion de tmux:
tmux new -s pong                     # abre sesion
./server 5000 pong.log               # ejecuta
# Ctrl+B y luego D para "desconectarse" sin matar el servidor
```

---

## 5. Conectar los clientes

Cada jugador, desde su computador:

```bash
cd proyecto-pong/client
pip install -r requirements.txt
python src/main.py <IP_PUBLICA_DE_LA_INSTANCIA> 5000
```

Donde `<IP_PUBLICA_DE_LA_INSTANCIA>` es la IP pública de la EC2.

---

## 6. Verificación

- Si un cliente conecta y se registra, verás las líneas en el **log del
  servidor** (consola y `pong.log`), con la IP pública del cliente.
- Dos clientes conectados pueden emparejarse y jugar a través de Internet.

---

## 7. Problemas comunes

| Síntoma | Causa probable | Solución |
|---------|----------------|----------|
| El cliente no conecta | El puerto 5000 no está abierto | Revisa el security group (regla Custom TCP 5000) |
| "Connection refused" | El servidor no está corriendo | Verifica que `./server` siga vivo en la instancia |
| SSH rechaza la llave | Permisos del `.pem` | `chmod 400 pong-key.pem` |
| El Lab se apaga | AWS Academy tiene tiempo límite | Reinicia el Lab y vuelve a arrancar el servidor |

---

## Para sustentar

**¿Dónde corre el servidor?** En una instancia EC2 de AWS Academy, compilado
con gcc, escuchando en un puerto TCP abierto en el security group.

**¿Cómo se conecta el cliente?** A la IP pública de la instancia y al puerto
del servidor, usando el mismo protocolo binario que en local.

**¿Qué es el security group?** El firewall de AWS. Hay que abrir explícitamente
el puerto del servidor para que los clientes externos puedan conectarse.
