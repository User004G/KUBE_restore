// ParallelMutationOrchestrator.cpp
#include "ParallelMutationOrchestrator.h"
#include <chrono>
#include <iostream>

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
ParallelMutationOrchestrator::ParallelMutationOrchestrator(
    const std::string& db_experts_path,
    const std::string& db_schwarm_path,
    const std::string& db_mutation_path,
    const std::string& config_path,
    int swarm_live,
    int swarm_paper)
    : db_experts_path_(db_experts_path),
      db_schwarm_path_(db_schwarm_path),
      db_mutation_path_(db_mutation_path),
      config_path_(config_path),
      swarm_live_(swarm_live),
      swarm_paper_(swarm_paper)
{
    std::printf("[ParallelOrchestrator] Constructed with:\n");
    std::printf("  DB Experts: %s\n", db_experts_path_.c_str());
    std::printf("  DB Schwarm: %s\n", db_schwarm_path_.c_str());
    std::printf("  DB Mutation: %s\n", db_mutation_path_.c_str());
    std::printf("  Config: %s\n", config_path_.c_str());
    std::printf("  Swarm Live: %d, Paper: %d\n", swarm_live_, swarm_paper_);
}

// -------------------------------------------------------
// Destructor
// -------------------------------------------------------
ParallelMutationOrchestrator::~ParallelMutationOrchestrator()
{
    stop();
}

// -------------------------------------------------------
// init_managers
// -------------------------------------------------------
bool ParallelMutationOrchestrator::init_managers()
{
    std::printf("[ParallelOrchestrator] Initializing managers...\n");

    // 1. MutationDataManager
    data_manager_ = std::make_unique<MutationDataManager>(
        db_experts_path_,
        db_mutation_path_
    );
    if (!data_manager_->init()) {
        std::fprintf(stderr, "[ParallelOrchestrator] Failed to init MutationDataManager\n");
        return false;
    }
    std::printf("[ParallelOrchestrator] MutationDataManager initialized\n");

    // 2. SwarmContextEvaluator (Dummy - braucht DB-Handle)
    // Für echte Implementierung: DB öffnen und Handle übergeben
    // Aktuell: Dummy-Konstruktor mit nullptr
    sqlite3* db_schwarm_dummy = nullptr;  // TODO: Echte DB-Verbindung
    context_evaluator_ = std::make_unique<SwarmContextEvaluator>(
        db_schwarm_dummy,
        swarm_live_
    );
    std::printf("[ParallelOrchestrator] SwarmContextEvaluator initialized (dummy)\n");

    // 3. SwarmQualityEvaluator (Dummy)
    quality_evaluator_ = std::make_unique<SwarmQualityEvaluator>(
        db_schwarm_dummy,
        swarm_live_
    );
    std::printf("[ParallelOrchestrator] SwarmQualityEvaluator initialized (dummy)\n");

    // 4. SelectionManager
    selection_manager_ = std::make_unique<SelectionManager>();
    // TODO: init() aufrufen wenn config_path und db_experts_path verfügbar sind
    std::printf("[ParallelOrchestrator] SelectionManager initialized\n");

    return true;
}

// -------------------------------------------------------
// start
// -------------------------------------------------------
bool ParallelMutationOrchestrator::start()
{
    if (running_.load()) {
        std::fprintf(stderr, "[ParallelOrchestrator] Already running!\n");
        return false;
    }

    std::printf("[ParallelOrchestrator] Starting 4 background threads...\n");

    // Manager initialisieren
    if (!init_managers()) {
        std::fprintf(stderr, "[ParallelOrchestrator] Manager initialization failed\n");
        return false;
    }

    // Threads starten
    running_.store(true);

    data_thread_ = std::thread(&ParallelMutationOrchestrator::run_data_manager, this);
    context_thread_ = std::thread(&ParallelMutationOrchestrator::run_context_evaluator, this);
    quality_thread_ = std::thread(&ParallelMutationOrchestrator::run_quality_evaluator, this);
    selection_thread_ = std::thread(&ParallelMutationOrchestrator::run_selection_manager, this);

    std::printf("[ParallelOrchestrator] All threads started successfully\n");
    return true;
}

// -------------------------------------------------------
// stop
// -------------------------------------------------------
void ParallelMutationOrchestrator::stop()
{
    if (!running_.load()) {
        return;  // Bereits gestoppt
    }

    std::printf("[ParallelOrchestrator] Stopping all threads...\n");
    running_.store(false);

    // Auf alle Threads warten
    if (data_thread_.joinable()) {
        data_thread_.join();
        std::printf("[ParallelOrchestrator] Data thread stopped\n");
    }
    if (context_thread_.joinable()) {
        context_thread_.join();
        std::printf("[ParallelOrchestrator] Context thread stopped\n");
    }
    if (quality_thread_.joinable()) {
        quality_thread_.join();
        std::printf("[ParallelOrchestrator] Quality thread stopped\n");
    }
    if (selection_thread_.joinable()) {
        selection_thread_.join();
        std::printf("[ParallelOrchestrator] Selection thread stopped\n");
    }

    std::printf("[ParallelOrchestrator] All threads stopped cleanly\n");
}

