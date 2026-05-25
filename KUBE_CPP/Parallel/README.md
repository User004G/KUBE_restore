# Parallel Mutation Subsystem

Dieses Verzeichnis enthält die parallele Mutations-Infrastruktur für das KUBE Trading System.

## Übersicht

Der **ParallelMutationOrchestrator** orchestriert vier Manager-Klassen in separaten Hintergrund-Threads:

1. **MutationDataManager** - Lädt kontinuierlich Fitness-Daten aus der Datenbank
2. **SwarmContextEvaluator** - Analysiert den Marktkontext für Mutationsentscheidungen
3. **SwarmQualityEvaluator** - Bewertet die Qualität des Swarms (NORMAL/SUCCESS/STRESS)
4. **SelectionManager** - Wählt Best/Worst EAs für Mutationen aus

## Architektur

```
┌─────────────────────────────────────────────────────────────┐
│                    Synchronisator (Main Thread)             │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         ParallelMutationOrchestrator                 │  │
│  │                                                      │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────┐ │  │
│  │  │ Data     │  │ Context  │  │ Quality  │  │ Sel │ │  │
│  │  │ Manager  │  │ Eval     │  │ Eval     │  │ Mgr │ │  │
│  │  │ Thread   │  │ Thread   │  │ Thread   │  │ Thd │ │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └─────┘ │  │
│  │                                                      │  │
│  │  Shared Data (Mutex-protected):                     │  │
│  │  - latest_fitness_data_                             │  │
│  │  - current_swarm_state_                             │  │
│  │  - current_context_                                 │  │
│  │  - best_eas_ / worst_eas_                           │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                  │
│                  MutationsManager                           │
│                  (nutzt vorbereitete Daten)                 │
└─────────────────────────────────────────────────────────────┘
```

## Verwendung

### Integration in Synchronisator.cpp

```cpp
#include "Parallel/ParallelMutationOrchestrator.h"

// Nach MutationsManager-Initialisierung:
ParallelMutationOrchestrator orchestrator(
    cfg.db_experts_path,
    cfg.db_schwarm_path,
    db_mutation_path,  // abgeleitet von db_schwarm_path
    cfg.config_path,
    0,                 // swarm_live
    cfg.swarm_id       // swarm_paper
);

if (!orchestrator.start()) {
    LOGF("[ERROR] Failed to start ParallelMutationOrchestrator");
    die("Orchestrator start failed");
}

// ... Hauptschleife läuft ...

// Beim Herunterfahren:
orchestrator.stop();
```

### Daten abrufen

```cpp
// Fitness-Daten holen
std::vector<EAParams> fitness_data;
if (orchestrator.get_latest_fitness_data(fitness_data)) {
    // Verwende fitness_data
}

// Swarm-State holen
SwarmState state = orchestrator.get_current_swarm_state();

// Kontext holen
MutationContext ctx = orchestrator.get_current_context();

// Best/Worst EAs holen
std::vector<EAParams> best, worst;
if (orchestrator.get_best_worst_eas(best, worst)) {
    // Verwende best/worst für Mutation
}
```

## Thread-Sicherheit

Alle öffentlichen Getter-Methoden sind **thread-sicher** durch `std::mutex`-Schutz.

Die Hintergrund-Threads laufen unabhängig vom Hauptsystem und blockieren **niemals** den Synchronisator.

## Aktueller Status

✅ **Implementiert**:
- Thread-Management (start/stop)
- Thread-sichere Datenzugriffe
- Grundstruktur aller 4 Manager

⚠️ **Stub/Dummy**:
- MutationDataManager.load_fitness_data() - lädt noch keine echten Daten
- SwarmContextEvaluator - gibt nur Dummy-Kontext zurück
- SwarmQualityEvaluator - gibt immer NORMAL zurück
- SelectionManager - funktioniert, aber braucht echte performance_metric-Werte

## Nächste Schritte

1. **MutationDataManager**: Echte SQL-Abfrage für Fitness_proBar implementieren
2. **SwarmContextEvaluator**: Marktkontext-Analyse implementieren
3. **SwarmQualityEvaluator**: Swarm-Qualitätsbewertung implementieren
4. **Integration**: MutationsManager nutzt vorbereitete Daten aus Orchestrator

## Dateien

- `ParallelMutationOrchestrator.h` - Header mit Klassen-Definition
- `ParallelMutationOrchestrator.cpp` - Implementierung des Orchestrators
- `MutationDataManager.cpp` - Implementierung des Data Managers
- `SelectionManager.cpp` - Implementierung des Selection Managers
- `README.md` - Diese Datei
