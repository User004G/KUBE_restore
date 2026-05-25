
# start.py
import threading, queue
from Gui import Gui
from Masterloop import MasterLoop

def Start():
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

if __name__ == "__main__":
    Start()
