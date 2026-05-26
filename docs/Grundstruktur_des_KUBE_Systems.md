# 🧠 Grundstruktur des KUBE-Systems (Restore Edition)

Das KUBE-System ist eine hybride, hochkomplexe Trading-Architektur, die mit Schwarmintelligenz, Parallelstrukturen, evolutionären Algorithmen und selbstadaptiven Mechanismen arbeitet. Das System ist primär für Forexmärkte optimiert und nutzt M30-Charts.

---

## 1. Systemarchitektur & Komponenten
Das System ist in drei funktionale und sprachliche Ebenen unterteilt.

* **Die MQL5-Ebene (Das Frontend)**
  Zuständig für die Kommunikation mit dem Broker, Orderausführung und Trade-Management in den MetaTrader 5 Terminals.
  🔗 *Detail-Dokumentation: [MQL5_Terminals_und_Experten.md](mql5_terminals.md)*

* **Die C++ Engine (Der Motor)**
  Zuständig für High-Speed-Simulation, Backtesting, Fitness-Berechnungen und die evolutionäre Mutation der Strategien. Kernmodule umfassen unter anderem den `MutationsManager`, `FitnessManager`, `LotManager`, `MultiObjectiveManager` und `Aktivator` für dynamische Systemsteuerung und evolutionäre Vielfalt.
  🔗 *Detail-Dokumentation: [CPP_Engine_Logik.md](cpp_engine.md)*

* **Der Python Master (Visualisierung & Systemstart)**
  Ist initial für den komplexen Startvorgang des Gesamtsystems zuständig. Nach dem erfolgreichen Bootprozess klinkt sich der Python-Masterloop aus der aktiven Logik - insbesondere der Timebeat-Taktung - aus und dient ausschließlich der Visualisierung (Dashboards per `Gui.py`).
  🔗 *Detail-Dokumentation: [Python_Master_und_GUI.md](python_master.md)*

---

## 2. Kommunikation & Synchronisation
Da drei unterschiedliche Programmiersprachen parallel laufen, ist eine strikte Taktung der entscheidenden Module erforderlich.

* **Die zentralen Regelschleifen (Timebeats):** 
  Die tatsächlichen Haupt-Loops des Systems sind die `OnTick`-Funktionen im MQL5-Code der Terminals sowie die Regelschleife in der `Synchronisator.cpp` auf C++ Seite. Die in von der OnTick-Funktion aufgerufenen Funktionen und der Synchronisator stellt dabei den Gleichtakt zwischen der C++ Engine und MQL5 her, sodass Live- und Paperschwarm, sowie die CPP-Engine perfekt harmonieren.
* **Shared Files:** Gemeinsam genutzte Dateien und Symlinks liegen zentral im Ordner `C:\MQL_Shared_Restore\Includes` (inkl. `KUBE_config.mqh`).
🔗 *Detail-Dokumentation: [Synchronisation_und_Dateistruktur.md](synchronisation.md)*

---

## 3. Die Datenbanken (SQLite)
Die asynchrone Kommunikation zwischen den Komponenten erfolgt rein über lokale SQLite-Datenbanken im MetaQuotes `Common/Files` Verzeichnis.

* **`KUBE_Schwarm.db`:** EA-Parameter, Konfigurationen und Balance-Historie.
* **`KUBE_Mutation.db`:** Das evolutionäre Gedächtnis (Performance Space) und Austausch von Mutationen (ersetzt das alte Grid-Konzept).
* **`KUBE_Experts.db`:** Echtzeit-Statusaustausch und Inter-Prozess-Signale.
🔗 *Detail-Dokumentation: [Datenbank_Schemata.md](datenbank_schemata.md)*

---

## 4. Die evolutionäre Schwarm-Logik
Das Kernkonzept der autonomen Signalgenerierung basiert auf der Nutzung von zwei Schwärmen mit jeweils **216 Expert Advisors (EAs)**, die sich nur durch 3 Parameter unterscheiden. 

* **Duale Struktur & Interaktion (Live vs. Paper):**
  Das System nutzt zwei korrelierende Schwärme, die grundlegend exakt identisch parametrisiert sind:
  - **Paperschwarm (Der "Scout"):** Handelt explorativ auf einem virtuellen Demokonto. Hier sind **alle 216 EAs immer aktiv** und handeln mit einem minimalen Basis-Volumen. Der Paperschwarm dient als permanenter Echtzeit-Sensor für die aktuelle Marktdynamik.
  - **Liveschwarm (Der "Trader"):** Handelt auf dem realen Konto. Welche der 216 EAs hier überhaupt Trades absetzen dürfen (Aktivierung) und mit welchem Hebel (Lot-Size) sie agieren, wird dynamisch und kontinuierlich aus der Performance-Rückkopplung und Fitness-Bewertung des Paperschwarms abgeleitet.
  🔗 *Detail-Dokumentation: [Live_und_Paper_Schwarm.md](duale_schwarm_logik.md)*

