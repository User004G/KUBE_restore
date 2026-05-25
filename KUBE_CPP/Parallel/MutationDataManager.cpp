// MutationDataManager.cpp
#include "MutationDataManager.h"
#include <cstdio>

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
MutationDataManager::MutationDataManager(
    const std::string& db_experts_path,
    const std::string& db_mutation_path)
    : db_experts_(nullptr),
      db_mutation_(nullptr),
      db_experts_path_(db_experts_path),
      db_mutation_path_(db_mutation_path)
{
    std::printf("[MutationDataManager] Constructed with:\n");
    std::printf("  DB Experts: %s\n", db_experts_path_.c_str());
    std::printf("  DB Mutation: %s\n", db_mutation_path_.c_str());
}

// -------------------------------------------------------
// Destructor
// -------------------------------------------------------
MutationDataManager::~MutationDataManager()
{
    close_db();
}

// -------------------------------------------------------
// open_db
// -------------------------------------------------------
bool MutationDataManager::open_db()
{
    if (db_experts_ && db_mutation_)
        return true;

    // KUBE_Experts.db öffnen
    if (!db_experts_)
    {
        int rc = sqlite3_open(db_experts_path_.c_str(), &db_experts_);
        if (rc != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationDataManager] Failed to open KUBE_Experts.db: %s\n",
                sqlite3_errmsg(db_experts_));
            return false;
        }
        sqlite3_exec(db_experts_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_experts_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_experts_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);
    }

    // KUBE_Mutation.db öffnen
    if (!db_mutation_)
    {
        int rc = sqlite3_open(db_mutation_path_.c_str(), &db_mutation_);
        if (rc != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationDataManager] Failed to open KUBE_Mutation.db: %s\n",
                sqlite3_errmsg(db_mutation_));
            return false;
        }
        sqlite3_exec(db_mutation_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_mutation_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_mutation_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);
    }

    std::printf("[MutationDataManager] Databases opened successfully\n");
    return true;
}

// -------------------------------------------------------
// close_db
// -------------------------------------------------------
void MutationDataManager::close_db()
{
    if (db_experts_)
    {
        sqlite3_close(db_experts_);
        db_experts_ = nullptr;
    }
    if (db_mutation_)
    {
        sqlite3_close(db_mutation_);
        db_mutation_ = nullptr;
    }
}

// -------------------------------------------------------
// init
// -------------------------------------------------------
bool MutationDataManager::init()
{
    std::printf("[MutationDataManager] Initializing...\n");

    if (!open_db())
    {
        std::fprintf(stderr, "[MutationDataManager] Failed to open databases\n");
        return false;
    }

    std::printf("[MutationDataManager] Initialized successfully\n");
    return true;
}

// -------------------------------------------------------
// load_fitness_data
// -------------------------------------------------------
bool MutationDataManager::load_fitness_data(std::vector<EAParams>& out_data)
{
    out_data.clear();

    if (!db_experts_)
    {
        std::fprintf(stderr, "[MutationDataManager] DB not open\n");
        return false;
    }

    // TODO: Echte Implementierung - Fitness-Daten aus Fitness_proBar laden
    // Aktuell: Dummy-Implementierung
    std::printf("[MutationDataManager] load_fitness_data() - STUB (not yet implemented)\n");

    // Beispiel: Dummy-Daten zurückgeben
    // In echter Implementierung: SELECT aus Fitness_proBar-Tabelle
    
    return true;  // Erfolgreich, aber keine Daten (Stub)
}
