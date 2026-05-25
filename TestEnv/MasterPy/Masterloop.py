
#     # MasterLoop.py
# import sqlite3
# import time
# import queue
# import threading
# import re
# from typing import List, Tuple, Optional, Dict, Any

# BalanceRow = Tuple[int, str, int]
# RiskRow    = Tuple[str, int, int, int]


# def read_lookback_from_mqh(path: str) -> int:
#     try:
#         with open(path, "r", encoding="utf-8") as f:
#             txt = f.read()
#     except OSError:
#         print(f"[MasterLoop] WARNUNG: config.mqh '{path}' nicht gefunden. Nutze Default 500.")
#         return 500

#     m = re.search(r"c_fitness_lookback_EAs\s*=\s*([0-9]+)", txt)
#     if m:
#         val = int(m.group(1))
#         print(f"[MasterLoop] c_fitness_lookback_EAs aus config.mqh: {val}")
#         return val

#     print(f"[MasterLoop] WARNUNG: c_fitness_lookback_EAs nicht in '{path}' gefunden. Nutze Default 500.")
#     return 500


# class MasterLoop:
#     def __init__(self, data_q: queue.Queue, cmd_q: queue.Queue, stop_evt: threading.Event):
#         self.data_q = data_q
#         self.cmd_q  = cmd_q
#         self.stop   = stop_evt

#         self._db_swarm_path   = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Schwarm.db"
#         self._table_balance   = "balance"
#         self._risk_table      = "risk_state"
#         self._poll_interval_s = 1.0

#         self._config_path  = r"C:\MQL_Shared\Includes\Config1\KUBE_config.mqh"
#         self._lookback_min = read_lookback_from_mqh(self._config_path)

#         self._max_mid = 5000
#         self._max_min = self._lookback_min

#         self._con: Optional[sqlite3.Connection] = None
#         self._cur: Optional[sqlite3.Cursor]     = None

#         self._last_bal_ts: Optional[str]  = None
#         self._last_risk_ts: Optional[str] = None

#         self._balance_error_logged = False
#         self._risk_error_logged    = False

#         # vom GUI gesteuerte Range
#         self._range_balance = "max"
#         self._range_stat    = "max"

#         # Flags: beim nächsten Nicht-Leer-Delta soll reset=True mitgeschickt werden
#         self._need_balance_reset = True
#         self._need_risk_reset    = True

#         self._last_active_frac: float = 0.0

        

#         print(f"[MasterLoop] init: range_balance={self._range_balance}, "
#               f"range_stat={self._range_stat}, lookback_min={self._lookback_min}")

#     # ------------------------------------------------------------------ DB

#     def _open(self) -> None:
#         self._con = sqlite3.connect(self._db_swarm_path, timeout=1.0)
#         self._con.execute("PRAGMA journal_mode=WAL;")
#         self._con.execute("PRAGMA busy_timeout=500;")
#         self._cur = self._con.cursor()
#         print("[MasterLoop] DB geöffnet:", self._db_swarm_path)

#     def _close(self) -> None:
#         try:
#             if self._cur:
#                 self._cur.close()
#         finally:
#             self._cur = None
#         try:
#             if self._con:
#                 self._con.close()
#         finally:
#             self._con = None
#         print("[MasterLoop] DB geschlossen")

#     # ------------------------------------------------------------------ Queue-Helfer

#     def _send_payload(self, payload: Dict[str, Any]) -> None:
#         try:
#             self.data_q.put_nowait(payload)
#         except queue.Full:
#             try:
#                 _ = self.data_q.get_nowait()
#             except queue.Empty:
#                 pass
#             self.data_q.put_nowait(payload)

#     # ------------------------------------------------------------------ Balance

#     def _fetch_new_balance_rows(self) -> List[BalanceRow]:
#         tbl = self._table_balance
#         rows: List[BalanceRow] = []

