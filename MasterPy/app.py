# app.py — KUBE Trading Splashscreen mit integriertem Startup
# Zeigt sofort Splashscreen mit Start-Button, dann Spinner während MT5/C++/Visual_KUBE starten

import os
import time
import subprocess
from pathlib import Path
import sys
import tkinter.messagebox as messagebox
import customtkinter as ctk
import json

# ---------- Pfade & Einstellungen ----------
HERE = Path(__file__).resolve().parent
BATCH = (HERE / "scripts" / "start_mt5.bat").resolve()

PLATFORM = "x64"
BUILD    = "Debug"
EXE_NAME = "KUBE_CPP.exe"
BUILD_DIR = (HERE / f"../{PLATFORM}/{BUILD}").resolve()
CONFIG_PATH = (HERE / "orchestrator_config.json").resolve()

START_DELAY_SEC = 20

# ---------- Theme ----------
ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

def load_config():
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        return json.load(f)

def find_exe() -> Path | None:
    """Suche EXE zuerst am erwarteten Ort, ansonsten rekursiv im Repo."""
    expected = BUILD_DIR / EXE_NAME
    if expected.exists():
        return expected

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

    def pref_key(p: Path):
        s = p.as_posix().lower()
        return (
            0 if "x64" in s else 1,
            0 if "debug" in s else 1,
            len(s)
        )

    candidates.sort(key=pref_key)
    return candidates[0]


