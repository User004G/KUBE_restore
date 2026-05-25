import sqlite3

db_path = r"c:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"

print(f"Checking schema for: {db_path}")
try:
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # List all tables
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
    tables = cursor.fetchall()
    print("Tables found:", tables)
    
    # Check ParamsIN schema if it exists
    if any('ParamsIN' in t for t in tables):
        cursor.execute("PRAGMA table_info(ParamsIN);")
        columns = cursor.fetchall()
        print("\nSchema for ParamsIN:")
        for col in columns:
            print(col)
    else:
        print("\nTable 'ParamsIN' not found.")
        
    conn.close()
except sqlite3.Error as e:
    print(f"SQLite error: {e}")
