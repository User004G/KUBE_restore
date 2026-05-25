
# start_test_env.py
import threading, queue
import sys
import os

# Ensure we can import modules from the current directory
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from Gui import Gui
from Masterloop import MasterLoop

def Start():
    print("[TestEnv] Starting KUBE Test Environment...")
    data_q = queue.Queue(maxsize=50)   # Loop -> GUI
    cmd_q  = queue.Queue(maxsize=200)   # GUI  -> Loop
    stop_evt = threading.Event()

    loop = MasterLoop(data_q=data_q, cmd_q=cmd_q, stop_evt=stop_evt)
                      
    t = threading.Thread(target=loop.run, daemon=True)
    t.start()

    app = Gui(data_q=data_q, cmd_q=cmd_q, stop_evt=stop_evt)  
    app.mainloop()

    stop_evt.set()
    t.join()
    print("[TestEnv] Terminated.")

if __name__ == "__main__":
    Start()
