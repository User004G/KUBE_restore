import sqlite3
import os

db_experts = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"

def check_indexes(path, table):
    print(f"--- Checking indexes for {table} in {path} ---")
    if not os.path.exists(path):
        print("File not found.")
        return
    
    try:
        conn = sqlite3.connect(path)
        cur = conn.cursor()
        
        cur.execute(f"PRAGMA index_list({table})")
        indexes = cur.fetchall()
        
        if not indexes:
            print(f"No indexes found for table '{table}'")
        else:
            for idx in indexes:
                # idx: (seq, name, unique, origin, partial)
                name = idx[1]
                unique = "UNIQUE" if idx[2] else ""
                print(f"Index: {name} {unique}")
                
                cur.execute(f"PRAGMA index_info({name})")
                cols = cur.fetchall()
                col_names = [c[2] for c in cols]
                print(f"  Columns: {', '.join(col_names)}")
                
        conn.close()
    except Exception as e:
        print(f"Error: {e}")

check_indexes(db_experts, "Fitness_proBar")
