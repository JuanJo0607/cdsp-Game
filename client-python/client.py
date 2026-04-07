import socket
import threading
import tkinter as tk
import random
from tkinter import messagebox, scrolledtext

PLAYER_COLORS = ["#FF5733", "#33FF57", "#3357FF", "#F333FF", "#33FFF3", "#FFF333", "#8D33FF", "#FF8D33", "#FF338D"]

class CDSPClient:
    def __init__(self, host: str, port: int, callback=None):
        self.host = host
        self.port = port
        self.callback = callback
        try:
            info = socket.getaddrinfo(host, port, socket.AF_UNSPEC, socket.SOCK_STREAM)[0]
            self.sock = socket.socket(*info[:3])
            self.sock.connect(info[4])
            self.rfile = self.sock.makefile('r', encoding='utf-8')
            threading.Thread(target=self._recv_loop, daemon=True).start()
        except Exception as e:
            raise ConnectionError(f"No se pudo conectar al servidor: {e}")

    def send(self, msg: str):
        print(f"CLIENT -> SERVER: {msg}")
        self.sock.sendall((msg + '\n').encode('utf-8'))

    def _recv_loop(self):
        try:
            for line in self.rfile:
                line = line.rstrip('\n')
                print(f"SERVER -> CLIENT: {line}")
                if self.callback:
                    self.callback(line)
        except Exception:
            if self.callback:
                self.callback("ERR 0 \"Conexión perdida\"")