#         try:
#             if self._last_bal_ts is None:
#                 # Initial-Load (auch nach Range-Wechsel)
#                 if self._range_balance == "max":
#                     t0 = time.time()
#                     self._cur.execute(f"""
#                         SELECT schwarm, timestamp, balance_cents
#                         FROM {tbl}
#                         ORDER BY timestamp, schwarm
#                     """)
#                     raw = self._cur.fetchall()
#                     dt = (time.time() - t0) * 1000.0
#                     print(f"[MasterLoop] balance initial (max): {len(raw)} Zeilen in {dt:.1f} ms")
#                 else:
#                     if self._range_balance == "mid":
#                         limit_ts = self._max_mid
#                     else:
#                         limit_ts = self._max_min

#                     t0 = time.time()
#                     self._cur.execute(f"""
#                         SELECT schwarm, timestamp, balance_cents
#                         FROM {tbl}
#                         WHERE timestamp IN (
#                             SELECT DISTINCT timestamp
#                             FROM {tbl}
#                             ORDER BY timestamp DESC
#                             LIMIT ?
#                         )
#                         ORDER BY timestamp, schwarm
#                     """, (limit_ts,))
#                     raw = self._cur.fetchall()
#                     dt = (time.time() - t0) * 1000.0
#                     print(f"[MasterLoop] balance initial ({self._range_balance}={limit_ts}): "
#                           f"{len(raw)} Zeilen in {dt:.1f} ms")
#             else:
#                 # nur Deltas
#                 t0 = time.time()
#                 self._cur.execute(f"""
#                     SELECT schwarm, timestamp, balance_cents
#                     FROM {tbl}
#                     WHERE timestamp > ?
#                     ORDER BY timestamp, schwarm
#                 """, (self._last_bal_ts,))
#                 raw = self._cur.fetchall()
#                 dt = (time.time() - t0) * 1000.0
#                 if raw:
#                     print(f"[MasterLoop] balance poll: +{len(raw)} Zeilen in {dt:.1f} ms")

#             rows = [(int(s), str(ts), int(c)) for (s, ts, c) in raw]

#             if rows:
#                 self._last_bal_ts = rows[-1][1]

#             self._balance_error_logged = False
#         except sqlite3.OperationalError as e:
#             if not self._balance_error_logged:
#                 print("*" * 60)
#                 print(f"[MasterLoop] balance Fehler: {e}")
#                 print("*" * 60)
#                 self._balance_error_logged = True

#         return rows

#     def _send_balance_delta(self, rows: List[BalanceRow]) -> None:
#         if not rows:
#             return
#         reset_flag = self._need_balance_reset
#         payload: Dict[str, Any] = {
#             "topic": "balance_delta",
#             "rows": rows,
#             "reset": reset_flag,
#             "ts": time.time(),
#         }
#         self._send_payload(payload)
#         # Reset-Flag nur beim ersten Send nach Range-Wechsel / Start verwenden
#         if reset_flag:
#             self._need_balance_reset = False

#     # ------------------------------------------------------------------ Risk / Statistik

#     def _fetch_new_risk_rows(self, swarm_id: int = 10) -> List[RiskRow]:
#         tbl = self._risk_table
#         rows: List[RiskRow] = []

#         try:
#             if self._last_risk_ts is None:
#                 # Initial-Load (auch nach Range-Wechsel)
#                 if self._range_stat == "max":
#                     t0 = time.time()
#                     self._cur.execute(f"""
#                         SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac
#                         FROM {tbl}
#                         WHERE schwarm=?
#                         ORDER BY timestamp
#                     """, (swarm_id,))
#                     raw = self._cur.fetchall()
#                     dt = (time.time() - t0) * 1000.0
#                     print(f"[MasterLoop] risk_state initial (max): {len(raw)} Zeilen in {dt:.1f} ms")
#                 else:
#                     if self._range_stat == "mid":
#                         limit_ts = self._max_mid
#                     else:
#                         limit_ts = self._max_min

