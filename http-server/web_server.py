import socket
import os

# Cargar variables de entorno si existen
def load_env():
    env_vars = {}
    paths = [".env", "../.env"]
    for path in paths:
        if os.path.exists(path):
            with open(path, "r") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#") and "=" in line:
                        k, v = line.split("=", 1)
                        env_vars[k] = v
            break
    return env_vars

ENV = load_env()
GAME_SERVER_HOST = ENV.get("SERVER_HOST", "server.cdsp")
GAME_SERVER_PORT = int(ENV.get("SERVER_PORT", 8080))
HTTP_PORT = 8000

def resolve_name(hostname, default_port):
    if not hostname.endswith(".cdsp"):
        return hostname, default_port
    
    print(f"[DNS] Resolviendo {hostname}...")
    try:
        dns_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        dns_sock.settimeout(2.0)
        dns_host = os.environ.get("DNS_SERVER", "127.0.0.1")
        dns_port = int(os.environ.get("DNS_PORT", 5353))
        dns_sock.sendto(f"QUERY {hostname}".encode("utf-8"), (dns_host, dns_port))
        
        data, _ = dns_sock.recvfrom(1024)
        response = data.decode("utf-8")
        if response.startswith("ANSWER"):
            parts = response.split(" ")
            if len(parts) >= 4 and parts[1] != "NOT_FOUND":
                return parts[2], int(parts[3])
    except Exception as e:
        print(f"[DNS ERROR] {e}")
    return hostname, default_port

def get_rooms_from_server():
    try:
        target_ip, target_port = resolve_name(GAME_SERVER_HOST, GAME_SERVER_PORT)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect((target_ip, target_port))
        s.sendall(b"LIST_ROOMS\n")
        data = s.recv(4096).decode("utf-8")
        s.close()
        if data.startswith("OK LIST_ROOMS"):
            parts = data.split(" ")
            if len(parts) >= 4: 
                return parts[3:]
        return []
    except Exception as e:
        print(f"Error LIST_ROOMS: {e}")
        return []

def create_room_on_server():
    try:
        target_ip, target_port = resolve_name(GAME_SERVER_HOST, GAME_SERVER_PORT)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect((target_ip, target_port))
        s.sendall(b"CREATE_ROOM\n")
        data = s.recv(1024).decode("utf-8")
        s.close()
        return data.strip()
    except Exception as e:
        return f"ERR {e}"

def handle_request(client_socket, addr):
    try:
        request = client_socket.recv(4096).decode("utf-8")
        if not request: 
            return

        lines = request.split("\r\n")
        if not lines: 
            return
            
        request_line = lines[0]
        # Cumplimiento Req 1 & 5: Interpretar cabeceras IP (Origen)
        print(f"\n[HTTP] [{addr[0]}:{addr[1]}] Petición: {request_line}")
        
        headers = {}
        for line in lines[1:]:
            if line == "": 
                break
            if ": " in line:
                k, v = line.split(": ", 1)
                headers[k] = v
        
        # Log de cabeceras interpretadas
        print(f"[HTTP] Headers: Host={headers.get('Host')}, CL={headers.get('Content-Length', 0)}")

        parts = request_line.split(" ")
        if len(parts) < 3: 
            return
        method, full_path, _ = parts[0], parts[1], parts[2]

        # Validar método: Solo GET soportado por ahora (Req 1: Códigos adecuados)
        if method != "GET":
            send_response(client_socket, 405, "<h1>405 Method Not Allowed</h1>")
            return

        query_params = {}
        path = full_path
        if "?" in full_path:
            path, query_str = full_path.split("?", 1)
            for pair in query_str.split("&"):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    query_params[k] = v

        if path == "/":
            rooms = get_rooms_from_server()
            html = "<html><body><h1>Lobby CDSP</h1><a href='/create'>[+ Crear Sala]</a><hr><h3>Salas:</h3>"
            if not rooms:
                html += "<p>Vacio</p>"
            else:
                html += "<ul>"
                for r in rooms:
                    if r.strip():
                        html += f"<li>{r} - <a href='/join?room={r}'>Unirse</a></li>"
                html += "</ul>"
            html += "</body></html>"
            send_response(client_socket, 200, html)

        elif path == "/create":
            create_room_on_server()
            # Redirect back to home
            response = "HTTP/1.1 302 Found\r\nLocation: /\r\nConnection: close\r\n\r\n"
            client_socket.sendall(response.encode("utf-8"))

        elif path == "/join":
            room = query_params.get("room", "unknown")
            html = f"<html><body><h2>Sala: {room}</h2><form action='/launch'><input type='hidden' name='room' value='{room}'><input type='text' name='user' placeholder='User' required><button>Entrar</button></form></body></html>"
            send_response(client_socket, 200, html)

        elif path == "/launch":
            room, user = query_params.get("room"), query_params.get("user")
            cp = f"python client-python/client.py {user} {room} {GAME_SERVER_HOST}"
            cc = f"./auth-cliente-c/cliente {GAME_SERVER_HOST} 8080 {user} {room}"
            html = f"<html><body><h2>OK</h2><b>Python:</b><br><code>{cp}</code><br><br><b>C Client:</b><br><code>{cc}</code><br><br><a href='/'>Menu</a></body></html>"
            send_response(client_socket, 200, html)
        else:
            send_response(client_socket, 404, "<h1>404 Not Found</h1>")

    except Exception as e:
        print(f"Error Request: {e}")
    finally:
        client_socket.close()

def send_response(sock, status, body):
    status_text = {200: "OK", 404: "Not Found", 302: "Found", 405: "Method Not Allowed"}.get(status, "OK")
    resp = f"HTTP/1.1 {status} {status_text}\r\n"
    resp += "Content-Type: text/html\r\n"
    resp += f"Content-Length: {len(body.encode('utf-8'))}\r\n"
    resp += "Connection: close\r\n\r\n"
    sock.sendall(resp.encode("utf-8") + body.encode("utf-8"))

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(("0.0.0.0", HTTP_PORT))
        server.listen(5)
        print(f"Gateway HTTP iniciado en puerto {HTTP_PORT}...")
        print(f"Abra http://localhost:{HTTP_PORT} en su navegador.")
        
        while True:
            client, addr = server.accept()
            handle_request(client, addr)
    except KeyboardInterrupt:
        print("\nCerrando pasarela HTTP...")
    finally:
        server.close()

if __name__ == "__main__":
    main()
