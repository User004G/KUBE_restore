
# Visual_KUBE.py - Haupteinstiegspunkt OHNE Splash Screen
# Der Splash Screen wird bereits von app.py gezeigt

import threading
import queue
import customtkinter as ctk
from Gui import Gui
from Masterloop import MasterLoop


def Start():
    """Startet die KUBE Trading Anwendung (ohne eigenen Splash Screen)"""
    
    # Queues und Event für Kommunikation
    data_q = queue.Queue(maxsize=50)   # Loop -> GUI
    cmd_q = queue.Queue(maxsize=200)   # GUI -> Loop
    stop_evt = threading.Event()
    
    # MasterLoop vorbereiten und starten
    loop = MasterLoop(data_q=data_q, cmd_q=cmd_q, stop_evt=stop_evt)
    master_thread = threading.Thread(target=loop.run, daemon=True)
    master_thread.start()
    
    # Verstecktes Root-Fenster fǬr CustomTkinter erstellen (exakt wie KUBE)
    root = ctk.CTk()
    root.withdraw()

    # Hauptanwendung als Toplevel starten (OHNE Splash Screen)
    app = Gui(master_root=root, data_q=data_q, cmd_q=cmd_q, stop_evt=stop_evt)
    
    # Warte auf das Schlieen des GUI-Fensters
    root.wait_window(app)
    
    # Cleanup nach Beenden der GUI
    stop_evt.set()
    master_thread.join(timeout=2.0)
    root.destroy()


if __name__ == "__main__":
    Start()
