import re

file_path = r'c:\KUBE_Restore\MasterPy\Gui.py'
with open(file_path, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Add DraggableModuleTile at the top before class Gui
tile_class = '''
class DraggableModuleTile(ctk.CTkButton):
    def __init__(self, master, module_key, text, **kwargs):
        super().__init__(master, text=text, **kwargs)
        self.module_key = module_key
        self.master_gui = master.winfo_toplevel()
        
        self.bind("<Button-1>", self.on_drag_start)
        self.bind("<B1-Motion>", self.on_drag_motion)
        self.bind("<ButtonRelease-1>", self.on_drag_end)

        self.drag_window = None

    def on_drag_start(self, event):
        self.drag_window = ctk.CTkToplevel()
        self.drag_window.overrideredirect(True)
        self.drag_window.attributes("-topmost", True)
        self.drag_window.attributes("-alpha", 0.7)
        
        lbl = ctk.CTkLabel(self.drag_window, text=self.cget("text"), 
                           fg_color=self.cget("fg_color"), 
                           font=self.cget("font"),
                           corner_radius=5,
                           width=self.winfo_width(),
                           height=self.winfo_height())
        lbl.pack()
        
        self._offset_x = event.x
        self._offset_y = event.y
        
        x = self.winfo_pointerx() - self._offset_x
        y = self.winfo_pointery() - self._offset_y
        self.drag_window.geometry(f"+{x}+{y}")

    def on_drag_motion(self, event):
        if self.drag_window:
            x = self.winfo_pointerx() - self._offset_x
            y = self.winfo_pointery() - self._offset_y
            self.drag_window.geometry(f"+{x}+{y}")

    def on_drag_end(self, event):
        if self.drag_window:
            self.drag_window.destroy()
            self.drag_window = None

        root = self.master_gui
        if hasattr(root, "handle_drop"):
            x_root = self.winfo_pointerx()
            y_root = self.winfo_pointery()
            root.handle_drop(x_root, y_root, self.module_key)

class Gui(ctk.CTk):
'''
if "class DraggableModuleTile" not in code:
    code = code.replace("class Gui(ctk.CTk):", tile_class)

# 2. Remove on_stat_view_change
code = re.sub(r'    def on_stat_view_change.*?self._redraw_statistik\(\)\n', '', code, flags=re.DOTALL)

# 3. Remove on_range_change
code = re.sub(r'    def on_range_change.*?pass\n', '', code, flags=re.DOTALL)

# 4. Refactor _init_LO_frame -> _init_balance_plot
code = code.replace("def _init_LO_frame(self):", "def _init_balance_plot(self, target_frame):")
code = code.replace("self.LO_frame", "target_frame")

# 5. Refactor _init_RO_frame -> _init_risk_plot
code = code.replace("def _init_RO_frame(self):", "def _init_risk_plot(self, target_frame):")
code = code.replace("self.RO_frame", "target_frame")

# 6. Refactor _init_LU_frame -> _init_3d_input_plot
code = code.replace("def _init_LU_frame(self):", "def _init_3d_input_plot(self, target_frame):")
code = code.replace("self.LU_frame", "target_frame")

# 7. Refactor _init_RU_frame -> _init_3d_output_plot
code = code.replace("def _init_RU_frame(self):", "def _init_3d_output_plot(self, target_frame):")
code = code.replace("self.RU_frame", "target_frame")

# 8. Modify _redraw_balance
code = re.sub(r'    def _redraw_balance\(self\):\n        mode = self.balance_range.get\(\)\n', 
              '    def _redraw_balance(self):\n        if not hasattr(self, "ax_bal"): return\n        mode = "max"\n', code)

# 9. Modify _redraw_statistik
# Replace logic around stat_view_mode
code = re.sub(r'    def _redraw_statistik\(self\):.*?if self.stat_view_mode == "RiskManager":',
              '    def _redraw_statistik(self):\n        if not hasattr(self, "ax_sta"): return\n        mode = "max"\n        if True:', 
              code, flags=re.DOTALL)
# Remove the else branch for "Anzahl aktive EAs"
# The else block for OPTION B – Anzahl aktive EAs needs to be removed.
# Let's find OPTION B and remove it
opt_b_pattern = r'        # ------------------------------\n        # OPTION B – Anzahl aktive EAs\n        # ------------------------------\n        else:\n.*?# Info-Box aktualisieren'
code = re.sub(opt_b_pattern, '        # Info-Box aktualisieren', code, flags=re.DOTALL)

# Also update update_3d_view
code = code.replace('    def _update_3d_view(self):', '    def _update_3d_view(self):\n        if not hasattr(self, "ax_3d"): return')
code = code.replace('    def _update_fitness_3d_view(self):', '    def _update_fitness_3d_view(self):\n        if not hasattr(self, "ax_fit3d"): return')

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(code)

print("Refactoring done.")
