# Projekt: [KUBE_Restore]

## 🤖 AI Agent Quickstart
**WICHTIG:** Bevor du Befehle ausführst oder Code analysierst, beachte diese Regeln:
1. **Umgebung:** Ich arbeite auf **Windows**. Verwende niemals `grep`, sondern immer PowerShell-Befehle wie `Select-String`.

2. **Programmier-Stil:** Wir programmieren nach dem Motto "Fast Fail". Keine Fallbacks einbauen, die später zu nicht auffindbaren Bugs führen. 
Wenn ein Wert nicht gefunden wird oder sonst irgendetwas klemmt, soll das System mit einer Fehlermeldung abstürzen.
Es gilt:
HARTE REGELN
- Keine Fallbacks.
- Keine try/except-Blöcke, die Fehler verschlucken.
- Keine Default-Werte bei fehlenden Pflichtdaten.
- Keine stillen Rückgaben wie null, [], {}, false, 0 oder "" bei Fehlern.
- Jeder unerwartete Zustand MUSS einen expliziten Fehler auslösen.
- Wenn die Anforderung ohne Fallback nicht sicher erfüllbar ist: STOPPEN und die Stelle markieren.

3. **Build-Anweisungen:** versuche nicht das Projekt zu bauen, ich kompiliere selbst

## System-Kontext:
 Dieses System ist hochkomplex. Lies für Details zur Architektur die Datei `/docs/Grundstruktur des KUBE_Systems.md`.
## Bestätigung

Gib mir ein Bestätigung wenn du das vorliegende Dokument,"Grundstruktur des KUBE_Systems.md" und die verlinkten Dokumente gelesen hast.
