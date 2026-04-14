import os
import sys
import tkinter as tk
import webbrowser
import platform
import subprocess
from gui import GameGui

def load_env():
    # Buscar .env en el directorio actual o en el padre
    env_paths = [".env", "../.env", os.path.join(os.path.dirname(__file__), "..", ".env")]
    for path in env_paths:
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        if "=" in line:
                            key, val = line.split("=", 1)
                            if key not in os.environ:
                                os.environ[key] = val
            break

if __name__ == "__main__":
    load_env()
    
    # Argumentos esperados: user room [host]
    if len(sys.argv) >= 3:
        user = sys.argv[1]
        room = sys.argv[2]
        host = sys.argv[3] if len(sys.argv) > 3 else os.environ.get("SERVER_HOST", "server.cdsp")
        
        print(f"Lanzando cliente: Usuario={user}, Sala={room}, Host={host}")
        root = tk.Tk()
        app = GameGui(root, username=user, room_id=room, host=host)
        root.mainloop()
    else:
        # Abrir portal web si no hay argumentos
        portal_url = "http://localhost:8000"
        print(f"Abriendo portal de juegos en: {portal_url}")
        
        try:
            # Detectar si estamos en WSL
            if "microsoft" in platform.uname().release.lower():
                subprocess.run(["powershell.exe", "/c", "start", portal_url], check=True)
            else:
                webbrowser.open(portal_url)
        except Exception:
            # Fallback al método estándar
            webbrowser.open(portal_url)

        print("\nPara lanzar manualmente use: python client.py <user> <room> <role> [host]")