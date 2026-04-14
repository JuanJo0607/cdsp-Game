#!/bin/bash

echo "=============================="
echo "   CDSP Game - Launcher"
echo "=============================="

# ============================
# CARGAR .env
# ============================

if [ -f .env ]; then
    sed -i 's/\r$//' .env   
fi

if [ -f .env ]; then
    echo " Cargando .env..."
    set -a
    source .env
    set +a
else
    echo " .env no encontrado, usando defaults"
fi

echo "DEBUG PORT=$SERVER_PORT"
echo "DEBUG LOG=$SERVER_LOG_FILE"

# ============================
# COMPILAR SERVER
# ============================
echo " Compilando server..."
make -C server

if [ $? -ne 0 ]; then
    echo " Error en compilación"
    exit 1
fi

# ============================
# INICIAR DNS
# ============================
echo " Iniciando DNS..."
./server/dns_server &
DNS_PID=$!

# ============================
# INICIAR AUTH SERVER
# ============================
echo " Iniciando Auth Server..."
python3 auth-server/auth_server.py &
AUTH_PID=$!

# ============================
# INICIAR HTTP GATEWAY (Req 1)
# ============================
echo " Iniciando HTTP Gateway..."
python3 http-server/web_server.py &
HTTP_PID=$!


# ============================
# ESPERAR DNS LISTO 
# ============================
echo " Esperando DNS listo..."

while ! (echo "" > /dev/udp/127.0.0.1/5353) 2>/dev/null; do
    sleep 0.2
done

echo " DNS listo"

echo " Esperando Auth Server listo..."
while ! (echo "" > /dev/tcp/127.0.0.1/9090) 2>/dev/null; do
    sleep 0.2
done
echo " Auth Server listo"


# ============================
# CLEANUP AUTOMÁTICO
# ============================
trap "echo ' Cerrando procesos...'; kill $DNS_PID $AUTH_PID $HTTP_PID 2>/dev/null" EXIT

# ============================
# INICIAR SERVER PRINCIPAL
# ============================
echo " Iniciando servidor principal..."
./server/server ${SERVER_PORT} ${SERVER_LOG_FILE}