class SplashScreen(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        # Splashscreen ohne Rahmen
        self.overrideredirect(True)
        
        # Größe und Zentrierung - Vergrößert für größeres Logo
        window_width = 600
        window_height = 850  # Erhöht von 700 auf 850 für größeres Logo
        
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        
        center_x = int(screen_width/2 - window_width/2)
        center_y = int(screen_height/2 - window_height/2)
        
        self.geometry(f"{window_width}x{window_height}+{center_x}+{center_y}")
        
        # Hauptframe
        self.main_frame = ctk.CTkFrame(
            self, 
            corner_radius=20, 
            fg_color=("#2b2b2b", "#1a1a1a"),
            border_width=2,
            border_color=("#00bfff", "#0080cc")
        )
        self.main_frame.pack(fill="both", expand=True, padx=2, pady=2)
        
        # Logo
        self.create_logo()
        
        # Title
        self.title_label = ctk.CTkLabel(
            self.main_frame,
            text="Advanced Forex Trading Manager",
            font=ctk.CTkFont(size=20, weight="bold"),
            text_color=("#00bfff", "#00bfff")
        )
        self.title_label.pack(pady=(10, 5))
        
        # Subtitle
        self.subtitle_label = ctk.CTkLabel(
            self.main_frame,
            text="Test Umgebung: historische Daten",
            font=ctk.CTkFont(size=14),
            text_color=("#888888", "#aaaaaa")
        )
        self.subtitle_label.pack(pady=(0, 30))
        
        # Start Button (initial sichtbar)
        self.btn_start = ctk.CTkButton(
            self.main_frame,
            text="🚀 System starten",
            height=50,
            width=300,
            font=ctk.CTkFont(size=16, weight="bold"),
            command=self.on_start_click,
            corner_radius=12,
            fg_color=("#00bfff", "#0080cc"),
            hover_color=("#0099dd", "#006699")
        )
        self.btn_start.pack(pady=20)
        
        # Spinner (initial versteckt) - größerer Font braucht mehr Platz
        self.spinner_label = ctk.CTkLabel(
            self.main_frame,
            text="◐",
            font=ctk.CTkFont(size=72, weight="bold"),
            text_color=("#00bfff", "#00bfff")
        )
        # Nicht packen - wird später angezeigt
        
        # Status Label
        self.status_label = ctk.CTkLabel(
            self.main_frame,
            text="Bereit zum Start",
            font=ctk.CTkFont(size=16),
            text_color=("#666666", "#999999")
        )
        self.status_label.pack(pady=10)
        
        # Progress Bar (optional) - KUBE LIVE Farbe (gelb)
        self.progress_bar = ctk.CTkProgressBar(
            self.main_frame,
            width=400,
            height=8,
            corner_radius=4,
            fg_color=("#333333", "#222222"),
            progress_color="yellow"  # KUBE LIVE Farbe wie in SplashScreen
        )
        # Nicht packen - wird später angezeigt
        
        # Footer
        footer = ctk.CTkLabel(
            self.main_frame,
            text="© KUBE Trading System • v1.0",
            font=ctk.CTkFont(size=10),
            text_color=("#444444", "#666666")
        )
        footer.pack(side="bottom", pady=15)
        
        # Variablen
        self.cpp_proc = None
        self.spinner_angle = 0
        self.spinner_running = False
        
        # After-IDs für Cleanup speichern
        self._spinner_after_id = None
        self._progress_after_ids = []  # Liste für mehrere Progress-Updates
        
        # Close handler
        self.protocol("WM_DELETE_WINDOW", self.on_close)
    
    def create_logo(self):
        """Logo erstellen oder Fallback"""
        logo_path = Path(r"C:\KUBE_Trading\Entwicklung\Icons\Logo_final.png")
        if logo_path.exists():
            try:
                from PIL import Image, ImageEnhance
                
                # Logo laden
                original_img = Image.open(logo_path)
                
                # Helligkeit erhöhen (1.0 = original, >1.0 = heller)
                enhancer = ImageEnhance.Brightness(original_img)
                brightened_img = enhancer.enhance(1.3)  # 30% heller
                
                # Größe anpassen - BEHALTE Seitenverhältnis bei
                # Zielbreite für das Logo
                target_width = 300
                
                # Berechne Höhe basierend auf Original-Seitenverhältnis
                aspect_ratio = original_img.height / original_img.width
                target_height = int(target_width * aspect_ratio)
                
                # Größe anpassen mit hoher Qualität
                resized_img = brightened_img.resize(
                    (target_width, target_height), 
                    Image.Resampling.LANCZOS
                )
                
                logo_img = ctk.CTkImage(
                    light_image=resized_img,
                    dark_image=resized_img,
                    size=(target_width, target_height)
                )
                logo_label = ctk.CTkLabel(self.main_frame, image=logo_img, text="")
                logo_label.pack(pady=(30, 10))
                return
            except Exception as e:
                print(f"Logo konnte nicht geladen werden: {e}")
        
        # Fallback: Text-Logo
        logo_label = ctk.CTkLabel(
            self.main_frame,
            text="KUBE",
            font=ctk.CTkFont(size=48, weight="bold"),
            text_color=("#00bfff", "#00bfff")
        )
        logo_label.pack(pady=(30, 10))
    
    def on_start_click(self):
        """Start-Button wurde geklickt"""
        # Button ausblenden
        self.btn_start.pack_forget()
        
        # Spinner und Progress Bar anzeigen (mehr Platz für großen Spinner)
        self.spinner_label.pack(pady=40)
        self.progress_bar.pack(pady=10)
        self.progress_bar.set(0)
        
        # Spinner-Animation starten
        self.spinner_running = True
        self.animate_spinner()
        
        # Status aktualisieren
        self.status_label.configure(
            text="Initialisiere System...",
            text_color=("#00bfff", "#00bfff")
        )
        self.update()
        
        # Startup-Sequenz starten
        self.after(500, self.start_batch)
    
    def animate_spinner(self):
        """Spinner-Animation"""
        # Vorherige Callback-ID löschen
        if self._spinner_after_id is not None:
            self._spinner_after_id = None
        
        if not self.spinner_running:
            return
        
        spinner_chars = ["◐", "◓", "◑", "◒"]
        self.spinner_angle = (self.spinner_angle + 1) % len(spinner_chars)
        self.spinner_label.configure(text=spinner_chars[self.spinner_angle])
        
        # ID speichern für Cleanup
        self._spinner_after_id = self.after(150, self.animate_spinner)
    
    def simulate_progress_during_wait(self, total_seconds, start_progress, end_progress, callback):
        """
        Simuliert Fortschritt während einer Wartezeit
        
        Args:
            total_seconds: Gesamte Wartezeit in Sekunden
            start_progress: Start-Fortschritt (0.0-1.0)
            end_progress: End-Fortschritt (0.0-1.0)
            callback: Funktion, die nach Ablauf aufgerufen wird
        """
        steps = int(total_seconds * 10)  # 10 Updates pro Sekunde
        progress_increment = (end_progress - start_progress) / steps
        current_step = 0
        
        def update_progress():
            nonlocal current_step
            if current_step < steps:
                current_progress = start_progress + (progress_increment * current_step)
                self.progress_bar.set(current_progress)
                current_step += 1
                # ID speichern für Cleanup
                after_id = self.after(100, update_progress)  # Alle 100ms updaten
                self._progress_after_ids.append(after_id)
            else:
                # Fertig - callback aufrufen
                self.progress_bar.set(end_progress)
                callback()
        
        update_progress()
    
    def start_batch(self):
        """Schritt 1: MT5 Batch starten"""
        self.status_label.configure(text="Starte MetaTrader Terminals...")
        self.update()
        
        if not BATCH.exists():
            self.show_error("Batch nicht gefunden", f"Batch nicht gefunden:\n{BATCH}")
            return
        
        try:
            subprocess.Popen(
                ["cmd", "/c", str(BATCH)],
                cwd=BATCH.parent,
                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
            )
        except Exception as e:
            self.show_error("Fehler", f"Start der Batch fehlgeschlagen:\n{e}")
            return
        
        # Warten auf MT5 mit gleichmäßiger Progress-Animation von 0% bis 100%
        self.status_label.configure(text=f"Lade Metatrader Terminals...")
        self.update()
        
        # Simuliere gleichmäßigen Fortschritt über die gesamte Wartezeit
        self.simulate_progress_during_wait(START_DELAY_SEC, 0.0, 1.0, self.start_cpp)
    
    def start_cpp(self):
        """Schritt 2: C++ Engine starten"""
        self.status_label.configure(text="Suche C++ Komponente...")
        self.update()
        
        exe_path = find_exe()
        if not exe_path or not exe_path.exists():
            listing = ""
            if BUILD_DIR.exists():
                try:
                    listing = "\n".join(sorted(os.listdir(BUILD_DIR)))
                except Exception:
                    listing = "(Zugriff fehlgeschlagen)"
            self.show_error(
                "EXE nicht gefunden",
                f"Die ausführbare Datei konnte nicht gefunden werden.\n\n"
                f"Gesucht wurde nach: {EXE_NAME}\n"
                f"Erwarteter Pfad: {BUILD_DIR / EXE_NAME}\n\n"
                f"Inhalt von {BUILD_DIR}:\n{listing or '(leer)'}"
            )
            return
        
        self.status_label.configure(text="Starte C++ Engine...")
        self.update()
        
        try:
            self.cpp_proc = subprocess.Popen([str(exe_path)])
        except Exception as e:
            self.show_error("Fehler", f"CPP-Start fehlgeschlagen:\n{e}")
            return
        
        self.after(1000, self.start_visual)
    
    def start_visual(self):
        """Schritt 3: Visual_KUBE starten (ohne eigenen Splash)"""
        self.status_label.configure(text="Starte Visualisierung...")
        self.update()
        
        visual_script = (HERE / "Visual_KUBE.py").resolve()
        visual_cwd = visual_script.parent
        
        if not visual_script.exists():
            listing = "\n".join(sorted(os.listdir(visual_cwd))) if visual_cwd.exists() else "(Ordner nicht vorhanden)"
            self.show_error(
                "Pfadfehler",
                f"Visual_KUBE.py nicht gefunden:\n{visual_script}\n\n"
                f"Inhalt von {visual_cwd}:\n{listing}"
            )
            return
        
        try:
            subprocess.Popen(
                [sys.executable, str(visual_script)],
                cwd=visual_cwd
            )
        except Exception as e:
            self.show_error("Fehler", f"Start von Visual_KUBE fehlgeschlagen:\n{e}")
            return
        
        # Erfolg!
        self.finish_startup()
    
    def finish_startup(self):
        """Startup erfolgreich abgeschlossen"""
        self.spinner_running = False
        self.status_label.configure(
            text="✓ System erfolgreich gestartet!",
            text_color=("#00ff88", "#00ff88")
        )
        self.progress_bar.set(1.0)
        self.update()
        
        # Splashscreen nach 2 Sekunden schließen
        self.after(2000, self.close_splash)
    
    def close_splash(self):
        """Schließt den Splash Screen sauber"""
        self.stop_animations()
        self.destroy()
    
    def show_error(self, title, message):
        """Fehler anzeigen und zurück zum Start-Button"""
        self.spinner_running = False
        messagebox.showerror(title, message)
        
        # Spinner und Progress Bar ausblenden
        self.spinner_label.pack_forget()
        self.progress_bar.pack_forget()
        
        # Start-Button wieder anzeigen
        self.btn_start.pack(pady=20)
        
        # Status zurücksetzen
        self.status_label.configure(
            text="Fehler - Bitte erneut versuchen",
            text_color=("#ff4444", "#ff4444")
        )
    
    def stop_animations(self):
        """
        Stoppt alle laufenden Animationen und bricht ausstehende after()-Callbacks ab.
        MUSS vor destroy() aufgerufen werden!
        """
        self.spinner_running = False
        
        # Spinner-Callback abbrechen
        if self._spinner_after_id is not None:
            try:
                self.after_cancel(self._spinner_after_id)
            except:
                pass
            self._spinner_after_id = None
        
        # Alle Progress-Callbacks abbrechen
        for after_id in self._progress_after_ids:
            try:
                self.after_cancel(after_id)
            except:
                pass
        self._progress_after_ids.clear()
    
    def on_close(self):
        """Fenster schließen"""
        # Animationen stoppen BEVOR wir schließen
        self.stop_animations()
        
        try:
            if self.cpp_proc and self.cpp_proc.poll() is None:
                self.cpp_proc.terminate()
        except Exception:
            pass
        self.destroy()


if __name__ == "__main__":
    SplashScreen().mainloop()