#                     t0 = time.time()
#                     self._cur.execute(f"""
#                         SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac
#                         FROM {tbl}
#                         WHERE schwarm=? AND timestamp IN (
#                             SELECT DISTINCT timestamp
#                             FROM {tbl}
#                             WHERE schwarm=?
#                             ORDER BY timestamp DESC
#                             LIMIT ?
#                         )
#                         ORDER BY timestamp
#                     """, (swarm_id, swarm_id, limit_ts))
#                     raw = self._cur.fetchall()
#                     dt = (time.time() - t0) * 1000.0
#                     print(f"[MasterLoop] risk_state initial ({self._range_stat}={limit_ts}): "
#                           f"{len(raw)} Zeilen in {dt:.1f} ms")
#             else:
#                 t0 = time.time()
#                 self._cur.execute(f"""
#                     SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac
#                     FROM {tbl}
#                     WHERE schwarm=? AND timestamp > ?
#                     ORDER BY timestamp
#                 """, (swarm_id, self._last_risk_ts))
#                 raw = self._cur.fetchall()
#                 dt = (time.time() - t0) * 1000.0
#                 if raw:
#                     print(f"[MasterLoop] risk_state poll: +{len(raw)} Zeilen in {dt:.1f} ms")

#             rows = [(str(ts), int(eq), int(pk), int(fl)) for (ts, eq, pk, fl, af) in raw]

#             if raw:
#                 self._last_risk_ts     = str(raw[-1][0])
#                 self._last_active_frac = float(raw[-1][4])

#             self._risk_error_logged = False
#         except sqlite3.OperationalError as e:
#             if not self._risk_error_logged:
#                 print("*" * 60)
#                 print(f"[MasterLoop] risk_state Fehler: {e}")
#                 print("*" * 60)
#                 self._risk_error_logged = True

#         return rows

#     def _send_statistik_delta(self, rows: List[RiskRow], swarm_id: int) -> None:
#         if not rows:
#             return
#         active_count = self._fetch_active_count(swarm_id=0)
#         reset_flag   = self._need_risk_reset
#         payload: Dict[str, Any] = {
#             "topic": "statistik_delta",
#             "swarm": swarm_id,
#             "rows": rows,
#             "active_count": active_count,
#             "active_frac": self._last_active_frac,
#             "reset": reset_flag,
#             "ts": time.time(),
#         }
#         self._send_payload(payload)
#         if reset_flag:
#             self._need_risk_reset = False

#     # ------------------------------------------------------------------ Kommandos aus GUI

#     def _poll_cmd_q(self) -> None:
#         try:
#             while True:
#                 msg = self.cmd_q.get_nowait()
#                 topic = msg.get("topic")

#                 if topic == "range_update":
#                     bal = msg.get("balance", self._range_balance)
#                     sta = msg.get("statistik", self._range_stat)

#                     if bal in ("max", "mid", "min"):
#                         if bal != self._range_balance:
#                             self._range_balance     = bal
#                             self._last_bal_ts       = None
#                             self._need_balance_reset = True
#                     if sta in ("max", "mid", "min"):
#                         if sta != self._range_stat:
#                             self._range_stat      = sta
#                             self._last_risk_ts    = None
#                             self._need_risk_reset = True

#                     print(f"[MasterLoop] Range Update: balance={self._range_balance}, "
#                           f"stat={self._range_stat} (min-lookback={self._max_min})")
#                 else:
#                     print(f"[MasterLoop] Unbekanntes Kommando: {msg}")
#         except queue.Empty:
#             pass

#     # ------------------------------------------------------------------ aktive EAs (swarms.aktiv)

#     def _fetch_active_count(self, swarm_id: int = 0) -> int:
#         try:
#             self._cur.execute(
#                 "SELECT COUNT(*) FROM swarms WHERE swarm=? AND aktiv=1;",
#                 (swarm_id,),
#             )
#             row = self._cur.fetchone()
#             if row is None:
#                 return 0
#             return int(row[0])
#         except sqlite3.OperationalError as e:
#             print(f"[MasterLoop] Fehler beim Lesen active_count: {e}")
#             return 0

#     # ------------------------------------------------------------------ Hauptloop

