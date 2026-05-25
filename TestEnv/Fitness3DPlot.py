import sqlite3
import time
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from matplotlib.animation import FuncAnimation

# Configuration
DB_PATH = r"C:\Users\kbeie\AppData\Roaming\MetaQuotes\Terminal\Common\Files\KUBE_Experts.db"
POLL_INTERVAL_MS = 1000

def fetch_data():
    """Fetches the latest 50 rows from Fitness_proBar."""
    try:
        conn = sqlite3.connect(DB_PATH, timeout=1.0)
        cursor = conn.cursor()
        cursor.execute("""
            SELECT NetProfitNorm, ActivityNorm, R
            FROM Fitness_proBar
            ORDER BY rowid DESC
            LIMIT 50
        """)
        rows = cursor.fetchall()
        conn.close()
        # Reverse to have oldest first (though for scatter it matters less, consistent with time)
        return rows[::-1]
    except sqlite3.Error as e:
        print(f"Database error: {e}")
        return []

def update_plot(frame, sc, ax):
    """Update function for animation."""
    data = fetch_data()
    if not data:
        return

    xs = [r[0] for r in data]
    ys = [r[1] for r in data]
    zs = [r[2] for r in data]

    # Update scatter plot data
    # Scatter plots in 3D don't have a simple set_data method for 3 coordinates + color
    # It's often easier to clear and redraw or use private attributes, but clear/redraw is safer for dynamic data size
    ax.clear()
    
    # Re-apply styling
    ax.set_facecolor("black")
    ax.set_title("Fitness 3D (Experts)", color="white")
    ax.set_xlabel("NetProfitNorm", color="white")
    ax.set_ylabel("ActivityNorm", color="white")
    ax.set_zlabel("R", color="white")
    
    ax.tick_params(axis='x', colors='white')
    ax.tick_params(axis='y', colors='white')
    ax.tick_params(axis='z', colors='white')
    
    ax.xaxis.pane.fill = False
    ax.yaxis.pane.fill = False
    ax.zaxis.pane.fill = False
    
    for axis in [ax.xaxis, ax.yaxis, ax.zaxis]:
        axis._axinfo["grid"]['color'] = (1, 1, 1, 0.2)

    # Set fixed limits to keep the view stable and meaningful for normalized data
    ax.set_xlim(0, 1.1)
    ax.set_ylim(0, 1.1)
    ax.set_zlim(0, 1.1)

    # Set view angle so high values (1,1,1) are closer to the viewer
    # elev=30, azim=45 puts the viewer in the first octant (x+, y+, z+)
    ax.view_init(elev=30, azim=45)

    # Plot new data
    sc = ax.scatter(xs, ys, zs, c=zs, cmap='plasma', marker='o', s=40, alpha=0.9)
    return sc,

def main():
    print(f"Starting 3D Fitness Plotter...")
    print(f"Database: {DB_PATH}")

    # Setup Figure
    fig = plt.figure(figsize=(10, 7))
    fig.patch.set_facecolor("black")
    
    ax = fig.add_subplot(111, projection='3d')
    ax.set_facecolor("black")

    # Initial empty plot
    sc = ax.scatter([], [], [], c=[], cmap='plasma')
    
    # Add colorbar
    cbar = fig.colorbar(sc, ax=ax, pad=0.1, shrink=0.8)
    cbar.set_label('R (Robustness/Return)', color='white')
    cbar.ax.yaxis.set_tick_params(color='white')
    plt.setp(plt.getp(cbar.ax.axes, 'yticklabels'), color='white')

    # Animation
    ani = FuncAnimation(fig, update_plot, fargs=(sc, ax), interval=POLL_INTERVAL_MS, cache_frame_data=False)

    plt.show()

if __name__ == "__main__":
    main()
