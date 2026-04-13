import tkinter as tk
import random
from tkinter import messagebox, scrolledtext
from network import CDSPClient

PLAYER_COLORS = [
    "#FF5733", "#33FF57", "#3357FF", "#F333FF",
    "#33FFF3", "#FFF333", "#8D33FF", "#FF8D33",
    "#FF338D",
]

class GameGui:

    def __init__(self, root):
        self.root = root
        self.root.title("CDSP Game Client")
        self.root.geometry("800x600")

        self.client = None
        self.username = ""
        self.role = ""
        self.pos = (0, 0)
        self.other_players = {}
        self.resources = {}

        self._setup_login_ui()

    def _proc_move(self, p):
        if len(p) < 5:
            return
        x = int(p[-2])
        y = int(p[-1])
        uname = " ".join(p[2:-2])

        if uname == self.username:
            self.pos = (x, y)
        else:
            self.other_players[uname] = {"x": x, "y": y, "color": self.other_players.get(uname, {}).get("color", random.choice(PLAYER_COLORS))}
        self._draw_grid()

    def _proc_game_over(self, p):
        winner = p[2]
        reason = " ".join(p[3:])
        msg = f"FIN DE PARTIDA\nGanador: {winner.upper()}\nMotivo: {reason}"
        messagebox.showinfo("Game Over", msg)
        self._safe_log(f"!!! {msg}")

        self.resources = {}
        self.other_players = {}

        self.root.after(2000, self._setup_lobby_ui)
        self.root.after(2500, lambda: self.client.send("LIST_ROOMS") if hasattr(self, "client") else None)

    def _setup_login_ui(self):
        self.login_frame = tk.Frame(self.root)
        self.login_frame.pack(expand=True)

        tk.Label(self.login_frame, text="Username:", font=("Arial", 12)).pack(pady=5)
        self.user_entry = tk.Entry(self.login_frame, font=("Arial", 12))
        self.user_entry.pack(pady=5)
        self.user_entry.insert(0, "juan")

        tk.Label(self.login_frame, text="Server Host:", font=("Arial", 10)).pack(pady=2)
        self.host_entry = tk.Entry(self.login_frame)
        self.host_entry.pack(pady=2)
        self.host_entry.insert(0, "server.cdsp")

        tk.Button(
            self.login_frame, text="Conectar", command=self._connect,
            font=("Arial", 12), bg="#28a745", fg="white"
        ).pack(pady=20)

    def _connect(self):
        host = self.host_entry.get()
        try:
            self.username = self.user_entry.get()
            self.client = CDSPClient(host, 8080, callback=self._on_message)
            self.client.send(f"AUTH {self.username}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _setup_lobby_ui(self):
        if hasattr(self, "login_frame"):
            self.login_frame.pack_forget()
        if hasattr(self, "lobby_frame"):
            self.lobby_frame.destroy()
        if hasattr(self, "game_frame"):
            self.game_frame.destroy()

        self.lobby_frame = tk.Frame(self.root)
        self.lobby_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        tk.Label(
            self.lobby_frame, text=f"Bienvenido, {self.username} ({self.role})",
            font=("Arial", 14, "bold")
        ).pack(pady=10)

        btn_frame = tk.Frame(self.lobby_frame)
        btn_frame.pack(pady=10)

        tk.Button(btn_frame, text="Listar Salas", command=lambda: self.client.send("LIST_ROOMS")).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Crear Sala", command=lambda: self.client.send("CREATE_ROOM")).pack(side=tk.LEFT, padx=5)

        self.rooms_listbox = tk.Listbox(self.lobby_frame, font=("Courier", 10))
        self.rooms_listbox.pack(fill=tk.BOTH, expand=True, pady=10)

        tk.Button(
            self.lobby_frame, text="Unirse a Sala", command=self._join_room,
            bg="#007bff", fg="white"
        ).pack(pady=5)

    def _join_room(self):
        selection = self.rooms_listbox.curselection()
        if selection:
            room_id = self.rooms_listbox.get(selection[0]).split()[0]
            self.client.send(f"JOIN {room_id} {self.role}")
        else:
            messagebox.showwarning("Aviso", "Selecciona una sala de la lista")

    def _setup_game_ui(self):
        if hasattr(self, "lobby_frame"):
            self.lobby_frame.pack_forget()
        self.game_frame = tk.Frame(self.root)
        self.game_frame.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(
            self.game_frame, width=400, height=400, bg="white",
            borderwidth=2, relief="sunken"
        )
        self.canvas.pack(side=tk.LEFT, padx=10, pady=10)
        self._draw_grid()

        ctrl_frame = tk.Frame(self.game_frame)
        ctrl_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        tk.Label(ctrl_frame, text=f"Tu Rol: {self.role.upper()}", font=("Arial", 10, "italic"), fg="blue").pack(pady=5)

        move_grid = tk.Frame(ctrl_frame)
        move_grid.pack(pady=5)
        tk.Button(move_grid, text="↑", width=5, command=lambda: self._move(0, -1)).grid(row=0, column=1)
        tk.Button(move_grid, text="←", width=5, command=lambda: self._move(-1, 0)).grid(row=1, column=0)
        tk.Button(move_grid, text="→", width=5, command=lambda: self._move(1, 0)).grid(row=1, column=2)
        tk.Button(move_grid, text="↓", width=5, command=lambda: self._move(0, 1)).grid(row=2, column=1)

        action_frame = tk.Frame(ctrl_frame)
        action_frame.pack(pady=20, fill=tk.X)

        if self.role == "attacker":
            tk.Label(action_frame, text="Resource ID:").pack()
            self.resource_entry = tk.Entry(action_frame)
            self.resource_entry.pack(fill=tk.X, pady=2)
            self.resource_entry.insert(0, "srv_01")

            tk.Button(
                action_frame, text="SCAN", bg="#ffc107",
                font=("Arial", 10, "bold"),
                command=lambda: self.client.send("SCAN")
            ).pack(fill=tk.X, pady=5)

            tk.Button(
                action_frame, text="ATTACK", bg="#dc3545", fg="white",
                font=("Arial", 12, "bold"), height=2,
                command=lambda: self.client.send(f"ATTACK {self.resource_entry.get()}")
            ).pack(fill=tk.X, pady=5)
        else:
            tk.Label(action_frame, text="Resource ID:").pack()
            self.resource_entry = tk.Entry(action_frame)
            self.resource_entry.pack(fill=tk.X, pady=2)
            self.resource_entry.insert(0, "srv_01")

            tk.Button(
                action_frame, text="MITIGATE", bg="#17a2b8",
                font=("Arial", 12, "bold"), height=2,
                command=lambda: self.client.send(f"MITIGATE {self.resource_entry.get()}")
            ).pack(fill=tk.X)

        tk.Button(ctrl_frame, text="STATUS", bg="#6c757d", fg="white",
                  command=lambda: self.client.send("STATUS")).pack(fill=tk.X, pady=5)

        self.log_area = scrolledtext.ScrolledText(ctrl_frame, height=10, font=("Courier", 8))
        self.log_area.pack(fill=tk.BOTH, expand=True, pady=10)

    def _draw_grid(self):
        if not hasattr(self, "canvas") or not self.canvas.winfo_exists():
            return
        self.canvas.delete("all")
        cell = 20
        for i in range(21):
            self.canvas.create_line(i*cell, 0, i*cell, 400, fill="#eee")
            self.canvas.create_line(0, i*cell, 400, i*cell, fill="#eee")

        for pdata in self.other_players.values():
            ox, oy = pdata["x"], pdata["y"]
            self.canvas.create_oval(ox*cell+4, oy*cell+4, (ox+1)*cell-4, (oy+1)*cell-4, fill=pdata["color"])

        for rid, rinfo in self.resources.items():
            rx, ry = rinfo["x"], rinfo["y"]
            color = {
                "safe": "#28a745",
                "under_attack": "#ffc107",
                "compromised": "#343a40",
                "mitigated": "#17a2b8"
            }.get(rinfo["status"], "#dc3545")
            self.canvas.create_rectangle(rx*cell+2, ry*cell+2, (rx+1)*cell-2, (ry+1)*cell-2, fill=color)

        mx, my = self.pos
        self.canvas.create_oval(mx*cell+2, my*cell+2, (mx+1)*cell-2, (my+1)*cell-2, fill="white", outline="black")

        
    def _auto_select_resource(self):
        # Si hay un recurso en la celda actual, actualizar el campo Resource ID
        if not hasattr(self, "resource_entry"):
            return
        for rid, rinfo in self.resources.items():
            if rinfo["x"] == self.pos[0] and rinfo["y"] == self.pos[1]:
                self.resource_entry.delete(0, tk.END)
                self.resource_entry.insert(0, rid)
                return


    def _move(self, dx, dy):
        if self.client:
            self.client.send(f"MOVE {dx} {dy}")

    def _on_message(self, line):
        self.root.after(0, self._handle_message, line)

    def _safe_log(self, text):
        if hasattr(self, "log_area") and self.log_area.winfo_exists():
            try:
                self.log_area.insert(tk.END, text + "\n")
                self.log_area.see(tk.END)
            except tk.TclError:
                pass

    def _handle_message(self, line):
        parts = line.split(" ")
        verb = parts[0]
        self._safe_log(line)

        if verb == "OK":
            self._proc_ok(parts)
        elif verb == "NOTIFY":
            self._proc_notify(parts[1], parts)
        elif verb == "SCAN_RESULT":
            self._proc_scan(parts)
        elif verb == "ERR":
            messagebox.showerror("Servidor", line)

    def _proc_ok(self, p):
        if len(p) < 2:
            return
        verb = p[1]

        if verb == "AUTH":
            for part in p[2:]:
                if part.startswith("ROLE="):
                    self.role = part.split("=")[1]
                    break
            self._setup_lobby_ui()

        elif verb == "LIST_ROOMS":
            # OK LIST_ROOMS 1 room_001 room_002
            if hasattr(self, "rooms_listbox") and self.rooms_listbox.winfo_exists():
                self.rooms_listbox.delete(0, tk.END)
                for r in p[3:]:
                    if r.strip():
                        self.rooms_listbox.insert(tk.END, r)

        elif verb == "CREATE_ROOM":
            # OK CREATE_ROOM room_001
            if hasattr(self, "rooms_listbox") and self.rooms_listbox.winfo_exists():
                if len(p) > 2:
                    self.rooms_listbox.insert(tk.END, p[2])

        elif verb == "JOIN":
    # OK JOIN room_001 ROLE=xxx POS=x,y RESOURCES=srv_01:5,5;srv_02:15,15
            for part in p[2:]:
                if part.startswith("POS="):
                    coords = part.split("=")[1].split(",")
                    self.pos = (int(coords[0]), int(coords[1]))
                elif part.startswith("RESOURCES="):
                    res_raw = part.split("=")[1]
                    for r_item in res_raw.split(";"):
                        if ":" in r_item:
                            rid, rpos = r_item.split(":")
                            rx, ry = rpos.split(",")
                            self.resources[rid] = {"x": int(rx), "y": int(ry), "status": "safe"}
            self._setup_game_ui()

        elif verb == "MOVE":
            for part in p[2:]:
                if part.startswith("POS="):
                    coords = part.split("=")[1].split(",")
                    self.pos = (int(coords[0]), int(coords[1]))
                    self._draw_grid()
                    # Verificar si hay un recurso en esta celda y actualizar el campo
                    self._auto_select_resource()
                    break

        elif verb == "ATTACK":
            # OK ATTACK srv_01
            if len(p) > 2 and p[2] in self.resources:
                self.resources[p[2]]["status"] = "under_attack"
            self._draw_grid()

        elif verb == "MITIGATE":
            # OK MITIGATE srv_01
            if len(p) > 2 and p[2] in self.resources:
                self.resources[p[2]]["status"] = "mitigated"
            self._draw_grid()

    def _proc_notify(self, ntype, p):
        if ntype == "MOVE":
            self._proc_move(p)
        elif ntype == "PLAYER_JOIN":
            self._proc_player_joined(p)
        elif ntype == "PLAYER_LEFT":
            uname = " ".join(p[2:])
            if uname in self.other_players:
                del self.other_players[uname]
            self._draw_grid()
        elif ntype == "ATTACK":
            if len(p) > 2 and p[2] in self.resources:
                self.resources[p[2]]["status"] = "under_attack"
            self._draw_grid()
        elif ntype == "MITIGATED":
            if len(p) > 2 and p[2] in self.resources:
                self.resources[p[2]]["status"] = "mitigated"
            self._draw_grid()
        elif ntype == "FOUND":
            # NOTIFY FOUND srv_01 — solo el atacante lo recibe
            if len(p) > 2:
                rid = p[2]
                messagebox.showinfo("¡Recurso encontrado!", f"Encontraste: {rid}")
                if hasattr(self, "resource_entry"):
                    self.resource_entry.delete(0, tk.END)
                    self.resource_entry.insert(0, rid)
        elif ntype == "GAME_OVER":
            self._proc_game_over(p)

    def _proc_player_joined(self, p):
        if len(p) < 5:
            return
        uname = " ".join(p[2:-2])
        self.other_players[uname] = {"x": int(p[-2]), "y": int(p[-1]), "color": random.choice(PLAYER_COLORS)}
        self._draw_grid()
        self._safe_log(f"*** {uname} entró")

    def _proc_scan(self, p):
        # SCAN_RESULT found srv_01  o  SCAN_RESULT none
        if len(p) > 1 and p[1] == "found" and len(p) > 2:
            rid = p[2]
            messagebox.showinfo("Scanner", f"Recurso encontrado cerca: {rid}")
            if hasattr(self, "resource_entry"):
                self.resource_entry.delete(0, tk.END)
                self.resource_entry.insert(0, rid)
        else:
            messagebox.showinfo("Scanner", "No hay recursos cerca")