
# Gui.py
import customtkinter as ctk
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
        self._config_path = r"C:\MQL_Shared\Includes\Config1\KUBE_config.mqh"

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
        self.cache_fitness:     list[tuple[float, float, float]] = [] # (NetProfitNorm, ActivityNorm, R)


        # Normalisierung
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
        self.after(1000, self._poll_data_q)

    def _init_sidebar(self):
        self.sidebar.grid_columnconfigure(0, weight=1)

        img_path = Path(r"C:\KUBE_Trading\Entwicklung\Icons\Logo_final_klein.jpg")
        if img_path.exists():
            self.logo_img = ctk.CTkImage(light_image=Image.open(img_path),
                                         size=(self.sidebar_width, self.sidebar_width))
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

    def stop_system(self):
        print("[GUI] Stop-Button – Event gesetzt")
        self.stop_evt.set()

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
        # (self.line_paper_sta,) = self.ax_sta.plot([], [], label="Equity (Paper)", color="#00BFFF", linewidth=1.6)
        ################

        # -------- zweite Achse für Lambda --------
        self.ax_lambda = self.ax_sta.twinx()
        self.ax_lambda.set_ylabel("lambda", color="#AA00FF")
        self.ax_lambda.tick_params(axis="y", colors="#AA00FF")

        # feste Skalierung
        self.ax_lambda.set_ylim(0.000, 0.0021)

        # Lambda-Linie
        (self.line_lambda,) = self.ax_lambda.plot(
            [], [], color="#AA00FF", linewidth=1.5, label="lambda"
        )

        #################
        # # ★ zweite y-Achse für λ
        # self.ax_lambda = self.ax_sta.twinx()
        # self.ax_lambda.set_ylabel("λ", color="magenta")
        # self.ax_lambda.tick_params(axis="y", colors="magenta")
        # for sp in self.ax_lambda.spines.values():
        #     sp.set_color("magenta")

        # (self.line_lambda,) = self.ax_lambda.plot([], [], label="lambda", color="magenta", linewidth=1.2)


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
        ####################
        
        #################

        # Info-Box oben rechts
        # self.txt_active_box = self.ax_sta.text(
        #     0.98, 0.98,
        #     "Aktiv: 0",
        #     transform=self.ax_sta.transAxes,
        #     ha="right",
        #     va="top",
        #     fontsize=10,
        #     fontweight="bold",
        #     color="white",
        #     bbox=dict(
        #         facecolor="black",
        #         edgecolor="white",
        #         boxstyle="round,pad=0.3",
        #         alpha=0.8,
        #     ),
        # )
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

        title = ctk.CTkLabel(self.LU_frame, text="3D Parameter View (Swarms)",
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
        rows: [(schwarm, ts, balance_cents), ...]
        reset=True  -> Caches vorher leeren (z.B. nach Range-Wechsel oder Programmstart)
        """
        if reset:
            self.cache_live.clear()
            self.cache_paper.clear()

        for s, ts, cents in rows:
            val = float(cents) / self.cent_divisor - self.start_capital.get(s, 0.0)
            if s == 0:
                self.cache_live.append((ts, val))
            elif s == 10:
                self.cache_paper.append((ts, val))

    # def _apply_statistik_delta(self, rows, swarm_id: int, reset: bool = False):
    #     """
    #     rows: [(ts, eq_c, pk_c, fl_c), ...]
    #     reset=True -> Caches vorher leeren
    #     """
    #     if reset:
    #         self.cache_peak.clear()
    #         self.cache_floor.clear()
    #         self.cache_paper_sta.clear()
    #
    #     start_cap = self.start_capital.get(swarm_id, 0.0)
    #     div = self.cent_divisor
    #
    #     for ts, eq_c, pk_c, fl_c in rows:
    #         self.cache_peak.append((ts,  (pk_c / div) - start_cap))
    #         self.cache_floor.append((ts, (fl_c / div) - start_cap))
    #         self.cache_paper_sta.append((ts, (eq_c / div) - start_cap))
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
            lambda_series = self._select_view_series(self.cache_lambda,   mode)

            peak_xy  = self._to_xy(peak_series)     if peak_series     else ([], [])
            floor_xy = self._to_xy(floor_series)    if floor_series    else ([], [])
            paper_xy = self._to_xy(paper_series)    if paper_series    else ([], [])
            lambda_xy = self._to_xy(lambda_series)  if lambda_series   else ([], [])


            self.line_peak.set_data(*peak_xy)
            self.line_floor.set_data(*floor_xy)
            self.line_paper_sta.set_data(*paper_xy)
            self.line_lambda.set_data(*lambda_xy)   # ★

            if hasattr(self, "line_active"):
                self.line_active.set_data([], [])

            # all_x = (peak_xy[0] or []) + (floor_xy[0] or []) + (paper_xy[0] or [])
            # all_y = (peak_xy[1] or []) + (floor_xy[1] or []) + (paper_xy[1] or [])

            # if all_x and all_y:
            #     xmin, xmax = min(all_x), max(all_x)
            #     ymin, ymax = min(all_y), max(all_y)
            #     xr = (xmax - xmin) * 0.03 if xmax > xmin else 1
            #     yr = (ymax - ymin) * 0.10 if ymax > ymin else 1
            #     self.ax_sta.set_xlim(xmin - xr, xmax + xr)
            #     self.ax_sta.set_ylim(ymin - yr, ymax + yr)
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

            # # Rechte Achse für λ dynamisch an λ-Werten ausrichten
            # lx, ly = lambda_xy
            # if lx and ly:
            #     lam_min = min(ly)
            #     lam_max = max(ly)
            #     if lam_min == lam_max:
            #         lam_min -= 0.0001
            #         lam_max += 0.0001
            #     pad = (lam_max - lam_min) * 0.1
            #     self.ax_lambda.set_ylim(lam_min - pad, lam_max + pad)
            #     # X-Range gleich wie Hauptachse
            # feste Achsenskala für lambda
                self.ax_lambda.set_ylim(0.000, 0.0021)
                self.ax_lambda.set_xlim(self.ax_sta.get_xlim())

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