class GameGui:
    def __init__(self, root):
        self.root = root
        self.root.title("CDSP Game Client")
        self.root.geometry("800x600")
        
        self.client = None
        self.username = ""
        self.role = ""
        self.pos = (0, 0)
        self.other_players = {} # {username: (x, y)}
        self.resources = {} # {id: {"x": x, "y": y, "status": "safe"}}
        
        self._setup_login_ui()

    def _setup_login_ui(self):
        self.login_frame = tk.Frame(self.root)
        self.login_frame.pack(expand=True)
        
        tk.Label(self.login_frame, text="Username:", font=("Arial", 12)).pack(pady=5)
        self.user_entry = tk.Entry(self.login_frame, font=("Arial", 12))
        self.user_entry.pack(pady=5)
        self.user_entry.insert(0, "juanito")
        
        tk.Label(self.login_frame, text="Server Host:", font=("Arial", 10)).pack(pady=2)
        self.host_entry = tk.Entry(self.login_frame)
        self.host_entry.pack(pady=2)
        self.host_entry.insert(0, "localhost")
        
        tk.Label(self.login_frame, text="Port:", font=("Arial", 10)).pack(pady=2)
        self.port_entry = tk.Entry(self.login_frame)
        self.port_entry.pack(pady=2)
        self.port_entry.insert(0, "8080")
        
        tk.Button(self.login_frame, text="Conectar", command=self._connect, font=("Arial", 12), bg="green", fg="white").pack(pady=20)

    def _connect(self):
        host = self.host_entry.get()
        port = int(self.port_entry.get())
        self.username = self.user_entry.get()
        
        try:
            self.client = CDSPClient(host, port, callback=self._on_message)
            self.client.send(f"AUTH {self.username}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _setup_lobby_ui(self):
        self.login_frame.pack_forget()
        self.lobby_frame = tk.Frame(self.root)
        self.lobby_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        tk.Label(self.lobby_frame, text=f"Bienvenido, {self.username} ({self.role})", font=("Arial", 14, "bold")).pack(pady=10)
        
        btn_frame = tk.Frame(self.lobby_frame)
        btn_frame.pack(pady=10)
        
        tk.Button(btn_frame, text="Listar Salas", command=lambda: self.client.send("LIST_ROOMS")).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Crear Sala", command=lambda: self.client.send("CREATE_ROOM")).pack(side=tk.LEFT, padx=5)
        
        self.rooms_listbox = tk.Listbox(self.lobby_frame, font=("Courier", 10))
        self.rooms_listbox.pack(fill=tk.BOTH, expand=True, pady=10)
        
        tk.Button(self.lobby_frame, text="Unirse a Sala", command=self._join_room, bg="blue", fg="white").pack(pady=5)

    def _join_room(self):
        selection = self.rooms_listbox.curselection()
        if selection:
            room_id = self.rooms_listbox.get(selection[0]).split()[0]
            self.client.send(f"JOIN {room_id}")
        else:
            messagebox.showwarning("Aviso", "Selecciona una sala de la lista")

    def _setup_game_ui(self):
        self.lobby_frame.pack_forget()
        self.game_frame = tk.Frame(self.root)
        self.game_frame.pack(fill=tk.BOTH, expand=True)
        
        # Panel Izquierdo: Tablero
        self.canvas = tk.Canvas(self.game_frame, width=400, height=400, bg="white", borderwidth=2, relief="sunken")
        self.canvas.pack(side=tk.LEFT, padx=10, pady=10)
        self._draw_grid()
        
        # Panel Derecho: Controles y Logs
        ctrl_frame = tk.Frame(self.game_frame)
        ctrl_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        tk.Label(ctrl_frame, text="Movimiento", font=("Arial", 12, "bold")).pack()
        
        # Muestra de rol
        tk.Label(ctrl_frame, text=f"Tu Rol: {self.role.upper()}", font=("Arial", 10, "italic"), fg="blue").pack(pady=5)
        
        move_grid = tk.Frame(ctrl_frame)
        move_grid.pack(pady=5)
        
        tk.Button(move_grid, text="↑", width=5, command=lambda: self._move(0, -1)).grid(row=0, column=1)
        tk.Button(move_grid, text="←", width=5, command=lambda: self._move(-1, 0)).grid(row=1, column=0)
        tk.Button(move_grid, text="→", width=5, command=lambda: self._move(1, 0)).grid(row=1, column=2)
        tk.Button(move_grid, text="↓", width=5, command=lambda: self._move(0, 1)).grid(row=2, column=1)
        
        action_frame = tk.Frame(ctrl_frame)
        action_frame.pack(pady=10)
        
        if self.role == "atacante":
            tk.Button(action_frame, text="SCAN", command=lambda: self.client.send("SCAN"), bg="orange").pack(fill=tk.X, pady=2)
            self.attack_entry = tk.Entry(action_frame)
            self.attack_entry.pack(fill=tk.X)
            tk.Button(action_frame, text="ATTACK", command=lambda: self.client.send(f"ATTACK {self.attack_entry.get()}"), bg="red", fg="white").pack(fill=tk.X, pady=2)
        else:
            self.mitigate_entry = tk.Entry(action_frame)
            self.mitigate_entry.pack(fill=tk.X)
            tk.Button(action_frame, text="MITIGATE", command=lambda: self.client.send(f"MITIGATE {self.mitigate_entry.get()}"), bg="cyan").pack(fill=tk.X, pady=2)
            
        tk.Button(ctrl_frame, text="STATUS", command=lambda: self.client.send("STATUS")).pack(fill=tk.X, pady=5)
        
        self.log_area = scrolledtext.ScrolledText(ctrl_frame, height=10, font=("Courier", 8))
        self.log_area.pack(fill=tk.BOTH, expand=True, pady=10)

    def _draw_grid(self):
        self.canvas.delete("all")
        cell_size = 20
        for i in range(21):
            self.canvas.create_line(i*cell_size, 0, i*cell_size, 400, fill="gray")
            self.canvas.create_line(0, i*cell_size, 400, i*cell_size, fill="gray")
        
        # Dibujar otros jugadores (colores aleatorios, rol anonimo)
        for name, pdata in self.other_players.items():
            ox, oy = pdata["x"], pdata["y"]
            pcolor = pdata.get("color", "#AFAFAF")
            self.canvas.create_oval(ox*cell_size+4, oy*cell_size+4, (ox+1)*cell_size-4, (oy+1)*cell_size-4, fill=pcolor, outline="#333333")

        # Dibujar recursos descubiertos
        for res_id, rinfo in self.resources.items():
            rx, ry = rinfo["x"], rinfo["y"]
            status = rinfo["status"]
            color = "green" if status == "safe" else "orange" if status == "under_attack" else "black" if status == "compromised" else "red"
            self.canvas.create_rectangle(rx*cell_size+2, ry*cell_size+2, (rx+1)*cell_size-2, (ry+1)*cell_size-2, fill=color, outline="black")
            self.canvas.create_text(rx*cell_size+10, ry*cell_size+10, text="S", fill="white", font=("Arial", 8, "bold"))

        # Dibujar mi Jugador (Blanco)
        x, y = self.pos
        self.canvas.create_oval(x*cell_size+2, y*cell_size+2, (x+1)*cell_size-2, (y+1)*cell_size-2, 
                               fill="#FFFFFF", outline="black", width=2)
        self.canvas.create_text(x*cell_size+10, y*cell_size+10, text="Tú", fill="black", font=("Arial", 7, "bold"))

    def _move(self, dx, dy):
        self.client.send(f"MOVE {dx} {dy}")

    def _on_message(self, line):
        self.root.after(0, self._handle_message, line)

    def _handle_message(self, line):
        parts = line.split(' ')
        verb = parts[0]
        
        if hasattr(self, 'log_area'):
            self.log_area.insert(tk.END, line + "\n")
            self.log_area.see(tk.END)
            
        if verb == "OK" and parts[1] == "AUTH":
            self.role = parts[3].split('=')[1]
            self._setup_lobby_ui()
            
        elif verb == "ROOM_LIST":
            self.rooms_listbox.delete(0, tk.END)
            for room in parts[2:]:
                self.rooms_listbox.insert(tk.END, room)
                
        elif verb == "ROOM_CREATED":
            self.rooms_listbox.insert(tk.END, parts[1])
            
        elif verb == "OK" and parts[1] == "JOIN":
            pos_part = parts[4].split('=')[1].split(',')
            self.pos = (int(pos_part[0]), int(pos_part[1]))
            
            # --- DEBUG: Revelar recursos al defensor (para pruebas) ---
            if self.role == "defensor":
                self.resources["srv_01"] = {"x": 5, "y": 5, "status": "safe"}
                self.resources["srv_02"] = {"x": 15, "y": 15, "status": "safe"}
            # --------------------------------------------
            
            self._setup_game_ui()
            
        elif verb == "OK" and parts[1] == "MOVE":
            self.pos = (int(parts[2]), int(parts[3]))
            self._draw_grid()
            
            # Auto-completar si estamos parados sobre un recurso conocido
            for res_id, rinfo in self.resources.items():
                if rinfo["x"] == self.pos[0] and rinfo["y"] == self.pos[1]:
                    if hasattr(self, 'attack_entry'):
                        self.attack_entry.delete(0, tk.END)
                        self.attack_entry.insert(0, res_id)
                    if hasattr(self, 'mitigate_entry'):
                        self.mitigate_entry.delete(0, tk.END)
                        self.mitigate_entry.insert(0, res_id)
                    break
            
        elif verb == "SCAN_RESULT":
            if "found" in parts:
                res_id = parts[2]
                rx, ry = int(parts[3]), int(parts[4])
                # Guardar en memoria y redibujar
                self.resources[res_id] = {"x": rx, "y": ry, "status": "safe"}
                self._draw_grid()
                
                # Auto-completar el cuadro de texto dependiendo del rol
                if hasattr(self, 'attack_entry'):
                    self.attack_entry.delete(0, tk.END)
                    self.attack_entry.insert(0, res_id)
                if hasattr(self, 'mitigate_entry'):
                    self.mitigate_entry.delete(0, tk.END)
                    self.mitigate_entry.insert(0, res_id)
            messagebox.showinfo("Scanner", line)
            
        elif verb == "GAME_STATE":
            # Formato: GAME_STATE x y rec1=estado rec2=estado
            self.pos = (int(parts[1]), int(parts[2]))
            self._draw_grid()
            
        elif verb == "NOTIFY":
            notify_type = parts[1]
            if notify_type == "MOVE":
                name = parts[2]
                nx, ny = int(parts[3]), int(parts[4])
                if name not in self.other_players:
                    self.other_players[name] = {"x": nx, "y": ny, "color": random.choice(PLAYER_COLORS)}
                else:
                    self.other_players[name]["x"] = nx
                    self.other_players[name]["y"] = ny
                self._draw_grid()
            elif notify_type == "PLAYER_JOIN":
                name = parts[2]
                nx, ny = int(parts[3]), int(parts[4])
                self.other_players[name] = {"x": nx, "y": ny, "color": random.choice(PLAYER_COLORS)}
                self._draw_grid()
                self.log_area.insert(tk.END, f"*** {name} entró a la sala\n")
            elif notify_type == "PLAYER_LEFT":
                name = parts[2]
                if name in self.other_players:
                    del self.other_players[name]
                self._draw_grid()
                self.log_area.insert(tk.END, f"*** {name} abandonó la sala\n")
            elif notify_type == "ATTACK":
                res_id = parts[2]
                if res_id in self.resources:
                    self.resources[res_id]["status"] = "under_attack"
                self._draw_grid()
            elif notify_type == "MITIGATED":
                res_id = parts[2]
                if res_id in self.resources:
                    self.resources[res_id]["status"] = "safe"
                self._draw_grid()
            elif notify_type == "COMPROMISED":
                res_id = parts[2]
                if res_id in self.resources:
                    self.resources[res_id]["status"] = "compromised"
                self._draw_grid()
                messagebox.showwarning("SISTEMA COMPROMETIDO", f"El recurso {res_id} ha sido perdido.")
            elif notify_type == "GAME_OVER":
                winner = parts[2]
                reason = " ".join(parts[3:])
                messagebox.showinfo("FIN DE PARTIDA", f"GANADOR: {winner.upper()}\nMotivo: {reason}")
            else:
                messagebox.showinfo("Notificación", " ".join(parts[1:]))

        elif verb == "ERR":
            messagebox.showerror("Error del Servidor", line)

if __name__ == "__main__":
    root = tk.Tk()
    app = GameGui(root)
    root.mainloop()
