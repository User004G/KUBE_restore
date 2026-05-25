#include "SelectionManager.h"
#include "../ToolBox.h"
#include "../MutationsManager.h"  // For EAParams definition
#include "sqlite3.h"
#include <iostream>
#include <algorithm>
#include <cstdio>

SelectionManager::SelectionManager()
    : db_experts_(nullptr)
{
}

SelectionManager::~SelectionManager()
{
    close_db();
}

bool SelectionManager::init(const std::string& config_path, const std::string& db_experts_path)
{
    config_path_ = config_path;
    db_experts_path_ = db_experts_path;

    // Config lesen
    try {
        mutation_lookback_ = ToolBox::read_int_value(config_path_, "c_mutation_lookback");
    }
    catch (...) {
        std::cerr << "[SelectionManager] Error reading c_mutation_lookback from " << config_path_ << "\n";
        return false;
    }

    if (!open_db()) {
        return false;
    }

    std::printf("[SelectionManager] Initialized. Lookback=%d, DB=%s\n", 
        mutation_lookback_, db_experts_path_.c_str());
    return true;
}

bool SelectionManager::open_db()
{
    if (db_experts_) return true;

    int rc = sqlite3_open(db_experts_path_.c_str(), &db_experts_);
    if (rc != SQLITE_OK) {
        std::cerr << "[SelectionManager] Failed to open DB: " << sqlite3_errmsg(db_experts_) << "\n";
        return false;
    }
    
    // Read-Only Optimierungen
    sqlite3_exec(db_experts_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_experts_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    
    return true;
}

void SelectionManager::close_db()
{
    if (db_experts_) {
        sqlite3_close(db_experts_);
        db_experts_ = nullptr;
    }
}

std::vector<int> SelectionManager::select_worst_eas(int count)
{
    std::vector<int> result;
    if (!db_experts_) return result;

    // 1. Neuesten Timestamp finden
    std::string sql_ts = "SELECT MAX(ts_key) FROM Fitness_proBar;";
    sqlite3_stmt* stmt = nullptr;
    std::string latest_ts;

    if (sqlite3_prepare_v2(db_experts_, sql_ts.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(stmt, 0);
            if (txt) latest_ts = reinterpret_cast<const char*>(txt);
        }
    }
    sqlite3_finalize(stmt);

    if (latest_ts.empty()) {
        std::cerr << "[SelectionManager] No timestamp found in Fitness_proBar.\n";
        return result;
    }

    // 2. EAs für diesen Timestamp laden und sortieren
    // Wir sortieren aufsteigend nach NetProfitNorm (schlechteste zuerst).
    std::string sql_query = 
        "SELECT ea_magic FROM Fitness_proBar "
        "WHERE ts_key = ? "
        "ORDER BY NetProfitNorm ASC " // Schlechteste zuerst (kleinster Profit)
        "LIMIT ?;";

    if (sqlite3_prepare_v2(db_experts_, sql_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[SelectionManager] SQL Error: " << sqlite3_errmsg(db_experts_) << "\n";
        return result;
    }

    sqlite3_bind_text(stmt, 1, latest_ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, count);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    std::printf("[SelectionManager] Selected %zu worst EAs from ts=%s\n", result.size(), latest_ts.c_str());
    return result;
}

bool SelectionManager::select_best_and_worst(
    const std::vector<EAParams>& fitness_data,
    std::vector<EAParams>& best_eas,
    std::vector<EAParams>& worst_eas)
{
    best_eas.clear();
    worst_eas.clear();

    if (fitness_data.empty())
    {
        std::fprintf(stderr, "[SelectionManager] No fitness data provided\n");
        return false;
    }

    // Für jetzt: best_eas bleibt leer (wird später für Mutation als Vorbild genutzt)
    // worst_eas wird mit den schlechtesten EAs gefüllt
    
    // Kopiere die Daten und sortiere nach performance_metric
    std::vector<EAParams> sorted_data = fitness_data;
    
    // Sortiere aufsteigend nach performance_metric (schlechteste zuerst)
    std::sort(sorted_data.begin(), sorted_data.end(),
        [](const EAParams& a, const EAParams& b) {
            return a.performance_metric < b.performance_metric;
        });

    // Nimm die schlechtesten N (vom Anfang, da aufsteigend sortiert)
    int n_worst = (24 < (int)sorted_data.size()) ? 24 : (int)sorted_data.size();
    for (int i = 0; i < n_worst; ++i)
    {
        worst_eas.push_back(sorted_data[i]);
    }

    std::printf("[SelectionManager] Selected %zu best, %zu worst EAs\n",
        best_eas.size(), worst_eas.size());

    return true;
}