#     def run(self) -> None:
#         swarm_id = 10
#         print("[MasterLoop] gestartet")

#         try:
#             self._open()

#             # Initial-Deltas (mit reset=True)
#             init_bal = self._fetch_new_balance_rows()
#             if init_bal:
#                 self._send_balance_delta(init_bal)

#             init_risk = self._fetch_new_risk_rows(swarm_id=swarm_id)
#             if init_risk:
#                 self._send_statistik_delta(init_risk, swarm_id=swarm_id)

#             while not self.stop.is_set():
#                 self._poll_cmd_q()

#                 new_bal = self._fetch_new_balance_rows()
#                 if new_bal:
#                     self._send_balance_delta(new_bal)

#                 new_risk = self._fetch_new_risk_rows(swarm_id=swarm_id)
#                 if new_risk:
#                     self._send_statistik_delta(new_risk, swarm_id=swarm_id)

#                 self.stop.wait(self._poll_interval_s)

#         finally:
#             self._close()
#             print("[MasterLoop] beendet")

# MasterLoop.py
import sqlite3
import time
import queue
import threading
import re
from typing import List, Tuple, Optional, Dict, Any

BalanceRow = Tuple[int, str, int]
# RiskRow    = Tuple[str, int, int, int]  # ts, equity, peak, floor
# vorher:
# RiskRow    = Tuple[str, int, int, int]
RiskRow    = Tuple[str, int, int, int, float, float]  # ts, eq, peak, floor, active_frac, lambda_dyn



def read_lookback_from_mqh(path: str) -> int:
    try:
        with open(path, "r", encoding="utf-8") as f:
            txt = f.read()
    except OSError:
        print(f"[MasterLoop] WARNUNG: config.mqh '{path}' nicht gefunden. Nutze Default 500.")
        return 500

    m = re.search(r"c_fitness_lookback_EAs\s*=\s*([0-9]+)", txt)
    if m:
        val = int(m.group(1))
        print(f"[MasterLoop] c_fitness_lookback_EAs aus config.mqh: {val}")
        return val

    print(f"[MasterLoop] WARNUNG: c_fitness_lookback_EAs nicht in '{path}' gefunden. Nutze Default 500.")
    return 500


