# CDSP Game — Cyber Defense Simulation Protocol

Juego multijugador de simulación de ciberseguridad.  
Internet: Arquitectura y Protocolos · 2026-1

---

## Estructura del proyecto (En progreso)

```
cdsp-game/
├── server/           
│   ├── main.c
│   └── Makefile
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
