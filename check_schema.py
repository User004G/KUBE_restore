import sqlite3

db_path = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Schwarm.db"

try:
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.execute("PRAGMA table_info(swarms)")
    columns = cur.fetchall()
    print("Columns in 'swarms' table:")
    for col in columns:
        print(col)
    con.close()
except Exception as e:
    print(f"Error: {e}")
