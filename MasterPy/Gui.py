
# Gui.py
import customtkinter as ctk
from CTkMessagebox import CTkMessagebox
import queue
from PIL import Image
from pathlib import Path
from datetime import datetime
import re
import sqlite3
import pandas as pd

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import matplotlib.dates as mdates
from mpl_toolkits.mplot3d import Axes3D


class Gui(ctk.CTk):
    """
    Visualizer-GUI:
    - Hält komplette Historie in Caches (kann bei reset-Flag gelöscht werden)
    - View-Range (max/mid/min) steuert, welcher Ausschnitt gezeichnet wird
    """

    def __init__(self, data_q, cmd_q, stop_evt):
        super().__init__()
        self.data_q   = data_q
        self.cmd_q    = cmd_q
        self.stop_evt = stop_evt

        # ---------------- View-Range / Limits ----------------
        # self.max_points_mid = 3000
        # self.max_points_min = 500
        # Pfad zu deiner KUBE_config.mqh
        self._config_path = r"C:\MQL_Shared_Restore\Includes\Config1\KUBE_config.mqh"

        # Lookback aus MQL-Konfig lesen
        lookback = self.read_lookback_from_mqh(self._config_path, default=500)

        # ---------------- View-Range / Limits ----------------
        # z.B.: mid ≈ 3*Lookback, min ≈ 1*Lookback
        self.max_points_mid = 5 * lookback
        self.max_points_min = 1 * lookback

        # Achsenbeschriftung aus MQL-Konfig lesen
        self.axis_labels = self.read_axis_labels_from_mqh(self._config_path)


        self.active_eas   = 0
        self.active_frac  = 0.0
        self.lambda_val   = 0.0          # ★ neu: aktueller lambda_dyn-Wert
        self.stat_view_mode = "RiskManager"

        # Caches: vollständige Historie (werden bei reset geleert)
        self.cache_live:        list[tuple[str, float]] = []
        self.cache_paper:       list[tuple[str, float]] = []
        self.cache_peak:        list[tuple[str, float]] = []
        self.cache_floor:       list[tuple[str, float]] = []
        self.cache_paper_sta:   list[tuple[str, float]] = []
        self.cache_active_eas:  list[tuple[str, float]] = []
        self.cache_lambda:      list[tuple[str, float]] = []   # ★ neu: Zeitreihe λ(ts)


        # Normalisierung
        self.cent_divisor   = 100.0
        self.start_capital  = {0: 10000.0, 10: 100000.0}
        
        # DB-Pfad für 3D-View
        self.db_path = r'c:/Users/kbeie/AppData/Roaming/MetaQuotes/Terminal/Common/Files/KUBE_Schwarm.db'
               
        # DB-Pfad für 3D-Fitness-View (Experts)
        self.db_fitness_path = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"
        self.last_timestamp = ""

        self.title("KUBE Trading System")
        
        # After-ID für Queue-Polling speichern
        self._poll_after_id = None
        
        self.create_screen("Dark", "blue")
        
        # Close handler registrieren
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    # ------------------------------------------------------------------ Layout

    def create_screen(self, Mode, Theme):
        ctk.set_appearance_mode(Mode)
        ctk.set_default_color_theme(Theme)

        window_width  = int(self.winfo_screenwidth() * 0.9)
        window_height = int(self.winfo_screenheight() * 0.9)
        self.geometry(f"{window_width}x{window_height}+10+10")

        self.sidebar_width = int(0.09 * window_width)

        # Grid
        self.grid_rowconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=0)
        self.grid_columnconfigure(1, weight=1)
        self.grid_columnconfigure(2, weight=1)

        # Sidebar
        self.sidebar = ctk.CTkFrame(self, width=self.sidebar_width,
                                    height=window_height, corner_radius=0)
        self.sidebar.grid(row=0, column=0, padx=5, pady=5, rowspan=2, sticky="ns")
        self.sidebar.grid_propagate(False)

        # Hauptframes (LO/RO/LU/RU)
        self.LO_frame = ctk.CTkFrame(self, corner_radius=0)
        self.RO_frame = ctk.CTkFrame(self, corner_radius=0)
        self.LU_frame = ctk.CTkFrame(self, corner_radius=0)
        self.RU_frame = ctk.CTkFrame(self, corner_radius=0)

        self.LO_frame.grid(row=0, column=1, sticky="nsew", padx=5, pady=5)
        self.RO_frame.grid(row=0, column=2, sticky="nsew", padx=5, pady=5)
        self.LU_frame.grid(row=1, column=1, sticky="nsew", padx=5, pady=5)
        self.RU_frame.grid(row=1, column=2, sticky="nsew", padx=5, pady=5)

        # Plots vorbereiten
        self._init_LO_frame()
        self._init_RO_frame()
        self._init_LU_frame()
        self._init_RU_frame()
        self._init_sidebar()

        # Queue-Polling starten
        self._poll_after_id = self.after(1000, self._poll_data_q)

    def _init_sidebar(self):
        self.sidebar.grid_columnconfigure(0, weight=1)

        # img_path = Path(r"C:\KUBE_Trading\Entwicklung\Icons\Logo_final_klein.jpg")
        img_path = Path(r"C:\KUBE_Trading\Entwicklung\Icons\Logo_final.png")
        if img_path.exists():
            self.logo_img = ctk.CTkImage(light_image=Image.open(img_path),
                                         size=(self.sidebar_width, self.sidebar_width*1.4))
            self.logo = ctk.CTkLabel(self.sidebar, image=self.logo_img, text="")
        else:
            self.logo = ctk.CTkLabel(self.sidebar, text="KUBE",
                                     font=("Segoe UI", 24, "bold"))
        self.logo.grid(row=0, column=0, sticky="nsew", pady=0)

        self.btn_stop = ctk.CTkButton(self.sidebar, text="Test beenden", command=self.stop_system)
        self.btn_stop.grid(row=1, column=0, padx=10, pady=15, sticky="nwe")

        self.InfoBox = ctk.CTkFrame(self.sidebar, corner_radius=8)
        self.InfoBox.grid(row=2, column=0, padx=(10, 10), pady=(5), sticky="nwe")

        title = ctk.CTkLabel(self.InfoBox, text="Basisdaten", font=("Segoe UI", 14, "bold"))
        title.grid(row=0, column=0, columnspan=2, padx=(10, 10), pady=(5, 5), sticky="ew")

        self.Anzahl_Aktiv_EAs = ctk.CTkLabel(self.InfoBox, text="aktive EAs:", font=ctk.CTkFont(size=14))
        self.Anzahl_Aktiv_EAs.grid(row=1, column=0, padx=10, pady=0, sticky="nwe")

        self.Anzahl_Aktiv_EAsWert = ctk.CTkLabel(self.InfoBox, text="0", font=ctk.CTkFont(size=14))
        self.Anzahl_Aktiv_EAsWert.grid(row=1, column=1, padx=0, pady=0, sticky="nwe")

        self.Mutation = ctk.CTkLabel(self.InfoBox, text="Mutationen:", font=ctk.CTkFont(size=14))
        self.Mutation.grid(row=2, column=0, padx=10, pady=0, sticky="nwe")

        self.MutationWert = ctk.CTkLabel(self.InfoBox, text="0", font=ctk.CTkFont(size=14))
        self.MutationWert.grid(row=2, column=1, padx=0, pady=0, sticky="nwe")

        # --- View Range Box -------------------------------------------------
        range_frame = ctk.CTkFrame(self.sidebar, corner_radius=8)
        range_frame.grid(row=3, column=0, padx=10, pady=10, sticky="nwe")

        range_frame.grid_columnconfigure(0, weight=1)

        title = ctk.CTkLabel(range_frame, text="Darstellung", font=("Segoe UI", 14, "bold"))
        title.grid(row=0, column=0, columnspan=2, padx=(10, 10), pady=(5, 5), sticky="ew")

        ctk.CTkLabel(range_frame, text="Balance:").grid(row=1, column=0, padx=(10, 5), sticky="w")
        self.balance_range = ctk.CTkComboBox(
            range_frame,
            values=["max", "mid", "min"],
            command=self.on_range_change,
            width=80
        )
        self.balance_range.set("max")
        self.balance_range.grid(row=1, column=1, padx=(5, 10), pady=5, sticky="e")

        ctk.CTkLabel(range_frame, text="Statistik:").grid(row=2, column=0, padx=(10, 5), sticky="w")
        self.stat_range = ctk.CTkComboBox(
            range_frame,
            values=["max", "mid", "min"],
            command=self.on_range_change,
            width=80
        )
        self.stat_range.set("max")
        self.stat_range.grid(row=2, column=1, padx=(5, 10), pady=5, sticky="e")

        # --- Statistik-View Auswahlbox -----------------------------------------
        stat_view_frame = ctk.CTkFrame(self.sidebar, corner_radius=8)
        stat_view_frame.grid(row=4, column=0, padx=10, pady=(5, 10), sticky="nwe")

        ctk.CTkLabel(
            stat_view_frame,
            text="Zeige in Statistik:",
            font=("Segoe UI", 14, "bold")
        ).grid(row=0, column=0, columnspan=2, pady=(5, 5), sticky="ew")

        self.stat_view = ctk.CTkComboBox(
            stat_view_frame,
            values=["RiskManager", "Anzahl aktive EAs"],
            command=self.on_stat_view_change,
            width=170,
        )
        self.stat_view.set("RiskManager")
        self.stat_view.grid(row=1, column=0, padx=10, pady=5, sticky="ew")

    def on_stat_view_change(self, _=None):
        mode = self.stat_view.get()  # "RiskManager" oder "Anzahl aktive EAs"
        msg = {
            "topic": "stat_view_update",
            "mode": mode,
        }
        try:
            self.cmd_q.put_nowait(msg)
        except queue.Full:
            pass

        # sofortiges Redraw erzwingen (GUI-Seite)
        self._redraw_statistik()

    def create_overlay_dialog(self, title, message, buttons=["OK"], default=None):
        """
        Erstellt einen modalen Dialog als Overlay innerhalb des Hauptfensters.
        Blockiert, bis eine Auswahl getroffen wurde.
        Gibt True (Ja/OK) oder False (Nein/Abbrechen) zurück.
        """
        import tkinter as tk
        
        # 1. Overlay Frame (dunkel, halbtransparent simuliert durch dunkle Farbe)
        overlay = ctk.CTkFrame(self, fg_color="#000000", bg_color="transparent")
        overlay.place(relx=0, rely=0, relwidth=1, relheight=1)
        
        # Event abfangen, damit Klicks nicht durchgehen (simuliert modal)
        overlay.bind("<Button-1>", lambda e: "break")
        
        # 2. Dialog Box zentriert
        dialog_frame = ctk.CTkFrame(
            overlay, 
            width=420 if len(buttons) < 3 else 520, 
            height=350,  # Erhöht für bessere Lesbarkeit
            corner_radius=15,
            border_width=2,
            border_color="#00bfff",
            fg_color="#1e1e1e"
        )
        dialog_frame.place(relx=0.5, rely=0.5, anchor="center")
        dialog_frame.pack_propagate(False) 
        
        # Buttons nach unten pinnen
        btn_frame = ctk.CTkFrame(dialog_frame, fg_color="transparent")
        btn_frame.pack(side="bottom", pady=30)

        # Inhalt (Title und Message)
        content_frame = ctk.CTkFrame(dialog_frame, fg_color="transparent")
        content_frame.pack(side="top", fill="both", expand=True, padx=20, pady=(20, 0))
        
        lbl_title = ctk.CTkLabel(content_frame, text=title, font=("Segoe UI", 18, "bold"), text_color="#00bfff")
        lbl_title.pack(pady=(10, 10))
        
        lbl_msg = ctk.CTkLabel(content_frame, text=message, font=("Segoe UI", 14), wraplength=380)
        lbl_msg.pack(pady=10)
        
        # Variable für Rückgabewert (String für Button-Text)
        result_var = tk.StringVar(value="")
        
        def on_btn_click(btn_text):
            result_var.set(btn_text)
            
        # Buttons dynamisch erstellen
        for btn_text in buttons:
            is_default = (btn_text == default)
            
            # Styling basierend auf Default
            if is_default:
                fg = "#00bfff"
                hover = "#0099cc"
                border_w = 0
                border_c = None
                text_c = "white"
            else:
                fg = "transparent"
                hover = "#333333"
                border_w = 2
                border_c = "#555555"
                text_c = ("gray10", "gray90")
            
            btn = ctk.CTkButton(
                btn_frame, 
                text=btn_text, 
                command=lambda t=btn_text: on_btn_click(t), 
                width=100, 
                fg_color=fg, 
                hover_color=hover,
                border_width=border_w,
                border_color=border_c,
                text_color=text_c
            )
            btn.pack(side="left", padx=10)
        
        # Warten auf User-Input
        self.wait_variable(result_var)
        
        # Cleanup
        overlay.destroy()
        
        # Ergebnis auswerten
        selected = result_var.get()
        return selected in ["Ja", "OK"]

    def stop_system(self):
        """Beendet das gesamte KUBE Trading System sauber"""
        
        # Bestätigung vom Benutzer mit Overlay-Dialog
        confirmed = self.create_overlay_dialog(
            title="System beenden",
            message="Möchten Sie das KUBE Trading System wirklich beenden?\n\n"
                    "Dies beendet:\n"
                    "• C++ Engine (KUBE_CPP.exe)\n"
                    "• MetaTrader 5 Terminals\n"
                    "• Alle Visualisierungen",
            buttons=["Ja", "Nein", "Abbrechen"],
            default="Abbrechen"
        )
        
        if not confirmed:
            return
        
        # Shutdown asynchron starten, damit der Dialog erst schließen kann
        # und die GUI nicht einfriert
        self.after(100, self._perform_shutdown)

    def _perform_shutdown(self):
        """Führt den eigentlichen Shutdown durch"""
        import subprocess
        import time

        print("[GUI] Beende System...")
        
        # 1. Stop-Event setzen (beendet MasterLoop)
        self.stop_evt.set()
        
        # 2. Polling stoppen
        self.stop_polling()
        
        # UI Update erzwingen
        self.update()
        
        # 3. C++ Engine beenden (KUBE_CPP.exe)
        print("[GUI] Beende C++ Engine...")
        try:
            subprocess.run(
                ['taskkill', '/F', '/IM', 'KUBE_CPP.exe'],
                creationflags=subprocess.CREATE_NO_WINDOW,
                check=False
            )
        except Exception as e:
            print(f"[GUI] Fehler beim Beenden der C++ Engine: {e}")
        
        self.update()

        # 4. MetaTrader 5 Terminals beenden
        print("[GUI] Beende MetaTrader 5 Terminals...")
        try:
            subprocess.run(
                ['taskkill', '/F', '/IM', 'terminal64.exe'],
                creationflags=subprocess.CREATE_NO_WINDOW,
                check=False
            )
        except Exception as e:
            print(f"[GUI] Fehler beim Beenden der MT5 Terminals: {e}")
        
        self.update()
        time.sleep(0.5)
        
        # 5. Erfolgsmeldung anzeigen (Overlay-Dialog)
        self.create_overlay_dialog(
            title="System beenden",
            message="Das KUBE Trading System wurde erfolgreich beendet.\n\n"
                    "Alle Prozesse wurden gestoppt:\n"
                    "✓ C++ Engine\n"
                    "✓ MetaTrader 5 Terminals\n"
                    "✓ Visualisierung\n\n"
                    "Das Fenster wird jetzt geschlossen.",
            buttons=["OK"],
            default="OK"
        )
        
        # 6. GUI schließen
        print("[GUI] Schließe GUI...")
        self.destroy()
    
    def stop_polling(self):
        """
        Stoppt das Queue-Polling und bricht ausstehende after()-Callbacks ab.
        MUSS vor destroy() aufgerufen werden!
        """
        if self._poll_after_id is not None:
            try:
                self.after_cancel(self._poll_after_id)
            except:
                pass
            self._poll_after_id = None
    
    def on_close(self):
        """Handler für Fenster-Schließen"""
        print("[GUI] Fenster wird geschlossen")
        self.stop_evt.set()
        self.stop_polling()
        self.destroy()

    def on_range_change(self, _=None):
        """
        Wird aufgerufen, wenn der Nutzer in der GUI den Range (max/mid/min) ändert.
        Wir schicken ein Kommando an den MasterLoop, der daraufhin:
          - seine Range-Variablen setzt
          - intern die Timestamps zurücksetzt
          - beim nächsten Poll eine 'reset'-Payload mit neuen Daten schickt.
        """
        msg = {
            "topic": "range_update",
            "balance": self.balance_range.get(),
            "statistik": self.stat_range.get(),
        }
        try:
            self.cmd_q.put_nowait(msg)
        except queue.Full:
            pass

    # ------------------------------------------------------------------ Balance-Plot

    def _init_LO_frame(self):
        self.LO_frame.configure(fg_color=("#1e1e1e", "#1e1e1e"))

        title = ctk.CTkLabel(self.LO_frame, text="Balance-Übersicht",
                             font=("Segoe UI", 20, "bold"), text_color="#00bfff")
        title.pack(pady=(10, 5))

        self.fig_bal = Figure(figsize=(5, 3), dpi=100)
        self.ax_bal  = self.fig_bal.add_subplot(111)
        self.fig_bal.tight_layout()

        self.fig_bal.patch.set_facecolor("black")
        self.ax_bal.set_facecolor("black")

        (self.line_live,)  = self.ax_bal.plot([], [], label="LIVE",  color="yellow",  linewidth=1.6)
        (self.line_paper,) = self.ax_bal.plot([], [], label="PAPER", color="#00BFFF", linewidth=1.6)

        self.ax_bal.tick_params(colors="white")
        for sp in self.ax_bal.spines.values():
            sp.set_color("white")

        self.ax_bal.grid(True, which="both", linestyle="--", alpha=0.25, color="white")
        self.ax_bal.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%m-%d %H:%M"))
        self.ax_bal.xaxis.set_major_locator(mdates.AutoDateLocator())

        leg = self.ax_bal.legend(loc="upper left")
        leg.get_frame().set_facecolor("black")
        leg.get_frame().set_edgecolor("white")
        for txt in leg.get_texts():
            txt.set_color("white")

        self.canvas_bal = FigureCanvasTkAgg(self.fig_bal, master=self.LO_frame)
        self.canvas_bal.get_tk_widget().pack(expand=True, fill="both", padx=10, pady=10)
        self.canvas_bal.draw_idle()

    # ------------------------------------------------------------------ Statistik-Plot

    def _init_RO_frame(self):
        self.RO_frame.configure(fg_color=("#1e1e1e", "#1e1e1e"))

        self.title = ctk.CTkLabel(self.RO_frame, text="Statistik (RiskManager)",
                                  font=("Segoe UI", 20, "bold"), text_color="#00bfff")
        self.title.pack(pady=(10, 5))

        self.fig_sta = Figure(figsize=(5, 3), dpi=100)
        self.ax_sta  = self.fig_sta.add_subplot(111)
        self.fig_sta.tight_layout()

        self.fig_sta.patch.set_facecolor("black")
        self.ax_sta.set_facecolor("black")

        (self.line_peak,)      = self.ax_sta.plot([], [], label="Peak",           color="orange",  linewidth=1.4)
        (self.line_floor,)     = self.ax_sta.plot([], [], label="Floor",          color="#aaaaaa", linewidth=1.2)
        (self.line_paper_sta,) = self.ax_sta.plot([], [], label="Equity (Paper)", color="#00BFFF", linewidth=1.6)
       
        # # -------- zweite Achse für Lambda --------
        # self.ax_lambda = self.ax_sta.twinx()
        # self.ax_lambda.set_ylabel("lambda", color="#AA00FF")
        # self.ax_lambda.tick_params(axis="y", colors="#AA00FF")

        # # feste Skalierung
        # self.ax_lambda.set_ylim(0.000, 0.0021)

        # # Lambda-Linie
        # (self.line_lambda,) = self.ax_lambda.plot(
        #     [], [], color="#AA00FF", linewidth=1.5, label="lambda"
        # )


        self.ax_sta.tick_params(colors="white")
        for sp in self.ax_sta.spines.values():
            sp.set_color("white")

        self.ax_sta.grid(True, which="both", linestyle="--", alpha=0.25, color="white")
        self.ax_sta.xaxis.set_major_formatter(mdates.DateFormatter("%Y-%m-%d %H:%M"))
        self.ax_sta.xaxis.set_major_locator(mdates.AutoDateLocator())

        leg = self.ax_sta.legend(loc="upper left")
        leg.get_frame().set_facecolor("black")
        leg.get_frame().set_edgecolor("white")
        for txt in leg.get_texts():
            txt.set_color("white")
        
        self.txt_active_box = self.ax_sta.text(
            0.14, 0.98,                     # <-- x etwas rechts von der Legende
            "Aktiv: 0\nfrac: 0.0000\nλ: 0.0000",
            transform=self.ax_sta.transAxes,
            ha="left",                      # linksbündig, damit Text nach rechts wächst
            va="top",
            fontsize=10,
            fontweight="bold",
            color="white",
            bbox=dict(
                facecolor="black",
                edgecolor="white",
                boxstyle="round,pad=0.3",
                alpha=0.8,
            ),
        )

        self.canvas_sta = FigureCanvasTkAgg(self.fig_sta, master=self.RO_frame)
        self.canvas_sta.get_tk_widget().pack(expand=True, fill="both", padx=10, pady=10)
        self.canvas_sta.draw_idle()

    # ------------------------------------------------------------------ 3D-View (LU_frame)

    def _init_LU_frame(self):
        self.LU_frame.configure(fg_color=("#1e1e1e", "#1e1e1e"))

        title = ctk.CTkLabel(self.LU_frame, text="3D Parameter View (Strategie-Input)",
                             font=("Segoe UI", 20, "bold"), text_color="#00bfff")
        title.pack(pady=(10, 5))

        # Create Figure and 3D Axis
        self.fig_3d = Figure(figsize=(5, 4), dpi=100)
        self.fig_3d.patch.set_facecolor("black")
        
        self.ax_3d = self.fig_3d.add_subplot(111, projection='3d')
        self.ax_3d.set_facecolor("black")
        
        # Style the axes
        self.ax_3d.xaxis.label.set_color('white')
        self.ax_3d.yaxis.label.set_color('white')
        self.ax_3d.zaxis.label.set_color('white')
        
        self.ax_3d.tick_params(axis='x', colors='white')
        self.ax_3d.tick_params(axis='y', colors='white')
        self.ax_3d.tick_params(axis='z', colors='white')
        
        self.ax_3d.set_xlabel(self.axis_labels[0])
        self.ax_3d.set_ylabel(self.axis_labels[1])
        self.ax_3d.set_zlabel(self.axis_labels[2])

        
        # Make the panes transparent or dark
        self.ax_3d.xaxis.pane.fill = False
        self.ax_3d.yaxis.pane.fill = False
        self.ax_3d.zaxis.pane.fill = False
        
        # Initial empty scatter plot
        self.scatter = self.ax_3d.scatter([], [], [], c=[], cmap='viridis', marker='o')

        # Embed in Tkinter
        self.canvas_3d = FigureCanvasTkAgg(self.fig_3d, master=self.LU_frame)
        self.canvas_3d.get_tk_widget().pack(expand=True, fill="both", padx=10, pady=10)
        
        # Einmaliges Laden der Daten
        self._update_3d_view()

    def _update_3d_view(self):
        """
        Liest die Swarm-Daten aus der DB und aktualisiert den 3D-Plot.
        Aktive EAs = Gelb, Inaktive = Grau halbtransparent.
        """
        try:
            conn = sqlite3.connect(self.db_path)
            # 'aktiv' Spalte mit abfragen
            df = pd.read_sql_query("SELECT param1, param2, param3, swarm, aktiv FROM swarms WHERE swarm = 0", conn)
            conn.close()

            if df.empty:
                return

            self.ax_3d.clear()
            
            # Styles wiederherstellen (clear() löscht sie)
            self.ax_3d.set_facecolor("black")
            self.ax_3d.set_xlabel(self.axis_labels[0], color='white')
            self.ax_3d.set_ylabel(self.axis_labels[1], color='white')
            self.ax_3d.set_zlabel(self.axis_labels[2], color='white')

            self.ax_3d.tick_params(axis='x', colors='white')
            self.ax_3d.tick_params(axis='y', colors='white')
            self.ax_3d.tick_params(axis='z', colors='white')
            self.ax_3d.xaxis.pane.fill = False
            self.ax_3d.yaxis.pane.fill = False
            self.ax_3d.zaxis.pane.fill = False

            # Daten aufteilen
            mask_active = (df['aktiv'] == 1)
            df_active   = df[mask_active]
            df_inactive = df[~mask_active]

            # 1) Inaktive: Grau, transparent
            if not df_inactive.empty:
                self.ax_3d.scatter(
                    df_inactive['param1'], 
                    df_inactive['param2'], 
                    df_inactive['param3'],
                    c='gray', 
                    marker='o', 
                    s=20, 
                    alpha=0.50  # stark transparent
                )

            # 2) Aktive: Gelb, deckend
            if not df_active.empty:
                self.ax_3d.scatter(
                    df_active['param1'], 
                    df_active['param2'], 
                    df_active['param3'],
                    c='yellow', 
                    marker='o', 
                    s=30,       # etwas größer
                    alpha=1.0
                )
            
            self.canvas_3d.draw_idle()
            # print("[Gui] 3D View Updated (Active/Inactive)")

        except Exception as e:
            print(f"[Gui] Error updating 3D view: {e}")

    def _update_active_eas_label(self, active_count: int) -> None:
        if active_count < 0:
            active_count = 0
        self.Anzahl_Aktiv_EAsWert.configure(text=f"{active_count}")
        self.active_eas = active_count

    # ------------------------------------------------------------------ Queue-Polling

    def _poll_data_q(self):
        try:
            while True:
                msg = self.data_q.get_nowait()
                topic = msg.get("topic")

                if topic == "balance_delta":
                    reset = msg.get("reset", False)
                    self._apply_balance_delta(msg.get("rows", []), reset)

                elif topic == "statistik_delta":
                    swarm_id = msg.get("swarm", 10)
                    reset = msg.get("reset", False)
                    self._apply_statistik_delta(msg.get("rows", []), swarm_id, reset)

                    ac = msg.get("active_count")
                    if ac is not None:
                        try:
                            self._update_active_eas_label(int(ac))
                        except (ValueError, TypeError):
                            pass

                    af = msg.get("active_frac")
                    if af is not None:
                        try:
                            self.active_frac = float(af)
                        except (ValueError, TypeError):
                            pass

                    lam = msg.get("lambda")     # ★ neu: lambda_dyn aus MasterLoop
                    if lam is not None:
                        try:
                            self.lambda_val = float(lam)
                        except (ValueError, TypeError):
                            pass
                    
                    # ★ Update 3D-View bei jedem Statistik-Schritt (neue Bar)
                    self._update_3d_view()

                elif topic == "active_eas_delta":
                    reset = msg.get("reset", False)
                    self._apply_active_eas_delta(msg.get("rows", []), reset)

                elif topic == "update_3d_view":
                    self._update_3d_view()

        except queue.Empty:
            pass
        except Exception as e:
            import traceback
            print("[GUI] Exception in _poll_data_q:", e)
            traceback.print_exc()

        self._redraw_balance()
        self._redraw_statistik()
        
        self._update_fitness_3d_view()

        if not self.stop_evt.is_set():
            self._poll_after_id = self.after(1000, self._poll_data_q)

  

    def _apply_balance_delta(self, rows, reset: bool = False):
        """
        rows: [(schwarm, ts, balance_cents), ...]
        ts hat das string-Format "YYYY.MM.DD HH:MM".
        reset=True  -> Caches vorher leeren (z.B. nach Range-Wechsel oder Programmstart)
    
        Aktualisiert self.last_timestamp (im String-Format) mit dem neuesten gefundenen Timestamp in 'rows'.
        """
    
        # Das erwartete Format des Timestamps-Strings
        TS_FORMAT = "%Y.%m.%d %H:%M"
    
        if reset:
            self.cache_live.clear()
            self.cache_paper.clear()

        # 1. Startwert für den Vergleich vorbereiten
        # Wenn self.last_timestamp existiert, wird es in ein datetime-Objekt konvertiert,
        # andernfalls wird None verwendet.
        current_last_ts_str = getattr(self, 'last_timestamp', None)
    
        if current_last_ts_str:
            # Konvertiere den existierenden String in ein vergleichbares datetime-Objekt
            newest_timestamp_dt = datetime.strptime(current_last_ts_str, TS_FORMAT)
        else:
            # Starte mit None, wenn noch kein Timestamp gespeichert ist
            newest_timestamp_dt = None
    
        # Speichervariable für den neuesten Timestamp im String-Format (wird am Ende gespeichert)
        newest_timestamp_str = current_last_ts_str
        
        # 2. Iteriere über die Daten und verarbeite sie
        for s, ts_str, cents in rows:
        
            # Den aktuellen String-Timestamp in ein datetime-Objekt konvertieren
            try:
                current_ts_dt = datetime.strptime(ts_str, TS_FORMAT)
            except ValueError as e:
                print(f"[Gui] Fehler bei der Konvertierung des Timestamps '{ts_str}': {e}")
                continue # Überspringe diese Zeile, wenn das Format falsch ist
            
            val = float(cents) / self.cent_divisor - self.start_capital.get(s, 0.0)
        
            # Caching-Logik
            if s == 0:
                self.cache_live.append((ts_str, val))
            elif s == 10:
                self.cache_paper.append((ts_str, val))
            
            # 3. Den größten/neuesten Timestamp finden
            if newest_timestamp_dt is None or current_ts_dt > newest_timestamp_dt:
                newest_timestamp_dt = current_ts_dt
                newest_timestamp_str = ts_str # Speichere den zugehörigen String
            
        # 4. self.last_timestamp aktualisieren (nur wenn ein Wert gefunden wurde)
        if newest_timestamp_str is not None:
            self.last_timestamp = newest_timestamp_str

    def _apply_statistik_delta(self, rows, swarm_id: int, reset: bool = False):
        """
        rows: [(ts, eq_c, pk_c, fl_c, active_frac, lambda_dyn), ...]
        reset=True -> Caches vorher leeren
        """
        if reset:
            self.cache_peak.clear()
            self.cache_floor.clear()
            self.cache_paper_sta.clear()
            self.cache_lambda.clear()    # ★ neu

        start_cap = self.start_capital.get(swarm_id, 0.0)
        div = self.cent_divisor

        for ts, eq_c, pk_c, fl_c, af, lam in rows:
            self.cache_peak.append((ts,  (pk_c / div) - start_cap))
            self.cache_floor.append((ts, (fl_c / div) - start_cap))
            self.cache_paper_sta.append((ts, (eq_c / div) - start_cap))
            self.cache_lambda.append((ts, float(lam)))     # ★ λ unskaliert speichern


    # Placeholder – du hast die Implementierung schon, ich lasse sie unverändert
    def _apply_active_eas_delta(self, rows, reset: bool = False):
        if reset:
            self.cache_active_eas.clear()
        for ts, val in rows:
            self.cache_active_eas.append((ts, float(val)))

    # ------------------------------------------------------------------ Hilfsfunktionen & Zeichnen

    @staticmethod
    def _parse_ts(ts: str) -> datetime:
        for fmt in ("%Y.%m.%d %H:%M:%S", "%Y.%m.%d %H:%M",
                    "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M"):
            try:
                return datetime.strptime(ts, fmt)
            except ValueError:
                continue
        return datetime.fromisoformat(ts.replace(".", "-"))

    def _to_xy(self, series):
        xs, ys = [], []
        for ts, val in series:
            dt = self._parse_ts(ts)
            xs.append(mdates.date2num(dt))
            ys.append(float(val))
        return xs, ys

    def _downsample(self, series, limit: int):
        n = len(series)
        if n <= limit or limit <= 0:
            return series
        step = n / float(limit)
        idxs = [int(i * step) for i in range(limit)]
        idxs[-1] = n - 1
        return [series[i] for i in idxs]

    def _select_view_series(self, series, mode: str):
        n = len(series)
        if n == 0:
            return series

        if mode == "max":
            return self._downsample(series, self.max_points_mid)
        elif mode == "mid":
            if n <= self.max_points_mid:
                return series
            return series[-self.max_points_mid:]
        elif mode == "min":
            if n <= self.max_points_min:
                return series
            return series[-self.max_points_min:]
        else:
            return series

    def _redraw_balance(self):
        mode = self.balance_range.get()

        live_series  = self._select_view_series(self.cache_live,  mode)
        paper_series = self._select_view_series(self.cache_paper, mode)

        live_xy  = self._to_xy(live_series)   if live_series  else ([], [])
        paper_xy = self._to_xy(paper_series)  if paper_series else ([], [])

        self.line_live.set_data(*live_xy)
        self.line_paper.set_data(*paper_xy)

        all_x = (live_xy[0] or []) + (paper_xy[0] or [])
        all_y = (live_xy[1] or []) + (paper_xy[1] or [])
        if all_x and all_y:
            xmin, xmax = min(all_x), max(all_x)
            ymin, ymax = min(all_y), max(all_y)
            xr = (xmax - xmin) * 0.03 if xmax > xmin else 1
            yr = (ymax - ymin) * 0.10 if ymax > ymin else 1
            self.ax_bal.set_xlim(xmin - xr, xmax + xr)
            self.ax_bal.set_ylim(ymin - yr, ymax + yr)

        self.fig_bal.autofmt_xdate()
        self.canvas_bal.draw_idle()

    def _redraw_statistik(self):
        """
        Zeichnet das rechte obere Statistik-Fenster (RO_frame) abhängig von:
        - self.stat_view_mode: "RiskManager" oder "Anzahl aktive EAs"
        - self.stat_range:     "max", "mid", "min"
        """

        mode = self.stat_range.get()  # "max" | "mid" | "min"

        # ------------------------------
        # OPTION A – RiskManager
        # ------------------------------
        if self.stat_view_mode == "RiskManager":
            self.ax_sta.set_title("Statistik (RiskManager)", color="white")

            peak_series  = self._select_view_series(self.cache_peak,      mode)
            floor_series = self._select_view_series(self.cache_floor,     mode)
            paper_series = self._select_view_series(self.cache_paper_sta, mode)
            # lambda_series = self._select_view_series(self.cache_lambda,   mode)

            peak_xy  = self._to_xy(peak_series)     if peak_series     else ([], [])
            floor_xy = self._to_xy(floor_series)    if floor_series    else ([], [])
            paper_xy = self._to_xy(paper_series)    if paper_series    else ([], [])
            # lambda_xy = self._to_xy(lambda_series)  if lambda_series   else ([], [])


            self.line_peak.set_data(*peak_xy)
            self.line_floor.set_data(*floor_xy)
            self.line_paper_sta.set_data(*paper_xy)
            # self.line_lambda.set_data(*lambda_xy)   # ★

            if hasattr(self, "line_active"):
                self.line_active.set_data([], [])

            # Hauptachse (Peak/Floor/Equity)
            all_x = (peak_xy[0] or []) + (floor_xy[0] or []) + (paper_xy[0] or [])
            all_y = (peak_xy[1] or []) + (floor_xy[1] or []) + (paper_xy[1] or [])

            if all_x and all_y:
                xmin, xmax = min(all_x), max(all_x)
                ymin, ymax = min(all_y), max(all_y)
                xr = (xmax - xmin) * 0.03 if xmax > xmin else 1
                yr = (ymax - ymin) * 0.10 if ymax > ymin else 1
                self.ax_sta.set_xlim(xmin - xr, xmax + xr)
                self.ax_sta.set_ylim(ymin - yr, ymax + yr)

            # # feste Achsenskala für lambda
            #     self.ax_lambda.set_ylim(0.000, 0.0021)
            #     self.ax_lambda.set_xlim(self.ax_sta.get_xlim())

        # ------------------------------
        # OPTION B – Anzahl aktive EAs
        # ------------------------------
        else:
            self.ax_sta.set_title("Anzahl aktive EAs (Timeline)", color="white")

            active_series = self._select_view_series(self.cache_active_eas, mode)
            active_xy     = self._to_xy(active_series) if active_series else ([], [])

            if hasattr(self, "line_active"):
                self.line_active.set_data(*active_xy)
            else:
                (self.line_active,) = self.ax_sta.plot([], [], color="cyan", linewidth=1.6)

            self.line_peak.set_data([], [])
            self.line_floor.set_data([], [])
            self.line_paper_sta.set_data([], [])

            xs, ys = active_xy
            if xs and ys:
                xmin, xmax = min(xs), max(xs)
                ymin, ymax = min(ys), max(ys)
                xr = (xmax - xmin) * 0.03 if xmax > xmin else 1
                yr = max(1, (ymax - ymin) * 0.10)
                self.ax_sta.set_xlim(xmin - xr, xmax + xr)
                self.ax_sta.set_ylim(ymin - yr, ymax + yr)

        # Info-Box aktualisieren (jetzt inkl. λ)
        self.txt_active_box.set_text(
            f"Aktiv: {self.active_eas}\n"
            f"frac: {self.active_frac:.4f}\n"
            f"λ: {self.lambda_val:.6f}"
        )

        self.fig_sta.autofmt_xdate()
        self.canvas_sta.draw_idle()
    

    def read_lookback_from_mqh(self,path: str, default: int = 500) -> int:
        """
        Liest aus einer MQL-Konfigurationsdatei die Zeile
            int c_fitness_lookback_EAs = 240;
        und gibt den Wert (hier 240) als int zurück.
        Fällt zurück auf 'default', wenn nichts gefunden wird.
        """
        p = Path(path)
        if not p.exists():
            print(f"[Gui] WARNUNG: config.mqh '{path}' nicht gefunden. Nutze Default {default}.")
            return default

        try:
            txt = p.read_text(encoding="utf-8")
        except OSError as e:
            print(f"[Gui] WARNUNG: Konnte '{path}' nicht lesen ({e}). Nutze Default {default}.")
            return default

        m = re.search(r"c_fitness_lookback_EAs\s*=\s*([0-9]+)", txt)
        if m:
            val = int(m.group(1))
            print(f"[Gui] c_fitness_lookback_EAs aus config.mqh: {val}")
            return val

        print(f"[Gui] WARNUNG: c_fitness_lookback_EAs nicht in '{path}' gefunden. Nutze Default {default}.")
        return default

    def read_axis_labels_from_mqh(self, path: str) -> list[str]:
        """
        Liest c_Achsenbeschriftung[] aus der MQL-Konfig.
        Erwartet Format:
           string c_Achsenbeschriftung[] = {
              "Label1",
              "Label2",
              "Label3"
           };
        Gibt Liste mit 3 Strings zurück. Default: ["Param 1", "Param 2", "Param 3"]
        """
        defaults = ["Param 1", "Param 2", "Param 3"]
        p = Path(path)
        if not p.exists():
            return defaults

        try:
            txt = p.read_text(encoding="utf-8")
        except OSError:
            return defaults

        # Suche nach dem Array-Inhalt zwischen { und }
        # pattern: string c_Achsenbeschriftung[] = { ... };
        # Wir suchen non-greedy bis };
        pattern = r'string\s+c_Achsenbeschriftung\[\]\s*=\s*\{(.*?)\};'
        m = re.search(pattern, txt, re.DOTALL)
        if not m:
            print(f"[Gui] c_Achsenbeschriftung nicht gefunden in {path}")
            return defaults

        content = m.group(1)
        # Split am Komma, bereinige Quotes und Whitespace
        # Bsp: "StopLoss (x10)", "TakeProfit (x10)", "DonchianLookback"
        parts = [p.strip() for p in content.split(',')]
        
        labels = []
        for p in parts:
            # Entferne Anführungszeichen
            clean = p.strip('"').strip("'").strip()
            if clean:
                labels.append(clean)
        
        # Auffüllen oder Kürzen auf 3 Elemente
        if len(labels) < 3:
            labels.extend(defaults[len(labels):])
        return labels[:3]
        # ------------------------------------------------------------------ Fitness-3D-View (RU_frame)

    def _fetch_fitness_data(self):
        """
        Holt ALLE Zeilen aus Fitness_proBar, die den 'ts_key' aus self.last_timestamp haben:
            NetProfitNorm, ActivityNorm, R, Pareto1
        und gibt sie als Liste von Tupeln zurück (älteste zuerst).
        """
        timestamp = self.last_timestamp
        try:
            conn = sqlite3.connect(self.db_fitness_path, timeout=1.0)
            cur = conn.cursor()
            cur.execute(
                """
                SELECT NetProfitNorm, ActivityNorm, R, Pareto1
                FROM Fitness_proBar
                WHERE ts_key = ?             -- Filterung nach Timestamp (Korrigierte Syntax)
                ORDER BY rowid ASC           -- Sortierung vom Ältesten zum Neuesten
                """,
                (timestamp,) # Übergabe des Timestamp-Wertes als Parameter-Tupel
            )
            rows = cur.fetchall()
            conn.close()
        
            # Die Daten sind bereits in der gewünschten Reihenfolge (älteste zuerst) 
            # durch 'ORDER BY rowid ASC', daher ist [::-1] NICHT mehr notwendig.
            print("*"*50)
            print(f"Länge des Fitness-Datensatzes für ts_key '{timestamp}': {len(rows)}")
            print("*"*50)
            return rows
        
        except sqlite3.Error as e:
            print(f"[Gui] Database error in _fetch_fitness_data: {e}")
            return []


    def _init_RU_frame(self):
        """
        Rechts unten: 3D-Scatterplot der Fitness (NetProfitNorm, ActivityNorm, R)
        Initialisiert die 3D-Achsen und den leeren Scatter-Plot.
        """
        import customtkinter as ctk
        from matplotlib.figure import Figure
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    
        self.RU_frame.configure(fg_color=("#1e1e1e", "#1e1e1e"))

        title = ctk.CTkLabel(
            self.RU_frame,
            text="3D Parameter View (Fitness Output)",
            font=("Segoe UI", 20, "bold"),
            text_color="#00bfff"
        )
        title.pack(pady=(10, 5))

        # Figure + 3D-Achse
        self.fig_fit3d = Figure(figsize=(5, 4), dpi=100)
        self.fig_fit3d.patch.set_facecolor("black")

        # Wichtig: projection="3d"
        self.ax_fit3d = self.fig_fit3d.add_subplot(111, projection="3d")
        self.ax_fit3d.set_facecolor("black")

        # Grund-Layout / Styling
        # self.ax_fit3d.set_title("Fitness 3D (Experts)", color="white")
        self.ax_fit3d.set_xlabel("NetProfitNorm", color="white")
        self.ax_fit3d.set_ylabel("ActivityNorm", color="white")
        self.ax_fit3d.set_zlabel("R", color="white")

        self.ax_fit3d.tick_params(axis='x', colors='white')
        self.ax_fit3d.tick_params(axis='y', colors='white')
        self.ax_fit3d.tick_params(axis='z', colors='white')

        # Gitter / Pane-Farben
        self.ax_fit3d.xaxis.pane.fill = False
        self.ax_fit3d.yaxis.pane.fill = False
        self.ax_fit3d.zaxis.pane.fill = False
        for axis in [self.ax_fit3d.xaxis, self.ax_fit3d.yaxis, self.ax_fit3d.zaxis]:
            axis._axinfo["grid"]['color'] = (1, 1, 1, 0.2)

        # Limits für normalisierte Daten
        self.ax_fit3d.set_xlim(0, 1.1)
        self.ax_fit3d.set_ylim(0, 1.1)
        self.ax_fit3d.set_zlim(0, 1.1)

        # Blickwinkel wie im Standalone: Nur Elevation setzen, Azimut bleibt Standard
        # self.ax_fit3d.view_init(elev=30)

        # Initialer leerer Scatter – WICHTIG: Erstellung des Objekts zur späteren Aktualisierung
        self.sc_fit = self.ax_fit3d.scatter(
            [], [], [],
            color='gray',
            marker='o',
            s=20,
            alpha=0.5
        )
    
        # In Tk einbetten
        self.canvas_fit3d = FigureCanvasTkAgg(self.fig_fit3d, master=self.RU_frame)
        self.canvas_fit3d.get_tk_widget().pack(expand=True, fill="both", padx=10, pady=10)
        self.canvas_fit3d.draw_idle()    


    def _update_fitness_3d_view(self):
        """
        Aktualisiert den 3D-Fitness-Scatter im RU_frame.
        Wird z.B. aus _poll_data_q zyklisch aufgerufen.
        Pareto Front 1 Punkte werden gelb hervorgehoben, andere grau.
        """
        # Falls die Achse/Figur noch nicht da ist (z.B. am Anfang), abbrechen
        if not hasattr(self, "ax_fit3d"):
            return

        data = self._fetch_fitness_data()
        if not data:
            # nichts zu plotten
            return

        # Daten aufteilen: (NetProfitNorm, ActivityNorm, R, Pareto1)
        xs = [r[0] for r in data]
        ys = [r[1] for r in data]
        zs = [r[2] for r in data]
        pareto_flags = [r[3] if len(r) > 3 else 0 for r in data]  # Fallback für alte Daten

        # Achse clearen und Style neu setzen (wie im Standalone)
        self.ax_fit3d.clear()

        self.ax_fit3d.set_facecolor("black")
        # self.ax_fit3d.set_title("Fitness 3D (Experts)", color="white")
        self.ax_fit3d.set_xlabel("Profit", color="white")
        self.ax_fit3d.set_ylabel("Activity", color="white")
        self.ax_fit3d.set_zlabel("Risiko", color="white")

        self.ax_fit3d.tick_params(axis='x', colors='white')
        self.ax_fit3d.tick_params(axis='y', colors='white')
        self.ax_fit3d.tick_params(axis='z', colors='white')

        self.ax_fit3d.xaxis.pane.fill = False
        self.ax_fit3d.yaxis.pane.fill = False
        self.ax_fit3d.zaxis.pane.fill = False
        for axis in [self.ax_fit3d.xaxis, self.ax_fit3d.yaxis, self.ax_fit3d.zaxis]:
            axis._axinfo["grid"]['color'] = (1, 1, 1, 0.2)

        # feste Limits beibehalten
        self.ax_fit3d.set_xlim(0, 1.1)
        self.ax_fit3d.set_ylim(0, 1.1)
        self.ax_fit3d.set_zlim(0, 1.1)

        # Blickwinkel
        # self.ax_fit3d.view_init(elev=30)

        # Daten nach Pareto-Flag aufteilen
        xs_pareto1 = [xs[i] for i in range(len(xs)) if pareto_flags[i] == 1]
        ys_pareto1 = [ys[i] for i in range(len(ys)) if pareto_flags[i] == 1]
        zs_pareto1 = [zs[i] for i in range(len(zs)) if pareto_flags[i] == 1]

        xs_other = [xs[i] for i in range(len(xs)) if pareto_flags[i] != 1]
        ys_other = [ys[i] for i in range(len(ys)) if pareto_flags[i] != 1]
        zs_other = [zs[i] for i in range(len(zs)) if pareto_flags[i] != 1]

        # 1) Andere Punkte: Grau, transparent
        if xs_other:
            self.ax_fit3d.scatter(
                xs_other, ys_other, zs_other,
                color='gray',
                marker='o',
                s=20,
                alpha=0.5,
                label='Other EAs'
            )

        # 2) Pareto Front 1: Gelb/Gold, größer, deckend
        if xs_pareto1:
            self.ax_fit3d.scatter(
                xs_pareto1, ys_pareto1, zs_pareto1,
                color='gold',
                marker='o',
                s=40,
                alpha=1.0,
                edgecolors='orange',
                linewidths=1,
                label='Pareto Front 1'
            )

        # Legende hinzufügen
        if xs_pareto1 or xs_other:
            legend = self.ax_fit3d.legend(loc='upper left', framealpha=0.8)
            legend.get_frame().set_facecolor('black')
            legend.get_frame().set_edgecolor('white')
            for text in legend.get_texts():
                text.set_color('white')

        self.canvas_fit3d.draw_idle()
