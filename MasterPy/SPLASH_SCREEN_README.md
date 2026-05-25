# KUBE Trading - Splash Screen

## Übersicht

Der neue Splash Screen bietet einen professionellen und ansprechenden Start für die KUBE Trading Anwendung.

## Features

### 🎨 Visuelles Design
- **Modernes Logo**: Zeigt das KUBE-Logo prominent mit abgerundeten Ecken
- **Dunkles Theme**: Konsistentes Design mit der Hauptanwendung
- **Glühender Rand**: Cyan-farbiger Rahmen für Premium-Look
- **Animierter Ladebalken**: Zeigt den Fortschritt des Ladevorgangs

### 📊 Lade-Phasen
1. **Initialisiere System** (0-20%)
2. **Lade Konfiguration** (20-40%)
3. **Verbinde zu Datenbanken** (40-60%)
4. **Starte MasterLoop** (60-80%)
5. **Initialisiere GUI** (80-100%)

### ✨ Animationen
- **Fortschrittsbalken**: Smooth animierter Ladebalken
- **Status-Punkte**: Animierte Punkte beim Laden
- **Prozent-Anzeige**: Echtzeit-Fortschrittsanzeige
- **Fade-Out**: Sanfter Übergang zur Hauptanwendung

## Technische Details

### Dateien
- `SplashScreen.py`: Splash Screen Komponente
- `Visual_KUBE.py`: Haupteinstiegspunkt mit Integration

### Anpassungen

#### Logo ändern
Ersetzen Sie das Bild unter:
```
C:\KUBE_Trading\Entwicklung\Icons\Logo_final_klein.jpg
```

#### Lade-Dauer anpassen
In `Visual_KUBE.py`, Zeile 33-38:
```python
loading_phases = [
    ("Initialisiere System...", 10),
    ("Lade Konfiguration...", 25),
    # ... weitere Phasen
]
```

#### Farben anpassen
In `SplashScreen.py`:
- **Hintergrund**: `fg_color="#0a0a0a"` (Zeile 36)
- **Rahmen**: `border_color="#00bfff"` (Zeile 42)
- **Titel**: `text_color="#00bfff"` (Zeile 141)

### Fallback-Modus
Falls das Logo nicht gefunden wird, zeigt der Splash Screen automatisch ein stilisiertes "KUBE" Text-Logo an.

## Verwendung

### Standard-Start
```bash
python Visual_KUBE.py
```

### Nur Splash Screen testen
```bash
python SplashScreen.py
```

## Vorteile

✅ **Professioneller Eindruck**: Hochwertige Optik beim Start
✅ **Benutzer-Feedback**: Klare Anzeige des Ladefortschritts
✅ **Markenbildung**: Prominente Logo-Darstellung
✅ **Smooth UX**: Sanfte Übergänge und Animationen
✅ **Fehlertoleranz**: Fallback bei fehlendem Logo

## Screenshots

Der Splash Screen zeigt:
- Großes, zentriertes Logo (280x280px)
- "KUBE TRADING" Titel in Cyan
- "Advanced Trading System" Untertitel
- Versions-Information
- Animierter Status-Text
- Fortschrittsbalken mit Prozent-Anzeige
- Copyright und Branding-Information

## Zukünftige Erweiterungen

Mögliche Verbesserungen:
- [ ] Fade-In Animation beim Start
- [ ] Konfigurierbarer Splash Screen über JSON
- [ ] Mehrsprachige Status-Meldungen
- [ ] System-Check während des Ladens
- [ ] Error-Handling mit Fehlermeldungen im Splash Screen
