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
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

// WICHTIG: MutationsManager.h zuerst - definiert EAParams, SwarmState, MutationContext
#include "../MutationsManager.h"

// Dann die Manager-Klassen, die diese Typen verwenden
#include "MutationDataManager.h"
#include "SwarmContextEvaluator.h"
#include "SwarmQualityEvaluator.h"
#include "SelectionManager.h"

/**
 * ParallelMutationOrchestrator
 * 
 * Orchestriert vier Manager-Klassen in parallelen Hintergrund-Threads:
 * 1. MutationDataManager    - Lädt kontinuierlich Fitness-Daten
 * 2. SwarmContextEvaluator  - Analysiert Marktkontext
 * 3. SwarmQualityEvaluator  - Bewertet Swarm-Qualität
 * 4. SelectionManager       - Bereitet Best/Worst-Listen vor
 * 
 * Diese Threads laufen unabhängig vom Hauptsystem (Synchronisator)
 * und bereiten Daten für den MutationsManager vor.
 */
class ParallelMutationOrchestrator
{
public:
    /**
     * Constructor
     * @param db_experts_path   Path zu KUBE_Experts.db
     * @param db_schwarm_path   Path zu KUBE_Schwarm.db
     * @param db_mutation_path  Path zu KUBE_Mutation.db
     * @param config_path       Path zu KUBE_config.mqh
     * @param swarm_live        Live-Swarm-ID (z.B. 0)
     * @param swarm_paper       Paper-Swarm-ID (z.B. 10)
     */
    ParallelMutationOrchestrator(
        const std::string& db_experts_path,
        const std::string& db_schwarm_path,
        const std::string& db_mutation_path,
        const std::string& config_path,
        int swarm_live = 0,
        int swarm_paper = 10
    );

    ~ParallelMutationOrchestrator();

    /**
     * Startet alle 4 Hintergrund-Threads
     * @return true, wenn erfolgreich gestartet
     */
    bool start();

    /**
     * Stoppt alle Threads koordiniert und wartet auf sauberes Beenden
     */
    void stop();

    /**
     * Prüft, ob die Threads laufen
     */
    bool is_running() const { return running_.load(); }

    // ========== Thread-sichere Daten-Getter ==========
    // Diese Methoden können vom MutationsManager aufgerufen werden

    /**
     * Gibt die zuletzt geladenen Fitness-Daten zurück
     * @param out_data Container für die Daten
     * @return true, wenn Daten verfügbar
     */
    bool get_latest_fitness_data(std::vector<EAParams>& out_data);

    /**
     * Gibt den aktuellen Swarm-State zurück (NORMAL/SUCCESS/STRESS)
     */
    SwarmState get_current_swarm_state();

    /**
     * Gibt den aktuellen Mutations-Kontext zurück
     */
    MutationContext get_current_context();

    /**
     * Gibt Best/Worst EA-Listen zurück
     */
    bool get_best_worst_eas(std::vector<EAParams>& best_eas, std::vector<EAParams>& worst_eas);

private:
    // ========== Konfiguration ==========
    std::string db_experts_path_;
    std::string db_schwarm_path_;
    std::string db_mutation_path_;
    std::string config_path_;
    int swarm_live_;
    int swarm_paper_;

    // ========== Manager-Instanzen ==========
    std::unique_ptr<MutationDataManager>   data_manager_;
    std::unique_ptr<SwarmContextEvaluator> context_evaluator_;
    std::unique_ptr<SwarmQualityEvaluator> quality_evaluator_;
    std::unique_ptr<SelectionManager>      selection_manager_;

    // ========== Thread-Verwaltung ==========
    std::thread data_thread_;
    std::thread context_thread_;
    std::thread quality_thread_;
    std::thread selection_thread_;

    std::atomic<bool> running_{false};

    // ========== Gemeinsame Daten (Thread-sicher) ==========
    std::mutex data_mutex_;
    std::vector<EAParams> latest_fitness_data_;
    SwarmState current_swarm_state_ = SwarmState::NORMAL;
    MutationContext current_context_;
    std::vector<EAParams> best_eas_;
    std::vector<EAParams> worst_eas_;

    // ========== Thread-Worker-Funktionen ==========
    void run_data_manager();
    void run_context_evaluator();
    void run_quality_evaluator();
    void run_selection_manager();

    // ========== Hilfsfunktionen ==========
    bool init_managers();
};
