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
    echo "📦 Cargando .env..."
    set -a
    source .env
    set +a
else
    echo "⚠️ .env no encontrado, usando defaults"
fi

echo "DEBUG PORT=$SERVER_PORT"
echo "DEBUG LOG=$SERVER_LOG_FILE"

# ============================
# COMPILAR SERVER
# ============================
echo "🔧 Compilando server..."
make -C server

if [ $? -ne 0 ]; then
    echo "❌ Error en compilación"
    exit 1
fi

# ============================
# INICIAR DNS
# ============================
echo "🌐 Iniciando DNS..."
./server/dns_server &
DNS_PID=$!

# ============================
# ESPERAR DNS LISTO 
# ============================
echo "⏳ Esperando DNS listo..."

while ! (echo "" > /dev/udp/127.0.0.1/5353) 2>/dev/null; do
    sleep 0.2
done

echo "✅ DNS listo"

# ============================
# CLEANUP AUTOMÁTICO
# ============================
trap "echo '🧹 Cerrando DNS...'; kill $DNS_PID 2>/dev/null" EXIT

# ============================
# INICIAR SERVER PRINCIPAL
# ============================
echo "🕹️ Iniciando servidor principal..."
./server/server