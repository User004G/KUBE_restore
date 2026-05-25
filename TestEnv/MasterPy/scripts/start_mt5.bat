@echo off
setlocal

:: =================================================================================
:: 1. DEFINIERE DEN PFAD ZUM COMMON/FILES-VERZEICHNIS
:: Standardpfad für das gemeinsame MQL5-Datenverzeichnis (unabhängig von /portable)
set "COMMON_FILES_PATH=%APPDATA%\MetaQuotes\Terminal\Common\Files"
echo Pruefe Common Files Pfad: %COMMON_FILES_PATH%

:: =================================================================================
:: 2. ALLE DATEIEN LÖSCHEN
echo Loesche alle Dateien (*.*) im Verzeichnis...

:: **ÄNDERUNG HIER:** Loesche alle Dateien (*.*). /Q unterdrueckt die Sicherheitsabfrage.
del /Q "%COMMON_FILES_PATH%\*.*"

:: =================================================================================
:: 3. ERGEBNIS PRÜFEN (Optional, um zu sehen, ob das Verzeichnis jetzt leer ist)
:: Hinweis: Mit dieser universellen Loeschung ist eine einfache if-exist-Pruefung
:: fuer eine einzelne Datei nicht mehr sinnvoll. Wir pruefen, ob noch Dateien da sind.
dir /B "%COMMON_FILES_PATH%" > nul
if %errorlevel% equ 0 (
    echo ✅ Alle Dateien im Verzeichnis erfolgreich geloescht.
) else (
    echo ⚠️ ACHTUNG: Das Verzeichnis war entweder leer oder konnte nicht zugegriffen werden.
)

:: Um das Skript mit Ihrer ursprünglichen Logik fortzusetzen, 
:: koennen Sie die einfache if-exist-Pruefung fuer *.* weglassen.

:: =================================================================================
:: 3. STARTE DIE TRADER-TERMINALS
echo Starte Livetrader...
start "Livetrader" "C:\MQL_Projekt\Livetrader\terminal64.exe" /portable /config:"C:\MQL_Projekt\Livetrader\Config\KUBE_autostart_live.ini"
timeout /t 3 /nobreak

echo Starte Papertrader10...
start "Papertrader10" "C:\MQL_Projekt\Papertrader10\terminal64.exe" /portable /config:"C:\MQL_Projekt\Papertrader10\Config\KUBE_autostart_paper10.ini"
timeout /t 1 /nobreak
::=================================================================================================

pause
exit
