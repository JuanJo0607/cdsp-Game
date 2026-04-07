# CDSP Game — Cyber Defense Simulation Protocol

Juego multijugador de simulación de ciberseguridad.  
Internet: Arquitectura y Protocolos · 2026-1

---

## Estructura del proyecto (En progreso)

```
cdsp-game/
├── server/
│   ├── main.c
│   ├── protocol.c
│   ├── protocol.h
│   ├── game.c
│   ├── game.h
│   └── Makefile
├── client-python/
│   └── client.py
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

Reinicia el equipo si te lo piden.

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
git clone https://github.com/JuanJo0607/cdsp-Game.git
cd cdsp-Game
```
### Bajar todas las ramas del repositorio al local
```bash
git branch -r | grep -v '\->' | grep -v 'HEAD' | while read remote; do git checkout --track "$remote"; done
```
Ahora verifica si todas las ramas del repositorio se bajaron correctamente:
```bash
git branch 
```
Si todo salió bien, deberás ver algo como lo siguiente:
```bash
  auth-cliente-c
  cliente-python
  dev
* main
  server
```
---

## Compilar el servidor

```bash
cd server
make
```

Si todo está bien verás algo como:

```
gcc -Wall -pthread -o server main.c protocol.c game.c
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

Abre **otra terminal** y conéctate con:

```bash
nc localhost 8080
```

Una vez conectado puedes enviar comandos del protocolo CDSP uno por uno:

```
AUTH juanito
CREATE_ROOM
LIST_ROOMS
JOIN room_001 atacante
QUIT
```

Respuestas esperadas:

```
OK AUTH "juanito" ROLE=atacante
OK CREATE_ROOM room_001
OK LIST_ROOMS 1 room_001
OK JOIN room_001 ROLE=atacante POS=0,0
OK QUIT
```

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
→ El puerto ya está en uso. Cambia el puerto o espera un momento:

```bash
./server 8081 logs.txt
```

**`nc: command not found`**  
→ Instala netcat: `sudo apt install netcat -y`

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

## Reglas del Juego 


### 1. Ataque y Mitigación (Temporizador de 30s)
- **Ataque**: Una vez que un atacante llega a la celda de un servidor y usa `ATTACK`, comienza una cuenta atrás de **30 segundos**.
- **Mitigación**: El defensor recibe una notificación y debe llegar a la celda para usar `MITIGATE` antes de que se acabe el tiempo.
- **Compromiso**: Si pasan los 30 segundos sin mitigación, el recurso se pierde (se vuelve negro en el mapa).

### 2. Condición de Victoria
- **Atacante**: Gana si logra comprometer los 2 servidores del sistema.
- **Defensor**: Gana si logra evitar que comprometan los servidores durante **5 minutos** (Tiempo global de partida).


---

##  Notas de Desarrollo (Provisional)

> [!NOTE]
> Algunos comportamientos son **temporales**:
> 
> **Autenticación con prefijos**: El uso de `atacante_` o `defensor_` para asignar roles en el nombre del usuario es una medida provisional. En la versión final, el rol se recibirá directamente desde el servicio de identidad externo.


