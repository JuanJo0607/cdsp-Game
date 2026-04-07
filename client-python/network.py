import os
import socket
import threading

class CDSPClient:
    def __init__(self, host: str, port: int, callback=None):
        self.host = host
        self.port = port
        self.callback = callback
        try:
            # Intentar resolver nombre via DNS UDP si termina en .cdsp
            resolved_host = self._resolve_name(host)

            info = socket.getaddrinfo(
                resolved_host, port, socket.AF_UNSPEC, socket.SOCK_STREAM
            )[0]
            self.sock = socket.socket(*info[:3])
            self.sock.connect(info[4])
            self.rfile = self.sock.makefile("r", encoding="utf-8")
            threading.Thread(target=self._recv_loop, daemon=True).start()
        except Exception as e:
            raise ConnectionError(f"No se pudo conectar al servidor: {e}")

    def _resolve_name(self, hostname: str) -> str:
        #Resuelve un nombre a una IP usando el DNS (UDP) en el puerto 5353.
        if not hostname.endswith(".cdsp"):
            return hostname

        print(f"DNS: Resolviendo {hostname} via UDP...")
        try:
            # Usamos una IP local de loopback para el DNS en este ejercicio
            dns_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            dns_sock.settimeout(2.0)
            # En un entorno real, la IP del DNS se obtendría por DHCP o config
            dns_host = os.environ.get("DNS_SERVER", "127.0.0.1")
            dns_sock.sendto(f"QUERY {hostname}".encode("utf-8"), (dns_host, 5353))

            data, _ = dns_sock.recvfrom(1024)
            response = data.decode("utf-8")
            if response.startswith("ANSWER"):
                parts = response.split(" ")
                if len(parts) >= 3 and parts[1] != "NOT_FOUND":
                    ip = parts[2]
                    print(f"DNS: {hostname} resuelto a {ip}")
                    return ip
        except Exception as e:
            print(f"DNS WARNING: Falló resolución UDP ({e}). Usando nombre directo.")

        return hostname  # Fallback

    def send(self, msg: str):
        print(f"CLIENT -> SERVER: {msg}")
        self.sock.sendall((msg + "\n").encode("utf-8"))

    def _recv_loop(self):
        try:
            for line in self.rfile:
                line = line.rstrip("\n")
                print(f"SERVER -> CLIENT: {line}")
                if self.callback:
                    self.callback(line)
        except Exception:
            if self.callback:
                self.callback('ERR 0 "Conexión perdida"')
