
from multiprocessing import Queue
from multiprocessing.queues import Queue as MPQueue  # sichergehen, dass der richtige Typ importiert ist
from threading import Event



class Controller:
    _instance = None

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super(Controller, cls).__new__(cls)
        return cls._instance

    def __init__(self):
        if not hasattr(self, "initialized"):
            self.statistik_data_queue = Queue()
            self.command_queue = Queue()
            self.result_queue = Queue()
            self.grid_data_queue = Queue()

            
            self.last_sent_result = None
            self.grid_callback = None

            self.stop_event = Event()
            self.start_event = Event()

            self.initialized = True
            print("✅ Controller initialisiert mit multiprocessing.Queue")

    


    # def send_statistik_data(self, data):
    #     """
    #     Sendet die Balance-Score-daten in die statistik_data_queue.
    #     Begrenzt auch die Länge der Score-Historie im Tupel.
    #     """
    #     if not isinstance(self.statistik_data_queue, MPQueue):
    #         print("❌ Fehler: statistik_data_queue ist keine gültige multiprocessing.Queue!")
    #         return

    #     if not isinstance(data, tuple) or len(data) != 2:
    #         print("❌ Ungültiges Statistik-Datenformat!")
    #         return

    #     paper_nr, score_history = data

    #     # 🧱 Begrenzung der Score-Liste
    #     max_len = config.mutations_lookback
    #     if isinstance(score_history, list):
    #         score_history = score_history[-max_len:]
    #     data = (paper_nr, score_history)

    #     if self.statistik_data_queue.qsize() >= self.max_queue_size:
    #         try:
    #             _ = self.statistik_data_queue.get_nowait()
    #         except Exception:
    #             pass

    #     try:
    #         self.statistik_data_queue.put_nowait(data)
    #     except Exception as e:
    #         print(f"❌ Fehler beim Hinzufügen zur Queue: {e}")

    # def send_command(self, command):
    #     self.command_queue.put(command)
    #     # print(f"📤 Befehl gesendet: {command}")

    # def send_grid_data(self, grid_data):
    #     if not self.grid_callback:
    #         print("⚠️ Kein grid_callback registriert.")
    #         return

    #     if "grid_data" in grid_data:
    #         first_entry = next(iter(grid_data["grid_data"].values()), None)
    #         if first_entry:
    #             paper_nr = first_entry.get("paper_nr")
    #             if paper_nr != config.show_swarm_details:
    #                 print(f"🚫 Grid-Daten ignoriert (Schwarm {paper_nr} != erwartet {config.show_swarm_details})")
    #                 return

    #     self.grid_callback(grid_data)
    #     # print("📨 Grid-Daten an Callback übergeben")

    # def get_grid_data(self):
    #     if not self.grid_data_queue.empty():
    #         data = self.grid_data_queue.get()
    #         # print(f"📥 Grid-Daten abgerufen: {data}")
    #         return data
    #     return None

    # def set_grid_callback(self, callback):
    #     self.grid_callback = callback
    #     print("🔁 Grid-Callback registriert")

    # def get_balance_data(self):
    #     try:
    #         if not self.result_queue.empty():
    #             result = self.result_queue.get_nowait()
    #             # print(f"📥 Ergebnis abgerufen: {result}")
    #             return result
    #     except Exception as e:
    #         print(f"❌ Fehler beim Abrufen der result_queue: {e}")
    #     return None

    # def send_balance_data(self, result):
    #     if self.last_sent_result == result:
    #         # print("⏭️ Ergebnis identisch, nicht gesendet")
    #         return

    #     if self.result_queue.qsize() > 50:
    #         _ = self.result_queue.get()

    #     self.result_queue.put(result)
    #     self.last_sent_result = result
    #     # print(f"📤 Ergebnis gesendet: {result}")

    # def get_statistik_data(self):
    #     if not self.statistik_data_queue.empty():
    #         try:
    #             data = self.statistik_data_queue.get_nowait()
    #             # print(f"📥 Statistikdaten abgerufen: {data}")
    #             return data
    #         except Exception as e:
    #             print(f"❌ Fehler beim Lesen der Statistikdaten: {e}")
    #     return None

    # def stop(self):
    #     self.stop_event.set()
    #     print("⛔ Stop-Event gesetzt")

    # def start(self):
    #     self.start_event.set()
    #     print("▶️ Start-Event gesetzt")