// -------------------------------------------------------
// Thread-Worker: MutationDataManager
// -------------------------------------------------------
void ParallelMutationOrchestrator::run_data_manager()
{
    std::printf("[MutationDataManager] Thread started\n");

    while (running_.load())
    {
        // Fitness-Daten laden
        std::vector<EAParams> temp_data;
        if (data_manager_->load_fitness_data(temp_data))
        {
            // Thread-sicher in gemeinsamen Speicher schreiben
            std::lock_guard<std::mutex> lock(data_mutex_);
            latest_fitness_data_ = std::move(temp_data);
            
            std::printf("[MutationDataManager] Loaded %zu fitness records\n", 
                latest_fitness_data_.size());
        }

        // Alle 30 Sekunden aktualisieren
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    std::printf("[MutationDataManager] Thread exiting\n");
}

// -------------------------------------------------------
// Thread-Worker: SwarmContextEvaluator
// -------------------------------------------------------
void ParallelMutationOrchestrator::run_context_evaluator()
{
    std::printf("[SwarmContextEvaluator] Thread started\n");

    while (running_.load())
    {
        // Kontext evaluieren (aktuell Dummy)
        MutationContext ctx;
        ctx.current_timestamp = "dummy_ts";  // TODO: Echten Timestamp holen
        
        // Dummy: enrich_context aufrufen
        context_evaluator_->enrich_context(ctx.current_timestamp, ctx);

        // Thread-sicher speichern
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            current_context_ = ctx;
        }

        // Alle 60 Sekunden aktualisieren
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    std::printf("[SwarmContextEvaluator] Thread exiting\n");
}

// -------------------------------------------------------
// Thread-Worker: SwarmQualityEvaluator
// -------------------------------------------------------
void ParallelMutationOrchestrator::run_quality_evaluator()
{
    std::printf("[SwarmQualityEvaluator] Thread started\n");

    while (running_.load())
    {
        // Swarm-State evaluieren (aktuell Dummy: immer NORMAL)
        std::string ts = "dummy_ts";  // TODO: Echten Timestamp
        SwarmState state = quality_evaluator_->evaluate_state(ts);

        // Thread-sicher speichern
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            current_swarm_state_ = state;
        }

        std::printf("[SwarmQualityEvaluator] State: %d\n", (int)state);

        // Alle 60 Sekunden aktualisieren
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    std::printf("[SwarmQualityEvaluator] Thread exiting\n");
}

// -------------------------------------------------------
// Thread-Worker: SelectionManager
// -------------------------------------------------------
void ParallelMutationOrchestrator::run_selection_manager()
{
    std::printf("[SelectionManager] Thread started\n");

    while (running_.load())
    {
        // Fitness-Daten holen
        std::vector<EAParams> fitness_data;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            fitness_data = latest_fitness_data_;
        }

        if (!fitness_data.empty())
        {
            std::vector<EAParams> best, worst;
            if (selection_manager_->select_best_and_worst(fitness_data, best, worst))
            {
                // Thread-sicher speichern
                std::lock_guard<std::mutex> lock(data_mutex_);
                best_eas_ = std::move(best);
                worst_eas_ = std::move(worst);

                std::printf("[SelectionManager] Selected %zu best, %zu worst EAs\n",
                    best_eas_.size(), worst_eas_.size());
            }
        }

        // Alle 30 Sekunden aktualisieren
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    std::printf("[SelectionManager] Thread exiting\n");
}

// -------------------------------------------------------
// Thread-sichere Getter
// -------------------------------------------------------
bool ParallelMutationOrchestrator::get_latest_fitness_data(std::vector<EAParams>& out_data)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    out_data = latest_fitness_data_;
    return !out_data.empty();
}

SwarmState ParallelMutationOrchestrator::get_current_swarm_state()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_swarm_state_;
}

MutationContext ParallelMutationOrchestrator::get_current_context()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_context_;
}

bool ParallelMutationOrchestrator::get_best_worst_eas(
    std::vector<EAParams>& best_eas, 
    std::vector<EAParams>& worst_eas)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    best_eas = best_eas_;
    worst_eas = worst_eas_;
    return !best_eas.empty() && !worst_eas.empty();
}
