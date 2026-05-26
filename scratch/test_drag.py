import customtkinter as ctk

app = ctk.CTk()
app.geometry("400x300")

def drag_start(event):
    drag_window = ctk.CTkToplevel(app) # or without app
    drag_window.overrideredirect(True)
    drag_window.attributes("-topmost", True)
    drag_window.attributes("-alpha", 0.7)
    
    lbl = ctk.CTkLabel(drag_window, text=btn.cget("text"), 
                       fg_color=btn.cget("fg_color"), 
                       font=btn.cget("font"),
                       corner_radius=5,
                       width=btn.winfo_width(),
                       height=btn.winfo_height())
    lbl.pack()
    
    drag_window.geometry(f"+{event.x_root}+{event.y_root}")

btn = ctk.CTkButton(app, text="Test Drag", width=70, height=28)
btn.pack(pady=50)
btn.bind("<Button-1>", drag_start)

app.mainloop()
