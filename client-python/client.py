import os
import tkinter as tk
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
    root = tk.Tk()
    app = GameGui(root)
    root.mainloop()