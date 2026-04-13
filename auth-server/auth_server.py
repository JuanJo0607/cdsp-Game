import socket
import json
import os

# Cargar usuarios desde users.json
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
USERS_FILE = os.path.join(BASE_DIR, "users.json")

def cargar_usuarios():
    with open(USERS_FILE, "r") as f:
        return json.load(f)

def manejar_cliente(conn, addr, usuarios):
    print(f"Conexión de {addr}")
    try:
        data = conn.recv(1024).decode().strip()
        print(f"Consultando usuario: {data}")

        if data in usuarios:
            rol = usuarios[data]["role"]
            respuesta = f"OK {rol}\n"
        else:
            respuesta = "ERR usuario no encontrado\n"

        conn.send(respuesta.encode())
        print(f"Respuesta: {respuesta.strip()}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        conn.close()

def main():
    HOST = "0.0.0.0"
    PORT = 9090

    usuarios = cargar_usuarios()
    print(f"Auth server iniciado en puerto {PORT}")
    print(f"Usuarios cargados: {list(usuarios.keys())}")

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(5)

    while True:
        conn, addr = server.accept()
        manejar_cliente(conn, addr, usuarios)

if __name__ == "__main__":
    main()