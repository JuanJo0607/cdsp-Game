# CDSP Game — Cyber Defense Simulation Protocol

Juego multijugador distribuido para simulación de ciberseguridad, desarrollado como parte del curso **Internet: Arquitectura y Protocolos (2026-1)**.

El sistema simula un entorno de red donde múltiples jugadores interactúan en tiempo real asumiendo roles de **Atacante** o **Defensor**, utilizando protocolos de comunicación diseñados e implementados sobre sockets.

---

## Descripción del sistema

Este proyecto implementa una arquitectura distribuida compuesta por:

- Un **servidor principal (C)** que gestiona la lógica del juego
- Un **servicio de autenticación (Python)** independiente
- Clientes en múltiples lenguajes (**Python y C**)
- Resolución de nombres (simulación tipo DNS)

El sistema cumple con principios reales de Internet:

- No se usan direcciones IP hardcodeadas  
- La autenticación está desacoplada  
- Soporta múltiples clientes concurrentes  
- Comunicación basada en un protocolo propio  

---

## Estructura del proyecto (En progreso)

```
cdsp-game/
├── server/ # Servidor principal (C)
│ ├── main.c
│ ├── game.c / game.h
│ ├── protocol.c / protocol.h
│ ├── dns_server.c / dns_server.h
│ ├── Makefile
│ └── logs.txt
│
├── client-python/ # Cliente en Python
│ ├── client.py
│ ├── network.py
│ └── gui.py
│
├── auth-server/ # Servicio de autenticación
│ ├── auth_server.py
│ └── users.json
│
├── auth-cliente-c/ # Cliente en C
│ └── client.c
│
├── .env
├── .env.example
├── run.sh
└── README.md
```

---

## Requisitos previos

### Ubuntu / Linux

```bash
sudo apt update
sudo apt install gcc make -y
```

Verificar instalación:

```bash
gcc --version
make --version
```

### Windows

En Windows necesitas instalar WSL (Windows Subsystem for Linux), que te da una terminal Ubuntu dentro de Windows. Es la forma más simple y recomendada.

**Paso 1 — Activar WSL**

Abre PowerShell como administrador y ejecuta:

```powershell
wsl --install
```

Reinicia el equipo cuando te lo pida.

**Paso 2 — Abrir Ubuntu**

Busca "Ubuntu" en el menú de inicio. La primera vez te pedirá crear un usuario y contraseña.

**Paso 3 — Instalar gcc y make dentro de Ubuntu (WSL)**

```bash
sudo apt update
sudo apt install gcc make -y
```

Verificar:

```bash
gcc --version
make --version
```

> A partir de aquí todos los comandos son iguales para Ubuntu y WSL.

---

## Clonar el repositorio

```bash
git clone https://github.com/TU_USUARIO/cdsp-game.git
cd cdsp-game
```

> Reemplaza `TU_USUARIO` con el usuario de GitHub del equipo.

---

## Compilar el servidor

```bash
cd server
make
```

Si todo está bien verás algo como:

```
gcc -Wall -pthread -o server main.c
```

Para limpiar los archivos compilados:

```bash
make clean
```

---

## Correr el servidor

```bash
./server <puerto> <archivoDeLogs>
```

Ejemplo:

```bash
./server 8080 logs.txt
```

Deberías ver:

```
Servidor escuchando en puerto 8080...
```

---

## Probar el servidor

Abre **otra terminal** (o pestaña) y ejecuta:

```bash
echo "Hola Servidor | nc localhost 8080
```

En la terminal del servidor deberías ver el mensaje recibido.  
En esta terminal deberías ver la respuesta del servidor.

---

## Ver los logs

Los logs se guardan en el archivo que pasaste como segundo argumento:

```bash
cat logs.txt
```

---

## Problemas comunes

**`make: command not found`**  
→ Ejecuta `sudo apt install make -y`

**`bind: Address already in use`**  
→ El puerto ya está en uso. Cambia el puerto o espera un momento y vuelve a intentarlo:
```bash
./server 8081 logs.txt
```

**`nc: command not found`** (para las pruebas)  
→ Instala netcat: `sudo apt install netcat -y`

---

## Micro-Servicio DNS (UDP)

Para cumplir con el **Requerimiento 3** (sin IPs hardcodeadas), hemos implementado un mini-servidor DNS que resuelve nombres como `server.cdsp`.

### Cómo usar el DNS
1. Compila los servidores: `cd server && make`
2. Ejecuta el servidor DNS en una terminal:
   ```bash
   ./dns_server 5354
   ```
3. Ejecuta el servidor de juego en otra terminal:
   ```bash
   ./server 8080 logs.txt
   ```
4. En el cliente Python, usa `server.cdsp` en el campo "Server Host". El cliente resolverá automáticamente la IP vía UDP antes de conectar por TCP.

---

## Cliente Gráfico (Python)

El proyecto incluye un cliente con interfaz gráfica para una experiencia de juego completa.

### Requisitos
- Python 3.x
- Tkinter (usualmente incluido en Python)
  
```bash
sudo apt install python3-tk
```


### Cómo ejecutar el cliente
1. Asegúrate de que el servidor esté corriendo en WSL/Linux.
2. En Linux (o Windows), abre una terminal y navega a la carpeta del proyecto.
3. Ejecuta:

   ```bash
   python client-python/client.py
   ```

---
### Condición de Victoria
- **Atacante**: Gana si logra comprometer los 2 servidores del sistema.
- **Defensor**: Gana si logra evitar que comprometan los servidores durante **5 minutos** (Tiempo global de partida).


---


