import tkinter as tk
import random
import webbrowser
import platform
import subprocess
from tkinter import messagebox, scrolledtext
from network import CDSPClient

PLAYER_COLORS = [
    "#FF5733", "#33FF57", "#3357FF", "#F333FF",
    "#33FFF3", "#FFF333", "#8D33FF", "#FF8D33",
    "#FF338D",
]

class GameGui:

    def __init__(self, root, username=None, room_id=None, role=None, host=None):
        self.root = root
        self.root.title("CDSP Game Client")
        self.root.geometry("800x600")

        self.client = None
        self.username = username or ""
        self.role = role or ""
        self.room_id = room_id or ""
        self.host = host or "server.cdsp"
        
        self.pos = (0, 0)
        self.other_players = {}
        self.resources = {}

        if self.username and self.room_id:
            # Entrada directa (el auth server nos dirá el rol)
            self.root.after(100, self._direct_connect)
        else:
            messagebox.showerror("Error", "Este cliente debe ser lanzado desde el Portal Web.")
            self.root.destroy()

    def _direct_connect(self):
        try:
            self.client = CDSPClient(self.host, 8080, callback=self._on_message)
            self.client.send(f"AUTH {self.username}")
        except Exception as e:
            messagebox.showerror("Error de Conexión", str(e))
            self.root.destroy()

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

        # Regresar al portal web y cerrar
        def back_to_web():
            portal_url = "http://localhost:8000"
            try:
                if "microsoft" in platform.uname().release.lower():
                    subprocess.run(["powershell.exe", "/c", "start", portal_url], check=True)
                else:
                    webbrowser.open(portal_url)
            except Exception:
                webbrowser.open(portal_url)
            self.root.destroy()

        self.root.after(3000, back_to_web)

        self.root.after(3000, back_to_web)

    def _setup_game_ui(self):
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
            
            if self.room_id:
                # Si vinimos de CLI, unirse a la sala directamente
                self.client.send(f"JOIN {self.room_id} {self.role}")

        elif verb == "JOIN":
            # OK JOIN room_001 ROLE=xxx POS=x,y PLAYERS=u1:x,y;u2:x,y; RESOURCES=srv_01:5,5;srv_02:15,15
            for part in p[2:]:
                if part.startswith("POS="):
                    coords = part.split("=")[1].split(",")
                    self.pos = (int(coords[0]), int(coords[1]))
                elif part.startswith("PLAYERS="):
                    self._parse_players_string(part.split("=")[1])
                elif part.startswith("RESOURCES="):
                    res_raw = part.split("=")[1]
                    for r_item in res_raw.split(";"):
                        if ":" in r_item:
                            rid, rpos = r_item.split(":")
                            rx, ry = rpos.split(",")
                            self.resources[rid] = {"x": int(rx), "y": int(ry), "status": "safe"}
            self._setup_game_ui()

        elif verb == "STATUS":
            # OK STATUS ROOM=room_001 TIME_LEFT=299 PLAYERS=u1:x,y;u2:x,y; RESOURCES=srv_01:safe;
            for part in p[2:]:
                if part.startswith("PLAYERS="):
                    self._parse_players_string(part.split("=")[1])
                elif part.startswith("RESOURCES="):
                    res_raw = part.split("=")[1]
                    for r_item in res_raw.split(";"):
                        if ":" in r_item:
                            rid, rstat = r_item.split(":")
                            if rid in self.resources:
                                self.resources[rid]["status"] = rstat
            self._draw_grid()

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

    def _parse_players_string(self, p_str):
        if not p_str: 
            return
        for p_item in p_str.split(";"):
            if ":" in p_item:
                try:
                    uname, pos_raw = p_item.split(":")
                    if uname == self.username: 
                        continue
                    coords = pos_raw.split(",")
                    if len(coords) == 2:
                        self.other_players[uname] = {
                            "x": int(coords[0]), 
                            "y": int(coords[1]), 
                            "color": self.other_players.get(uname, {}).get("color", random.choice(PLAYER_COLORS))
                        }
                except (ValueError, IndexError):
                    continue
        self._draw_grid()

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