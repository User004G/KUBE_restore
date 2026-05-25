// MutationsManager.h
#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>
#include <random>

#include "MutationStrategy.h"   // IMutationStrategy, MutationAction (nur Deklaration)
#include "DefaultMutationStrategy.h"
#include "Parallel/SelectionManager.h"


// Vorwärtsdeklarationen (Definitionen stehen in eigenen .h/.cpp, aktuell Dummy)
class SwarmQualityEvaluator;
class SwarmContextEvaluator;

// -------------------------------------------------------
// Gemeinsame Typen für Manager & Strategien
// -------------------------------------------------------
enum class SwarmState {
    NORMAL,
    SUCCESS,
    STRESS
};

struct EAParams {
    int    magic = 0;
    int    param1 = 0;
    int    param2 = 0;
    int    param3 = 0;
    bool   active = false;
    double lot = 0.0;
    int    swarm_id = 0;
    double performance_metric = 0.0;  // später für Fitness nutzbar
};

struct MutationAction {
    int magic = 0;
    int new_param1 = 0;
    int new_param2 = 0;
    int new_param3 = 0;
};

struct MutationContext {
    std::string current_timestamp;

    // Bounds für die Zufalls-Mutation
    int param1_min = 0;
    int param1_max = 0;
    int param2_min = 0;
    int param2_max = 0;
    int param3_min = 0;
    int param3_max = 0;

    // Abgeleitete Bewährungsfrist (in Bars) = c_schonfrist_multiplikator * c_mutations_zyklus
    int probation_bars = 0;

    // Platzhalter für spätere Kontextgrößen (Volatilität etc.)
    double current_volatility = 0.0;
    double system_equity = 0.0;
};

// -------------------------------------------------------
// MutationsManager
//  - hält Mutationszyklus (mutation_counter, mutation_zyklus)
//  - entscheidet WANN mutiert wird (tick_once)
//  - orchestriert Evaluatoren + Strategien
//  - schreibt als einziger in KUBE_Mutation.db
// -------------------------------------------------------
class MutationsManager
{
public:
    /**
     * Constructor
     * @param db_schwarm_path Pfad zu KUBE_Schwarm.db
     * @param config_path     Pfad zu KUBE_config.mqh
     * @param swarm_live      Live-Schwarm-ID (z.B. 0)
     * @param swarm_paper     Paper-Schwarm-ID (z.B. 10)
     */
    MutationsManager(const std::string& db_schwarm_path,
        const std::string& config_path,
        int swarm_live = 0,
        int swarm_paper = 10);

    ~MutationsManager();

    /// Initialisiert DB, Config, Evaluatoren, Strategien
    bool init();

    /// Wird vom Synchronisator bei jedem M30-Schritt aufgerufen
    bool tick_once(const std::string& ts);

    /// Aktuellen SwarmState (NORMAL/SUCCESS/STRESS) abfragen
    SwarmState current_state() const { return current_state_; }

    /// State von außen ändern (aktuell nur Logging)
    void change_state(SwarmState new_state);

    // --- von außen lesbar, aber Wert kommt aus config ---
    int mutation_counter = 0;   // wird bei jedem M30-Schritt erhöht
    int mutation_zyklus = 10;  // wird in read_config() aus c_mutations_zyklus überschrieben

private:
    // --- DB-Handles ---
    sqlite3* db_schwarm_ = nullptr;  // KUBE_Schwarm.db
    sqlite3* db_mutation_ = nullptr;  // KUBE_Mutation.db

    // --- Pfade ---
    std::string db_schwarm_path_;
    std::string db_mutation_path_;
    std::string config_path_;

    // --- Swarm IDs ---
    int swarm_live_ = 0;
    int swarm_paper_ = 10;

    // --- Parameter-Grenzen (aus config) ---
    int    param1_min_ = 10;
    int    param1_max_ = 30;
    int    param2_min_ = 15;
    int    param2_max_ = 45;
    int    param3_min_ = 24;
    int    param3_max_ = 54;
    double base_lot_ = 0.01;    // Default, falls config nichts liefert

    // Mutations-/Schonfrist-Parameter
    int schonfrist_multiplikator_ = 2;   // aus c_schonfrist_multiplikator
    int schonfrist_bars_ = 0;   // = mutation_zyklus * schonfrist_multiplikator_

    // Anzahl Magics, die pro Mutation betrachtet werden
    int first_n_magics_ = 24;

    // Rundlauf über Blöcke 0–23, 24–47, ... (Block-Index & Block-Anzahl)
    int mutation_block_index_ = 0;   // 0..mutation_num_blocks_-1
    int mutation_num_blocks_ = 9;   // 9 Blöcke à 24 = 216 EAs

    // Interner Mutationszähler in der Tracking-DB
    int        current_mutation_count_ = 0;
    SwarmState current_state_ = SwarmState::NORMAL;

    // Evaluatoren (aktuell Dummy)
    std::unique_ptr<SwarmQualityEvaluator>  swarm_quality_eval_;
    std::unique_ptr<SwarmContextEvaluator>  swarm_context_eval_;

    // Strategien (z.B. DefaultMutationStrategy)
    std::vector<std::unique_ptr<IMutationStrategy>> strategies_;

    // RNG
    std::mt19937_64 rng_;

    // SelectionManager
    std::unique_ptr<SelectionManager> selection_manager_;

    // ========= Private Methoden =========

    // DB-Verwaltung
    bool open_db();
    void close_db();

    // Konfig aus KUBE_config.mqh lesen
    bool read_config();

    // Tracking-Tabelle in KUBE_Mutation.db sicherstellen
    bool ensure_table_mutation();

    // Aktuellen mutation_count aus Tracking-DB lesen
    bool load_mutation_count();

    // Mutations-Tracking nach erfolgreicher Mutation aktualisieren
    bool update_mutation_tracking(const std::string& ts);

    // Führt für den aktuellen State eine Mutation aus (State/Strategy-Pipeline)
    bool perform_mutation_by_state(const std::string& ts);

    // Wählt eine IMutationStrategy passend zu SwarmState
    IMutationStrategy* select_strategy_for_state(SwarmState state);

    // Lädt Live-EAs für den aktuellen Block (24er-Fenster) aus swarms
    bool load_live_ea_params(std::vector<EAParams>& out_eas);

    // Lädt EAs anhand einer Liste von Magic-Numbers
    bool load_ea_params_by_magics(const std::vector<int>& magics, std::vector<EAParams>& out_eas);

    // Wendet einen Satz MutationAction auf Live+Paper an (innerhalb laufender TX)
    bool apply_mutation_actions(const std::vector<MutationAction>& actions);

    // Konsistenzcheck Live/Paper (param1..3, aktiv, lot identisch?)
    bool verify_live_paper_consistency();
};
