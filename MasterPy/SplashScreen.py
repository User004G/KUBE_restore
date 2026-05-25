"""
SplashScreen.py - Ansprechender Startbildschirm für KUBE Trading
"""
import customtkinter as ctk
from PIL import Image, ImageTk, ImageDraw, ImageFilter
from pathlib import Path
import threading
import time


class SplashScreen(ctk.CTkToplevel):
    """
    Moderner Splash Screen mit Logo, Ladeanimation und Statusanzeige
    """
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        # Fenster-Konfiguration
        self.title("")
        self.overrideredirect(True)  # Entfernt Titelleiste
        
        # Größe und Position
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        
        splash_width = 600
        splash_height = 700
        
        x = (screen_width - splash_width) // 2
        y = (screen_height - splash_height) // 2
        
        self.geometry(f"{splash_width}x{splash_height}+{x}+{y}")
        
        # Immer im Vordergrund
        self.attributes('-topmost', True)
        
        # Hintergrund - KUBE System Farbe
        self.configure(fg_color="#0a0a0a")
        
        # Hauptframe mit Rand-Effekt - KUBE System Farben
        self.main_frame = ctk.CTkFrame(
            self,
            fg_color="#1e1e1e",  # KUBE Hintergrundfarbe
            corner_radius=20,
            border_width=2,
            border_color="#00bfff"  # KUBE Akzentfarbe (Cyan)
        )
        self.main_frame.pack(fill="both", expand=True, padx=3, pady=3)
        
        # Logo-Bereich
        self._create_logo_section()
        
        # Titel-Bereich
        self._create_title_section()
        
        # Status-Bereich
        self._create_status_section()
        
        # Ladebalken
        self._create_progress_section()
        
        # Fußzeile
        self._create_footer()
        
        # Animation-Variablen
        self.progress_value = 0
        self.loading_dots = 0
        self.is_loading = True
        
        # After-IDs für Cleanup speichern
        self._progress_after_id = None
        self._status_after_id = None
        
        # Starte Animationen
        self._animate_progress()
        self._animate_status()
    
    def _create_logo_section(self):
        """Erstellt den Logo-Bereich mit Schatten-Effekt"""
        logo_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="transparent"
        )
        logo_frame.pack(pady=(40, 20))
        
        # Logo laden und anzeigen
        logo_path = Path(r"C:\KUBE_Trading\Entwicklung\Icons\Logo_final_klein.jpg")
        
        if logo_path.exists():
            try:
                # Logo laden
                logo_img = Image.open(logo_path)
                
                # Größe anpassen
                logo_size = 280
                logo_img = logo_img.resize((logo_size, logo_size), Image.Resampling.LANCZOS)
                
                # Runde Ecken hinzufügen
                logo_img = self._add_rounded_corners(logo_img, radius=30)
                
                # CTkImage erstellen
                self.logo_ctk = ctk.CTkImage(
                    light_image=logo_img,
                    dark_image=logo_img,
                    size=(logo_size, logo_size)
                )
                
                # Logo-Label
                logo_label = ctk.CTkLabel(
                    logo_frame,
                    image=self.logo_ctk,
                    text=""
                )
                logo_label.pack()
                
            except Exception as e:
                print(f"[SplashScreen] Fehler beim Laden des Logos: {e}")
                self._create_fallback_logo(logo_frame)
        else:
            self._create_fallback_logo(logo_frame)
    
    def _create_fallback_logo(self, parent):
        """Erstellt ein Fallback-Logo wenn das Bild nicht gefunden wird"""
        fallback_frame = ctk.CTkFrame(
            parent,
            width=280,
            height=280,
            fg_color="#00bfff",  # KUBE Cyan
            corner_radius=30
        )
        fallback_frame.pack()
        fallback_frame.pack_propagate(False)
        
        ctk.CTkLabel(
            fallback_frame,
            text="KUBE",
            font=("Segoe UI", 72, "bold"),
            text_color="#1e1e1e"  # Dunkler Hintergrund für Kontrast
        ).place(relx=0.5, rely=0.5, anchor="center")
    
    def _add_rounded_corners(self, image, radius=30):
        """Fügt runde Ecken zu einem Bild hinzu"""
        # Maske für runde Ecken erstellen
        mask = Image.new('L', image.size, 0)
        draw = ImageDraw.Draw(mask)
        draw.rounded_rectangle([(0, 0), image.size], radius=radius, fill=255)
        
        # Alpha-Kanal hinzufügen
        if image.mode != 'RGBA':
            image = image.convert('RGBA')
        
        # Maske anwenden
        output = Image.new('RGBA', image.size, (0, 0, 0, 0))
        output.paste(image, (0, 0))
        output.putalpha(mask)
        
        return output
    
    def _create_title_section(self):
        """Erstellt den Titel-Bereich"""
        title_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="transparent"
        )
        title_frame.pack(pady=(10, 5))
        
        # Haupttitel
        title_label = ctk.CTkLabel(
            title_frame,
            text="KUBE TRADING",
            font=("Segoe UI", 42, "bold"),
            text_color="#00bfff"
        )
        title_label.pack()
        
        # Untertitel
        subtitle_label = ctk.CTkLabel(
            title_frame,
            text="Advanced Trading System",
            font=("Segoe UI", 16),
            text_color="#888888"
        )
        subtitle_label.pack(pady=(5, 0))
        
        # Version
        version_label = ctk.CTkLabel(
            title_frame,
            text="v2.0 Beta",
            font=("Segoe UI", 12),
            text_color="#666666"
        )
        version_label.pack(pady=(5, 0))
    
    def _create_status_section(self):
        """Erstellt den Status-Bereich"""
        status_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="transparent"
        )
        status_frame.pack(pady=(30, 10))
        
        self.status_label = ctk.CTkLabel(
            status_frame,
            text="Initialisiere System...",
            font=("Segoe UI", 14),
            text_color="#ffffff"
        )
        self.status_label.pack()
    
    def _create_progress_section(self):
        """Erstellt den Ladebalken-Bereich"""
        progress_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="transparent"
        )
        progress_frame.pack(pady=(10, 20), padx=80, fill="x")
        
        # Ladebalken - KUBE Farben (Yellow für LIVE/Aktiv)
        self.progress_bar = ctk.CTkProgressBar(
            progress_frame,
            width=400,
            height=8,
            corner_radius=4,
            progress_color="yellow",  # KUBE LIVE Farbe
            fg_color="#333333"
        )
        self.progress_bar.pack(fill="x")
        self.progress_bar.set(0)
        
        # Prozent-Anzeige
        self.percent_label = ctk.CTkLabel(
            progress_frame,
            text="0%",
            font=("Segoe UI", 12),
            text_color="#888888"
        )
        self.percent_label.pack(pady=(5, 0))
    
    def _create_footer(self):
        """Erstellt die Fußzeile"""
        footer_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="transparent"
        )
        footer_frame.pack(side="bottom", pady=(0, 20))
        
        # Copyright
        copyright_label = ctk.CTkLabel(
            footer_frame,
            text="© 2025 KUBE Trading Systems",
            font=("Segoe UI", 10),
            text_color="#555555"
        )
        copyright_label.pack()
        
        # Zusatzinfo
        info_label = ctk.CTkLabel(
            footer_frame,
            text="Powered by AI & Machine Learning",
            font=("Segoe UI", 9),
            text_color="#444444"
        )
        info_label.pack(pady=(3, 0))
    
    def _animate_progress(self):
        """Animiert den Ladebalken"""
        # Vorherige Callback-ID löschen
        if self._progress_after_id is not None:
            self._progress_after_id = None
        
        if not self.is_loading:
            return
        
        # Simuliere Ladefortschritt
        if self.progress_value < 100:
            # Nicht-linearer Fortschritt für realistischeres Laden
            if self.progress_value < 30:
                increment = 2
            elif self.progress_value < 70:
                increment = 1
            else:
                increment = 0.5
            
            self.progress_value = min(100, self.progress_value + increment)
            self.progress_bar.set(self.progress_value / 100)
            self.percent_label.configure(text=f"{int(self.progress_value)}%")
        
        # Weiter animieren - ID speichern
        self._progress_after_id = self.after(50, self._animate_progress)
    
    def _animate_status(self):
        """Animiert den Status-Text mit Punkten"""
        # Vorherige Callback-ID löschen
        if self._status_after_id is not None:
            self._status_after_id = None
        
        if not self.is_loading:
            return
        
        # Lade-Phasen
        phases = [
            "Initialisiere System",
            "Lade Konfiguration",
            "Verbinde zu Datenbanken",
            "Starte MasterLoop",
            "Initialisiere GUI",
            "Bereit"
        ]
        
        # Bestimme aktuelle Phase basierend auf Fortschritt
        phase_index = min(int(self.progress_value / 20), len(phases) - 1)
        base_text = phases[phase_index]
        
        # Animierte Punkte
        dots = "." * (self.loading_dots % 4)
        self.status_label.configure(text=f"{base_text}{dots}")
        
        self.loading_dots += 1
        
        # Weiter animieren - ID speichern
        self._status_after_id = self.after(400, self._animate_status)
    
    def update_status(self, message: str, progress: float = None):
        """
        Aktualisiert den Status-Text und optional den Fortschritt
        
        Args:
            message: Neue Status-Nachricht
            progress: Fortschritt in Prozent (0-100), optional
        """
        self.status_label.configure(text=message)
        
        if progress is not None:
            self.progress_value = progress
            self.progress_bar.set(progress / 100)
            self.percent_label.configure(text=f"{int(progress)}%")
    
    def stop_animations(self):
        """
        Stoppt alle laufenden Animationen und bricht ausstehende after()-Callbacks ab.
        MUSS vor destroy() aufgerufen werden!
        """
        self.is_loading = False
        
        # Alle ausstehenden after()-Callbacks abbrechen
        if self._progress_after_id is not None:
            try:
                self.after_cancel(self._progress_after_id)
            except:
                pass
            self._progress_after_id = None
        
        if self._status_after_id is not None:
            try:
                self.after_cancel(self._status_after_id)
            except:
                pass
            self._status_after_id = None
    
    def finish(self, callback=None):
        """
        Beendet den Splash Screen mit Fade-Out-Effekt
        
        Args:
            callback: Funktion, die nach dem Schließen aufgerufen wird
        """
        self.stop_animations()  # Animationen stoppen BEVOR wir schließen
        
        self.progress_value = 100
        self.progress_bar.set(1.0)
        self.percent_label.configure(text="100%")
        self.status_label.configure(text="Starte Anwendung...")
        
        # Kurze Pause vor dem Schließen
        def close_splash():
            time.sleep(0.5)
            self.destroy()
            if callback:
                callback()
        
        threading.Thread(target=close_splash, daemon=True).start()


def show_splash_screen(duration=3.0):
    """
    Zeigt den Splash Screen für eine bestimmte Dauer an
    
    Args:
        duration: Anzeigedauer in Sekunden
    
    Returns:
        SplashScreen-Instanz
    """
    # Erstelle unsichtbares Hauptfenster
    root = ctk.CTk()
    root.withdraw()
    
    # Erstelle Splash Screen
    splash = SplashScreen(root)
    
    # Automatisch schließen nach Dauer
    def auto_close():
        time.sleep(duration)
        splash.finish()
    
    threading.Thread(target=auto_close, daemon=True).start()
    
    return splash, root


if __name__ == "__main__":
    # Test des Splash Screens
    ctk.set_appearance_mode("dark")
    
    splash, root = show_splash_screen(duration=5.0)
    root.mainloop()
