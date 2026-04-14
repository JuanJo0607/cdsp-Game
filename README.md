Readme · MD
Copy

# CDSP Game — Cyber Defense Simulation Protocol
 
Juego multijugador distribuido para simulación de ciberseguridad, desarrollado como parte del curso **Internet: Arquitectura y Protocolos (2026-1)**.
 
El sistema simula un entorno de red donde múltiples jugadores interactúan en tiempo real asumiendo roles de **Atacante** o **Defensor**, utilizando protocolos de comunicación diseñados e implementados sobre sockets.
 
---
 
## Descripción del sistema
 
Este proyecto implementa una arquitectura distribuida compuesta por:
 
- Un **servidor principal (C)** que gestiona la lógica del juego
- Un **servicio de autenticación (Python)** independiente
- Clientes en múltiples lenguajes (**Python y C**)
- Resolución de nombres mediante un mini-servidor DNS propio
 
El sistema cumple con principios reales de Internet:
 
- No se usan direcciones IP hardcodeadas
- La autenticación está desacoplada del servidor de juego
- Soporta múltiples clientes concurrentes
- Comunicación basada en un protocolo propio (CDSP)
 
---
 
## Estructura del proyecto
 
```
cdsp-game/
├── server/
│   ├── main.c
│   ├── game.c / game.h
│   ├── protocol.c / protocol.h
│   ├── dns_server.c / dns_server.h
│   └── Makefile
├── client-python/
│   ├── client.py
│   ├── network.py
│   └── gui.py
├── auth-server/
│   ├── auth_server.py
│   └── users.json
├── auth-cliente-c/
│   └── client.c
├── .env.example
├── run.sh
└── README.md
```
 
---
 
## Requisitos previos
 
### Ubuntu / Linux
 
```bash
sudo apt update
sudo apt install gcc make python3 python3-tk -y
```
 
Verificar instalación:
 
```bash
gcc --version
make --version
python3 --version
```
 
### Windows
 
En Windows necesitas instalar WSL (Windows Subsystem for Linux), que te da una terminal Ubuntu dentro de Windows.
 
**Paso 1 — Activar WSL**
 
Abre PowerShell como administrador y ejecuta:
 
```powershell
wsl --install
```
 
Reinicia el equipo cuando te lo pida.
 
**Paso 2 — Abrir Ubuntu**
 
Busca "Ubuntu" en el menú de inicio. La primera vez te pedirá crear un usuario y contraseña.
 
**Paso 3 — Instalar dependencias dentro de Ubuntu (WSL)**
 
```bash
sudo apt update
sudo apt install gcc make python3 python3-tk -y
```
 
> A partir de aquí todos los comandos son iguales para Ubuntu y WSL.
 
---
 
## Configuración inicial
 
**1. Clonar el repositorio:**
 
```bash
git clone https://github.com/JuanJo0607/cdsp-Game.git
cd cdsp-Game
```
 
**2. Crear el archivo `.env`:**
 
```bash
cp .env.example .env
```
 
**3. Dar permisos al script de arranque:**
 
```bash
chmod +x run.sh
```
 
---
 
## Cómo ejecutar el juego completo
 
El juego requiere tres componentes corriendo al mismo tiempo. Abre **tres terminales**.
 
**Terminal 1 — Auth server:**
```bash
cd auth-server
python3 auth_server.py
```
 
Deberías ver:
```
Auth server iniciado en puerto 9090
Usuarios cargados: ['andres', 'juan']
```
 
**Terminal 2 — DNS + Servidor principal:**
```bash
./run.sh
```
 
Deberías ver:
```
DNS: Servidor escuchando en puerto 5354
DNS: Registro exitoso
Servidor escuchando en puerto 8080...
```
 
**Terminal 3 — Cliente Python (repetir para cada jugador):**
```bash
python3 client-python/client.py
```
 
Se abrirá una ventana gráfica. Ingresa un usuario y haz clic en **Conectar**.
 
---
 
## Usuarios disponibles
 
| Usuario | Rol |
|---------|-----|
| `juan` | Atacante |
| `andres` | Defensor |
 
---
 
## Flujo del juego
 
1. El **atacante** se conecta, crea una sala y entra
2. El **defensor** se conecta, lista las salas y entra a la misma sala
3. El **atacante** explora el plano usando las flechas de movimiento
4. Cuando el **atacante** esté cerca de un recurso, usa **SCAN** para detectarlo
5. El **atacante** se mueve a la celda exacta del recurso y presiona **ATTACK**
6. El **defensor** recibe una notificación de ataque — el recurso se marca en amarillo
7. El **defensor** se mueve a la celda del recurso y presiona **MITIGATE**
8. El recurso se marca en azul — ataque mitigado
 
### Recursos críticos en el plano
 
| Recurso | Posición |
|---------|----------|
| `srv_01` | 5, 5 |
| `srv_02` | 15, 15 |
 
### Condición de victoria
 
- **Atacante gana** si compromete los 2 servidores sin que sean mitigados
- **Defensor gana** si mitiga todos los ataques
 
---
 
## Compilar el servidor manualmente
 
Si necesitas recompilar el servidor sin usar `run.sh`:
 
```bash
cd server
make clean
make
```
 
---
 
## Probar el servidor con netcat
 
Para pruebas rápidas sin la GUI:
 
```bash
nc localhost 8080
```
 
Comandos disponibles:
```
AUTH juan
CREATE_ROOM
LIST_ROOMS
JOIN room_001 attacker
MOVE 1 0
SCAN
ATTACK srv_01
QUIT
```
 
---
 
## Ver los logs
 
```bash
cat server/logs.txt
```
 
---
 
## Problemas comunes
 
**`make: command not found`**
→ Ejecuta `sudo apt install make -y`
 
**`bind: Address already in use`**
→ El puerto ya está en uso. Espera unos segundos y vuelve a intentarlo, o cambia el puerto en `.env`
 
**`nc: command not found`**
→ Instala netcat: `sudo apt install netcat -y`
 
**El cliente dice `DNS WARNING: Falló resolución UDP`**
→ El DNS no está corriendo. Usa `./run.sh` en vez de arrancar el servidor manualmente
 
**El defensor no ve las salas al listar**
→ Asegúrate de que el atacante ya haya creado la sala y entrado a ella antes de que el defensor liste