class MasterLoop:
    def __init__(self, data_q: queue.Queue, cmd_q: queue.Queue, stop_evt: threading.Event):
        self.data_q = data_q
        self.cmd_q  = cmd_q
        self.stop   = stop_evt

        self._db_swarm_path   = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Schwarm.db"
        self._db_experts_path = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"
        self._table_balance   = "balance"
        self._risk_table      = "risk_state"
        self._poll_interval_s = 1.0

        self._config_path  = r"C:\MQL_Shared\Includes\Config1\KUBE_config.mqh"
        self._lookback_min = read_lookback_from_mqh(self._config_path)

        self._max_mid = 5000
        self._max_min = self._lookback_min

        self._con: Optional[sqlite3.Connection] = None
        self._cur: Optional[sqlite3.Cursor]     = None
        self._con_experts: Optional[sqlite3.Connection] = None
        self._cur_experts: Optional[sqlite3.Cursor]     = None

        self._last_bal_ts: Optional[str]  = None
        self._last_risk_ts: Optional[str] = None

        self._balance_error_logged = False
        self._risk_error_logged    = False

        # vom GUI gesteuerte Range
        self._range_balance = "max"
        self._range_stat    = "max"

        # Flags: beim nächsten Nicht-Leer-Delta soll reset=True mitgeschickt werden
        self._need_balance_reset = True
        self._need_risk_reset    = True

        self._last_active_frac: float = 0.0
        self._last_lambda: float      = 0.0   # ★ neu: letzter lambda_dyn-Wert


        print(f"[MasterLoop] init: range_balance={self._range_balance}, "
              f"range_stat={self._range_stat}, lookback_min={self._lookback_min}")

    # ------------------------------------------------------------------ DB

    def _open(self) -> None:
        self._con = sqlite3.connect(self._db_swarm_path, timeout=1.0)
        self._con.execute("PRAGMA journal_mode=WAL;")
        self._con.execute("PRAGMA busy_timeout=500;")
        self._cur = self._con.cursor()
        print("[MasterLoop] DB geöffnet:", self._db_swarm_path)

    def _close(self) -> None:
        try:
            if self._cur:
                self._cur.close()
        finally:
            self._cur = None
        try:
            if self._con:
                self._con.close()
        finally:
            self._con = None
        print("[MasterLoop] DB geschlossen")

    def _open_experts(self) -> None:
        self._con_experts = sqlite3.connect(self._db_experts_path, timeout=1.0)
        self._con_experts.execute("PRAGMA journal_mode=WAL;")
        self._con_experts.execute("PRAGMA busy_timeout=500;")
        self._cur_experts = self._con_experts.cursor()
        print("[MasterLoop] Experts DB geöffnet:", self._db_experts_path)

    def _close_experts(self) -> None:
        try:
            if self._cur_experts:
                self._cur_experts.close()
        finally:
            self._cur_experts = None
        try:
            if self._con_experts:
                self._con_experts.close()
        finally:
            self._con_experts = None
        print("[MasterLoop] Experts DB geschlossen")

    # ------------------------------------------------------------------ Queue-Helfer

    def _send_payload(self, payload: Dict[str, Any]) -> None:
        try:
            self.data_q.put_nowait(payload)
        except queue.Full:
            try:
                _ = self.data_q.get_nowait()
            except queue.Empty:
                pass
            self.data_q.put_nowait(payload)

    # ------------------------------------------------------------------ Balance

    def _fetch_new_balance_rows(self) -> List[BalanceRow]:
        tbl = self._table_balance
        rows: List[BalanceRow] = []

        try:
            if self._last_bal_ts is None:
                # Initial-Load (auch nach Range-Wechsel)
                if self._range_balance == "max":
                    t0 = time.time()
                    self._cur.execute(f"""
                        SELECT schwarm, timestamp, balance_cents
                        FROM {tbl}
                        ORDER BY timestamp, schwarm
                    """)
                    raw = self._cur.fetchall()
                    dt = (time.time() - t0) * 1000.0
                    print(f"[MasterLoop] balance initial (max): {len(raw)} Zeilen in {dt:.1f} ms")
                else:
                    if self._range_balance == "mid":
                        limit_ts = self._max_mid
                    else:
                        limit_ts = self._max_min

                    t0 = time.time()
                    self._cur.execute(f"""
                        SELECT schwarm, timestamp, balance_cents
                        FROM {tbl}
                        WHERE timestamp IN (
                            SELECT DISTINCT timestamp
                            FROM {tbl}
                            ORDER BY timestamp DESC
                            LIMIT ?
                        )
                        ORDER BY timestamp, schwarm
                    """, (limit_ts,))
                    raw = self._cur.fetchall()
                    dt = (time.time() - t0) * 1000.0
                    print(f"[MasterLoop] balance initial ({self._range_balance}={limit_ts}): "
                          f"{len(raw)} Zeilen in {dt:.1f} ms")
            else:
                # nur Deltas
                t0 = time.time()
                self._cur.execute(f"""
                    SELECT schwarm, timestamp, balance_cents
                    FROM {tbl}
                    WHERE timestamp > ?
                    ORDER BY timestamp, schwarm
                """, (self._last_bal_ts,))
                raw = self._cur.fetchall()
                dt = (time.time() - t0) * 1000.0
                if raw:
                    print(f"[MasterLoop] balance poll: +{len(raw)} Zeilen in {dt:.1f} ms")

            rows = [(int(s), str(ts), int(c)) for (s, ts, c) in raw]

            if rows:
                self._last_bal_ts = rows[-1][1]

            self._balance_error_logged = False
        except sqlite3.OperationalError as e:
            if not self._balance_error_logged:
                print("*" * 60)
                print(f"[MasterLoop] balance Fehler: {e}")
                print("*" * 60)
                self._balance_error_logged = True

        return rows

    def _send_balance_delta(self, rows: List[BalanceRow]) -> None:
        if not rows:
            return
        reset_flag = self._need_balance_reset
        payload: Dict[str, Any] = {
            "topic": "balance_delta",
            "rows": rows,
            "reset": reset_flag,
            "ts": time.time(),
        }
        self._send_payload(payload)
        # Reset-Flag nur beim ersten Send nach Range-Wechsel / Start verwenden
        if reset_flag:
            self._need_balance_reset = False

    # ------------------------------------------------------------------ Risk / Statistik

    # def _fetch_new_risk_rows(self, swarm_id: int = 10) -> List[RiskRow]:
    #     tbl = self._risk_table
    #     rows: List[RiskRow] = []

    #     try:
    #         if self._last_risk_ts is None:
    #             # Initial-Load (auch nach Range-Wechsel)
    #             if self._range_stat == "max":
    #                 t0 = time.time()
    #                 self._cur.execute(f"""
    #                     SELECT timestamp, equity_cents, peak_cents, floor_cents,
    #                            active_frac, lambda_dyn
    #                     FROM {tbl}
    #                     WHERE schwarm=?
    #                     ORDER BY timestamp
    #                 """, (swarm_id,))
    #                 raw = self._cur.fetchall()
    #                 dt = (time.time() - t0) * 1000.0
    #                 print(f"[MasterLoop] risk_state initial (max): {len(raw)} Zeilen in {dt:.1f} ms")
    #             else:
    #                 if self._range_stat == "mid":
    #                     limit_ts = self._max_mid
    #                 else:
    #                     limit_ts = self._max_min

    #                 t0 = time.time()
    #                 self._cur.execute(f"""
    #                     SELECT timestamp, equity_cents, peak_cents, floor_cents,
    #                            active_frac, lambda_dyn
    #                     FROM {tbl}
    #                     WHERE schwarm=? AND timestamp IN (
    #                         SELECT DISTINCT timestamp
    #                         FROM {tbl}
    #                         WHERE schwarm=?
    #                         ORDER BY timestamp DESC
    #                         LIMIT ?
    #                     )
    #                     ORDER BY timestamp
    #                 """, (swarm_id, swarm_id, limit_ts))
    #                 raw = self._cur.fetchall()
    #                 dt = (time.time() - t0) * 1000.0
    #                 print(f"[MasterLoop] risk_state initial ({self._range_stat}={limit_ts}): "
    #                       f"{len(raw)} Zeilen in {dt:.1f} ms")
    #         else:
    #             t0 = time.time()
    #             self._cur.execute(f"""
    #                 SELECT timestamp, equity_cents, peak_cents, floor_cents,
    #                        active_frac, lambda_dyn
    #                 FROM {tbl}
    #                 WHERE schwarm=? AND timestamp > ?
    #                 ORDER BY timestamp
    #             """, (swarm_id, self._last_risk_ts))
    #             raw = self._cur.fetchall()
    #             dt = (time.time() - t0) * 1000.0
    #             if raw:
    #                 print(f"[MasterLoop] risk_state poll: +{len(raw)} Zeilen in {dt:.1f} ms")

    #         # rows für Plot (ts, equity, peak, floor)
    #         rows = [(str(ts), int(eq), int(pk), int(fl))
    #                 for (ts, eq, pk, fl, af, lam) in raw]

    #         if raw:
    #             self._last_risk_ts     = str(raw[-1][0])
    #             self._last_active_frac = float(raw[-1][4])
    #             self._last_lambda      = float(raw[-1][5])  # ★ letzte lambda_dyn

    #         self._risk_error_logged = False
    #     except sqlite3.OperationalError as e:
    #         if not self._risk_error_logged:
    #             print("*" * 60)
    #             print(f"[MasterLoop] risk_state Fehler: {e}")
    #             print("*" * 60)
    #             self._risk_error_logged = True

    #     return rows
    def _fetch_new_risk_rows(self, swarm_id: int = 10) -> List[RiskRow]:
        tbl = self._risk_table
        rows: List[RiskRow] = []

        try:
            if self._last_risk_ts is None:
                if self._range_stat == "max":
                    t0 = time.time()
                    self._cur.execute(f"""
                        SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac, lambda_dyn
                        FROM {tbl}
                        WHERE schwarm=?
                        ORDER BY timestamp
                    """, (swarm_id,))
                    raw = self._cur.fetchall()
                    dt = (time.time() - t0) * 1000.0
                    print(f"[MasterLoop] risk_state initial (max): {len(raw)} Zeilen in {dt:.1f} ms")
                else:
                    if self._range_stat == "mid":
                        limit_ts = self._max_mid
                    else:
                        limit_ts = self._lookback_min

                    t0 = time.time()
                    self._cur.execute(f"""
                        SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac, lambda_dyn
                        FROM {tbl}
                        WHERE schwarm=? AND timestamp IN (
                            SELECT DISTINCT timestamp
                            FROM {tbl}
                            WHERE schwarm=?
                            ORDER BY timestamp DESC
                            LIMIT ?
                        )
                        ORDER BY timestamp
                    """, (swarm_id, swarm_id, limit_ts))
                    raw = self._cur.fetchall()
                    dt = (time.time() - t0) * 1000.0
                    print(f"[MasterLoop] risk_state initial ({self._range_stat}={limit_ts}): "
                          f"{len(raw)} Zeilen in {dt:.1f} ms")
            else:
                t0 = time.time()
                self._cur.execute(f"""
                    SELECT timestamp, equity_cents, peak_cents, floor_cents, active_frac, lambda_dyn
                    FROM {tbl}
                    WHERE schwarm=? AND timestamp > ?
                    ORDER BY timestamp
                """, (swarm_id, self._last_risk_ts))
                raw = self._cur.fetchall()
                dt = (time.time() - t0) * 1000.0
                if raw:
                    print(f"[MasterLoop] risk_state poll: +{len(raw)} Zeilen in {dt:.1f} ms")

            # ts, eq, pk, fl, active_frac, lambda_dyn
            rows = [
                (str(ts), int(eq), int(pk), int(fl), float(af), float(lam))
                for (ts, eq, pk, fl, af, lam) in raw
            ]

            if raw:
                self._last_risk_ts     = str(raw[-1][0])
                self._last_active_frac = float(raw[-1][4])
                self._last_lambda      = float(raw[-1][5])

            self._risk_error_logged = False
        except sqlite3.OperationalError as e:
            if not self._risk_error_logged:
                print("*" * 60)
                print(f"[MasterLoop] risk_state Fehler: {e}")
                print("*" * 60)
                self._risk_error_logged = True

        return rows

    # def _send_statistik_delta(self, rows: List[RiskRow], swarm_id: int) -> None:
    #     if not rows:
    #         return
    #     active_count = self._fetch_active_count(swarm_id=0)
    #     reset_flag   = self._need_risk_reset
    #     payload: Dict[str, Any] = {
    #         "topic": "statistik_delta",
    #         "swarm": swarm_id,
    #         "rows": rows,
    #         "active_count": active_count,
    #         "active_frac": self._last_active_frac,
    #         "lambda": self._last_lambda,   # ★ neu: aktuelles lambda_dyn
    #         "reset": reset_flag,
    #         "ts": time.time(),
    #     }
    #     self._send_payload(payload)
    #     if reset_flag:
    #         self._need_risk_reset = False
    def _send_statistik_delta(self, rows: List[RiskRow], swarm_id: int) -> None:
        if not rows:
            return
        active_count = self._fetch_active_count(swarm_id=0)
        reset_flag   = self._need_risk_reset
        payload: Dict[str, Any] = {
            "topic": "statistik_delta",
            "swarm": swarm_id,
            "rows": rows,
            "active_count": active_count,
            "active_frac": self._last_active_frac,
            "lambda": self._last_lambda,   # ★ neu
            "reset": reset_flag,
            "ts": time.time(),
        }
        self._send_payload(payload)
        if reset_flag:
            self._need_risk_reset = False

    # ------------------------------------------------------------------ Kommandos aus GUI

    def _poll_cmd_q(self) -> None:
        try:
            while True:
                msg = self.cmd_q.get_nowait()
                topic = msg.get("topic")

                if topic == "range_update":
                    bal = msg.get("balance", self._range_balance)
                    sta = msg.get("statistik", self._range_stat)

                    if bal in ("max", "mid", "min"):
                        if bal != self._range_balance:
                            self._range_balance      = bal
                            self._last_bal_ts        = None
                            self._need_balance_reset = True
                    if sta in ("max", "mid", "min"):
                        if sta != self._range_stat:
                            self._range_stat      = sta
                            self._last_risk_ts    = None
                            self._need_risk_reset = True

                    print(f"[MasterLoop] Range Update: balance={self._range_balance}, "
                          f"stat={self._range_stat} (min-lookback={self._max_min})")
                else:
                    print(f"[MasterLoop] Unbekanntes Kommando: {msg}")
        except queue.Empty:
            pass

    # ------------------------------------------------------------------ aktive EAs (swarms.aktiv)

    def _fetch_active_count(self, swarm_id: int = 0) -> int:
        try:
            self._cur.execute(
                "SELECT COUNT(*) FROM swarms WHERE swarm=? AND aktiv=1;",
                (swarm_id,),
            )
            row = self._cur.fetchone()
            if row is None:
                return 0
            return int(row[0])
        except sqlite3.OperationalError as e:
            print(f"[MasterLoop] Fehler beim Lesen active_count: {e}")
            return 0

    # ------------------------------------------------------------------ Fitness (KUBE_Experts)

    def _fetch_fitness_data(self) -> List[Tuple[float, float, float]]:
        # Liest NetProfitNorm, ActivityNorm, R aus Fitness_proBar
        rows = []
        try:
            self._cur_experts.execute("""
                SELECT NetProfitNorm, ActivityNorm, R
                FROM Fitness_proBar
                ORDER BY rowid DESC
                LIMIT 50
            """)
            # Reverse to have oldest first
            raw = self._cur_experts.fetchall()[::-1]
            rows = [(float(r[0]), float(r[1]), float(r[2])) for r in raw]
        except sqlite3.OperationalError as e:
            print(f"[MasterLoop] Fehler beim Lesen Fitness_proBar: {e}")
        return rows

    def _send_fitness_delta(self, rows: List[Tuple[float, float, float]]) -> None:
        if not rows:
            return
        payload: Dict[str, Any] = {
            "topic": "fitness_update",
            "rows": rows,
            "ts": time.time(),
        }
        self._send_payload(payload)

    # ------------------------------------------------------------------ Hauptloop

    def run(self) -> None:
        swarm_id = 10
        print("[MasterLoop] gestartet")

        try:
            self._open()
            self._open_experts()

            # Initial-Deltas (mit reset=True)
            init_bal = self._fetch_new_balance_rows()
            if init_bal:
                self._send_balance_delta(init_bal)

            init_risk = self._fetch_new_risk_rows(swarm_id=swarm_id)
            if init_risk:
                self._send_statistik_delta(init_risk, swarm_id=swarm_id)

            while not self.stop.is_set():
                self._poll_cmd_q()

                new_bal = self._fetch_new_balance_rows()
                if new_bal:
                    self._send_balance_delta(new_bal)

                new_risk = self._fetch_new_risk_rows(swarm_id=swarm_id)
                if new_risk:
                    self._send_statistik_delta(new_risk, swarm_id=swarm_id)

                fitness_data = self._fetch_fitness_data()
                if fitness_data:
                    self._send_fitness_delta(fitness_data)

                self.stop.wait(self._poll_interval_s)

        finally:
            self._close()
            self._close_experts()
            print("[MasterLoop] beendet")
