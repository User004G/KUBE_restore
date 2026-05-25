# app.py — CustomTkinter GUI zum Start von MT5 (Batch) + C++-Tool + Python-TopLevel
# Robust: findet die EXE automatisch (Fallback), zeigt Ordner-Inhalt bei Fehlern.

import os
import time
import subprocess
from pathlib import Path
import sys
import tkinter.messagebox as messagebox
import customtkinter as ctk
import json

# import Visual_KUBE

# ---------- Pfade & Einstellungen ----------
HERE = Path(__file__).resolve().parent
BATCH = (HERE / "scripts" / "start_mt5.bat").resolve()   # <- deine Batch

# Erwartete Build-Ordner-Struktur (kannst du hier anpassen)
PLATFORM = "x64"
BUILD    = "Debug"
EXE_NAME = "KUBE_CPP.exe"                          # <- Zielname des C++-Projekts (ohne Pfad)
BUILD_DIR = (HERE / f"../{PLATFORM}/{BUILD}").resolve()  # <- Erwarteter Outputordner
CONFIG_PATH = (HERE / "orchestrator_config.json").resolve()

# Wartezeit nach Batch-Start (z. B. bis MT5 hochgefahren ist)
START_DELAY_SEC = 30

# ---------- Theme ----------
ctk.set_appearance_mode("Dark")      # "Dark" | "Light" | "System"
ctk.set_default_color_theme("blue")  # "blue" | "dark-blue" | "green"

def load_config():
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        return json.load(f)
def find_exe() -> Path | None:
    """
    Suche EXE zuerst am erwarteten Ort, ansonsten rekursiv im Repo.
    Bevorzugt Pfade, die 'x64' und 'Debug' enthalten.
    """
    expected = BUILD_DIR / EXE_NAME
    if expected.exists():
        return expected

    # Fallback: in Projekt-/Solution-Umgebung suchen
    roots = [HERE, HERE.parent, HERE.parent.parent]
    candidates = []
    for root in roots:
        try:
            for cand in root.rglob(EXE_NAME):
                candidates.append(cand)
        except Exception:
            pass

    if not candidates:
        return None

    # Bevorzugung: .../x64/... und .../Debug/...
    def pref_key(p: Path):
        s = p.as_posix().lower()
        return (
            0 if "x64" in s else 1,
            0 if "debug" in s else 1,
            len(s)
        )

    candidates.sort(key=pref_key)
    return candidates[0]


class App(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("KUBE Trading")
        self.geometry("360x180")
        self.minsize(320, 160)

        # Layout
        frame = ctk.CTkFrame(self, corner_radius=16)
        frame.grid(row=0, column=0, padx=16, pady=16, sticky="nsew")

        title = ctk.CTkLabel(frame, text="KUBE Teststart", font=ctk.CTkFont(size=18, weight="bold"))
        title.grid(row=0, column=0, padx=8, pady=(12, 8))

        self.btn_start = ctk.CTkButton(frame, text="Start", height=38, command=self.on_start)
        self.btn_start.grid(row=1, column=0, padx=12, pady=10, sticky="ew")

        self.cpp_proc = None
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def on_start(self):
        # 0) Batch starten (für MT5-Terminals etc.)
        if not BATCH.exists():
            messagebox.showerror("Fehlt", f"Batch nicht gefunden:\n{BATCH}")
            return
        try:
            subprocess.Popen(
                ["cmd", "/c", str(BATCH)],
                cwd=BATCH.parent,
                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
            )
        except Exception as e:
            messagebox.showerror("Fehler", f"Start der Batch fehlgeschlagen:\n{e}")
            return

        # Optional warten, bis MT5 bereit ist
        if START_DELAY_SEC > 0:
            time.sleep(START_DELAY_SEC)

        # 1) EXE suchen
        exe_path = find_exe()
        if not exe_path or not exe_path.exists():
            listing = ""
            if BUILD_DIR.exists():
                try:
                    listing = "\n".join(sorted(os.listdir(BUILD_DIR)))
                except Exception:
                    listing = "(Zugriff fehlgeschlagen)"
            messagebox.showerror(
                "EXE nicht gefunden",
                "Die ausführbare Datei konnte nicht gefunden werden.\n\n"
                f"Gesucht wurde nach: {EXE_NAME}\n"
                f"Erwarteter Pfad:    {BUILD_DIR / EXE_NAME}\n"
                f"Fallback-Suche ab:  {HERE.parent}\n\n"
                f"Inhalt von {BUILD_DIR}:\n{listing or '(leer)'}"
            )
            return

        # 2) C++-Tool starten
        try:
            self.cpp_proc = subprocess.Popen([str(exe_path)])
        except Exception as e:
            messagebox.showerror("Fehler", f"CPP-Start fehlgeschlagen:\n{e}")
            return

        # 3) Python-App starten (Visual_KUBE)
                # 3) Python-App starten (Visual_KUBE) — eigener Prozess, kein zweites Tk im selben Interpreter
        visual_script = (HERE / "Visual_KUBE.py").resolve()   # liegt laut Screenshot im selben Ordner wie app.py
        visual_cwd    = visual_script.parent

        if not visual_script.exists():
            # Diagnose, falls der Pfad doch anders ist
            listing = "\n".join(sorted(os.listdir(visual_cwd))) if visual_cwd.exists() else "(Ordner nicht vorhanden)"
            messagebox.showerror(
                "Pfadfehler",
                f"Visual_KUBE.py nicht gefunden:\n{visual_script}\n\n"
                f"Inhalt von {visual_cwd}:\n{listing}"
            )
            return

        try:
            # sys.executable = dein aktiver Python-Interpreter (ggf. venv)
            # Wenn du die Konsole unterdrücken willst: ersetze sys.executable durch "pythonw"
            subprocess.Popen(
                [sys.executable, str(visual_script)],
                cwd=visual_cwd
            )
        except Exception as e:
            messagebox.showerror("Fehler", f"Start von Visual_KUBE fehlgeschlagen:\n{e}")
            return



    def on_close(self):
        try:
            if self.cpp_proc and self.cpp_proc.poll() is None:
                self.cpp_proc.terminate()
        except Exception:
            pass
        self.destroy()


if __name__ == "__main__":
    App().mainloop()