---

## 5. Das Risiko-Management
Der RiskManager arbeitet asymmetrisch und überwacht ausschließlich den Paper-Schwarm, um das reale Live-Kapital proaktiv vor Drawdowns zu schützen.

* **Peak, Floor & Cushion:** Der RiskManager berechnet kontinuierlich den historischen Höchststand (Peak), den festgelegten Tiefststand (Floor) und das daraus resultierende Risikopolster (Cushion) der Paper-Schwarm-Equity.
* **Exponentielle Skalierung:** Befindet sich der Paper-Schwarm in einer Drawdown-Phase (Equity nähert sich dem Floor), werden über eine negative exponentielle Skalierung das Lot-Volumen und die Anzahl der aktiven EAs im Liveschwarm drastisch reduziert. Steigt die Performance hingegen an, werden diese Werte positiv exponentiell hochskaliert.
* **Dynamische Steuerung:** Flankiert wird dies durch dynamische Metriken wie `lambda_dyn` und `active_frac`, welche die Sensibilität und Aggressivität dieses Steuermechanismus kontinuierlich anpassen.
🔗 *Detail-Dokumentation: [Risk_Management.md](risk_management.md)*
---
## 6. Aktivator, Evolution und Mutation
Um sich stetig an veränderte Marktphasen anzupassen, nutzt das System einen evolutionären Algorithmus, der maßgeblich durch den `MutationsManager` (C++) gesteuert wird. Eine essenzielle architektonische Entscheidung ist hierbei die strenge **Trennung der Selektionsmechanismen** für Mutation (Aussortieren) und Aktivierung (Beförderung in den Live-Handel):

* **Aktivierung (Pareto-Front 1):** Die Entscheidung, welche EAs im Live-Konto traden dürfen, trifft der `Aktivator` mithilfe des `MultiObjectiveManager` über eine mehrdimensionale Auswertung (z.B. NetProfitNorm, ActivityNorm, normiertes Drawdown-Risiko). Nur die absoluten "Elite-EAs", die auf der Pareto-Front 1 liegen (der beste Kompromiss aus Profit, Frequenz und Robustheit), werden aktiviert.
* **Mutation (Aussortieren nach Leistung):** Die Entscheidung, welche EAs mutiert werden, ist hingegen eindimensional. Der `SelectionManager` identifiziert gezielt die schlechtesten Performer (die größten Verlierer) des gesamten Schwarms rein anhand der Metrik `NetProfitNorm`. 

Daraus ergibt sich der konkrete Ablauf der Mutation:
* **Leistungsbezogene Selektion:** In jedem Zyklus werden primär die 24 EAs mit dem schlechtesten `NetProfitNorm` des gesamten Schwarms selektiert und überschrieben. Dies verhindert abrupte Systembrüche, da der profitable Kern unberührt bleibt.
* **Fallback (Blockmutation):** Sollte die leistungsbezogene Auswertung fehlschlagen (z.B. fehlende Fitness-Daten beim Systemstart), fällt das System auf eine "stumpfe Rotation" zurück. Dabei werden statisch vordefinierte Blöcke (0-23, 24-47, etc.) reihum mutiert, um einen Systemstillstand zu verhindern.
* **Form der Mutation (Random):** Die Neuzuweisung der Parameter (`param1`, `param2`, `param3`) erfolgt aktuell über die `DefaultMutationStrategy` als reine Zufalls-Gleichverteilung (Random). Es findet keine Kreuzmutation oder NearBest-Suche statt; die ausselektierten EAs werden innerhalb der Grenzen blind neu ausgewürfelt, um eine hohe Explorationsrate zu gewährleisten.
* **Konsistenz:** Jede genetische Anpassung wird stets identisch und zeitgleich im Live- und Paperschwarm durchgeführt. Dieser Prozess wird in der `KUBE_Mutation.db` geloggt. 🔗 *Detail-Dokumentation: [Mutations_Logik.md](mutations_logik.md)*
---

## 7. Systemstart & Bootvorgang
Die genaue Reihenfolge, wie Terminals, Engine und Python-Master hochgefahren und synchronisiert werden.
🔗 *Detail-Dokumentation: [StartProzedur.md](start_prozedur.md)*



