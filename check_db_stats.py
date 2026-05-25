import sqlite3
import os

db_schwarm = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Schwarm.db"
db_experts = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"

def check_db(path, tables):
    print(f"--- Checking {path} ---")
    if not os.path.exists(path):
        print("File not found.")
        return
    
    print(f"Size: {os.path.getsize(path) / 1024 / 1024:.2f} MB")
    
    try:
        conn = sqlite3.connect(path)
        cur = conn.cursor()
        for tbl in tables:
            try:
                cur.execute(f"SELECT COUNT(*) FROM {tbl}")
                count = cur.fetchone()[0]
                print(f"Table '{tbl}': {count} rows")
            except sqlite3.OperationalError as e:
                print(f"Table '{tbl}': Error - {e}")
        conn.close()
    except Exception as e:
        print(f"Error opening DB: {e}")

check_db(db_schwarm, ["balance", "risk_state", "swarms", "mutation_tracking"])
check_db(db_experts, ["timebeat_paper10", "Fitness_proBar"])
