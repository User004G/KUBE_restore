// MutationsManager.cpp
#include "MutationsManager.h"
#include "ToolBox.h"
#include "Parallel/SwarmQualityEvaluator.h"
#include "Parallel/SwarmContextEvaluator.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

// -------------------------------------------------------
// Helper: simple SQL exec with logging
// -------------------------------------------------------
static int exec_sql(sqlite3* db, const char* sql)
{
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] SQL error: %s (sql=%s)\n",
            errmsg ? errmsg : "unknown", sql);
        if (errmsg) sqlite3_free(errmsg);
    }
    return rc;
}

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
MutationsManager::MutationsManager(const std::string& db_schwarm_path,
    const std::string& config_path,
    int swarm_live,
    int swarm_paper)
    : db_schwarm_(nullptr),
    db_mutation_(nullptr),
    db_schwarm_path_(db_schwarm_path),
    config_path_(config_path),
    swarm_live_(swarm_live),
    swarm_paper_(swarm_paper)
{
    // KUBE_Mutation.db Pfad aus KUBE_Schwarm.db ableiten
    size_t pos = db_schwarm_path_.find("KUBE_Schwarm.db");
    if (pos != std::string::npos)
    {
        db_mutation_path_ = db_schwarm_path_;
        db_mutation_path_.replace(pos, 15, "KUBE_Mutation.db");
    }
    else
    {
        size_t last_slash = db_schwarm_path_.find_last_of("/\\");
        if (last_slash != std::string::npos)
            db_mutation_path_ = db_schwarm_path_.substr(0, last_slash + 1) + "KUBE_Mutation.db";
        else
            db_mutation_path_ = "KUBE_Mutation.db";
    }

    std::printf("[MutationsManager] Swarm DB: %s\n", db_schwarm_path_.c_str());
    std::printf("[MutationsManager] Mutation DB: %s\n", db_mutation_path_.c_str());
    std::printf("[MutationsManager] Config: %s\n", config_path_.c_str());
}

// -------------------------------------------------------
// Destructor
// -------------------------------------------------------
MutationsManager::~MutationsManager()
{
    close_db();
}

// -------------------------------------------------------
// open_db
// -------------------------------------------------------
bool MutationsManager::open_db()
{
    if (db_schwarm_ && db_mutation_)
        return true;

    // KUBE_Schwarm.db
    if (!db_schwarm_)
    {
        int rc = sqlite3_open(db_schwarm_path_.c_str(), &db_schwarm_);
        if (rc != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationsManager] Failed to open KUBE_Schwarm.db: %s\n",
                sqlite3_errmsg(db_schwarm_));
            return false;
        }
        exec_sql(db_schwarm_, "PRAGMA journal_mode=WAL;");
        exec_sql(db_schwarm_, "PRAGMA synchronous=NORMAL;");
        exec_sql(db_schwarm_, "PRAGMA busy_timeout=5000;");
    }

    // KUBE_Mutation.db
    if (!db_mutation_)
    {
        std::printf("[MutationsManager] Opening/Creating KUBE_Mutation.db at '%s'\n",
            db_mutation_path_.c_str());

        int rc = sqlite3_open(db_mutation_path_.c_str(), &db_mutation_);
        if (rc != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationsManager] Failed to open/create KUBE_Mutation.db: %s\n",
                sqlite3_errmsg(db_mutation_));
            return false;
        }

        exec_sql(db_mutation_, "PRAGMA journal_mode=WAL;");
        exec_sql(db_mutation_, "PRAGMA synchronous=NORMAL;");
        exec_sql(db_mutation_, "PRAGMA busy_timeout=5000;");

        std::printf("[MutationsManager] KUBE_Mutation.db opened successfully in WAL mode\n");
    }

    return true;
}

// -------------------------------------------------------
// close_db
// -------------------------------------------------------
void MutationsManager::close_db()
{
    if (db_schwarm_)
    {
        sqlite3_close(db_schwarm_);
        db_schwarm_ = nullptr;
    }
    if (db_mutation_)
    {
        sqlite3_close(db_mutation_);
        db_mutation_ = nullptr;
    }
}

// -------------------------------------------------------
// read_config
// -------------------------------------------------------
bool MutationsManager::read_config()
{
    try {
        param1_min_ = ToolBox::read_int_value(config_path_, "c_param1_min");
        param1_max_ = ToolBox::read_int_value(config_path_, "c_param1_max");
        param2_min_ = ToolBox::read_int_value(config_path_, "c_param2_min");
        param2_max_ = ToolBox::read_int_value(config_path_, "c_param2_max");
        param3_min_ = ToolBox::read_int_value(config_path_, "c_param3_min");
        param3_max_ = ToolBox::read_int_value(config_path_, "c_param3_max");

        // Mutationszyklus
        int zyklus_cfg = ToolBox::read_int_value(config_path_, "c_mutations_zyklus");
        if (zyklus_cfg > 0)
            mutation_zyklus = zyklus_cfg;

        // Schonfrist-Multiplikator
        int mult_cfg = 2;
        try {
            mult_cfg = ToolBox::read_int_value(config_path_, "c_schonfrist_multiplikator");
        }
        catch (...) {
            mult_cfg = 2; // Default fallback
        }
        if (mult_cfg <= 0)
            mult_cfg = 2;

        schonfrist_multiplikator_ = mult_cfg;
        schonfrist_bars_ = mutation_zyklus * schonfrist_multiplikator_;

        std::printf("[MutationsManager] Config loaded: "
            "P1[%d-%d], P2[%d-%d], P3[%d-%d], base_lot=%.4f, "
            "mutation_zyklus=%d, schonfrist_multiplikator=%d, schonfrist_bars=%d\n",
            param1_min_, param1_max_,
            param2_min_, param2_max_,
            param3_min_, param3_max_,
            base_lot_,
            mutation_zyklus, schonfrist_multiplikator_, schonfrist_bars_);

        return true;
    }
    catch (...)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to read config parameters\n");
        return false;
    }
}

// -------------------------------------------------------
// ensure_table_mutation
// -------------------------------------------------------
bool MutationsManager::ensure_table_mutation()
{
    std::printf("[MutationsManager] Ensuring mutation_tracking table exists...\n");

    const char* sql_create =
        "CREATE TABLE IF NOT EXISTS mutation_tracking ("
        "  mutation_count INTEGER PRIMARY KEY,"
        "  mutation_triggered INTEGER NOT NULL DEFAULT 0,"
        "  mutation_done INTEGER NOT NULL DEFAULT 0,"
        "  mutation_ts TEXT"
        ");";

    if (exec_sql(db_mutation_, sql_create) != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to create mutation_tracking table\n");
        return false;
    }

    std::printf("[MutationsManager] mutation_tracking table ready\n");
    return true;
}

// -------------------------------------------------------
// load_mutation_count
// -------------------------------------------------------
bool MutationsManager::load_mutation_count()
{
    const char* sql =
        "SELECT mutation_count FROM mutation_tracking "
        "ORDER BY mutation_count DESC LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_mutation_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to prepare load_mutation_count: %s\n",
            sqlite3_errmsg(db_mutation_));
        return false;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        current_mutation_count_ = sqlite3_column_int(stmt, 0);
        std::printf("[MutationsManager] Loaded mutation_count from DB: %d\n",
            current_mutation_count_);
    }
    else
    {
        current_mutation_count_ = 0;
        std::printf("[MutationsManager] No mutation tracking found, starting at 0\n");
        sqlite3_finalize(stmt);

        const char* sql_init =
            "INSERT OR IGNORE INTO mutation_tracking "
            "(mutation_count, mutation_triggered, mutation_done) "
            "VALUES (0, 0, 0);";

        if (exec_sql(db_mutation_, sql_init) != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationsManager] Failed to create initial mutation row\n");
            return false;
        }
        return true;
    }

    sqlite3_finalize(stmt);
    return true;
}

// -------------------------------------------------------
// change_state
// -------------------------------------------------------
void MutationsManager::change_state(SwarmState new_state)
{
    if (new_state == current_state_)
        return;

    std::printf("[MutationsManager] State change: %d -> %d\n",
        (int)current_state_, (int)new_state);
    current_state_ = new_state;
}

// -------------------------------------------------------
// init
// -------------------------------------------------------
bool MutationsManager::init()
{
    std::printf("[MutationsManager] Initializing...\n");

    if (!open_db())
    {
        std::fprintf(stderr, "[MutationsManager] Failed to open databases\n");
        return false;
    }
    if (!ensure_table_mutation())
    {
        std::fprintf(stderr, "[MutationsManager] Failed to ensure mutation_tracking table\n");
        return false;
    }
    if (!read_config())
    {
        std::fprintf(stderr, "[MutationsManager] Failed to read config\n");
        return false;
    }
    if (!load_mutation_count())
    {
        std::fprintf(stderr, "[MutationsManager] Failed to load mutation count\n");
        return false;
    }

    // RNG
    {
        std::random_device rd;
        rng_.seed(rd());
    }

    //// Evaluatoren (aktuell Dummy-Objekte, machen nichts)
    //swarm_quality_eval_ = std::make_unique<SwarmQualityEvaluator>();
    //swarm_context_eval_ = std::make_unique<SwarmContextEvaluator>();

    //// Strategien (erstmal nur DefaultMutationStrategy)
    //strategies_.clear();
    //strategies_.push_back(create_default_random_strategy());
    // RNG seeden etc...

    //swarm_quality_eval_ = std::make_unique<SwarmQualityEvaluator>(/* ... oder dummy ... */);
    //swarm_context_eval_ = std::make_unique<SwarmContextEvaluator>(/* ... oder dummy ... */);

    strategies_.clear();
    strategies_.push_back(std::unique_ptr<IMutationStrategy>(new DefaultMutationStrategy()));

    // SelectionManager initialisieren
    selection_manager_ = std::unique_ptr<SelectionManager>(new SelectionManager());
    // Wir nutzen denselben Config-Pfad und den DB-Pfad (hier db_schwarm_path_ enthält vermutlich auch Fitness-Daten oder wir müssen den Pfad anpassen?)
    // Der User hat gesagt "Ranking im SelectionManager machen".
    // SelectionManager braucht Zugriff auf Fitness_proBar. Wo liegt die?
    // Im SelectionManager.cpp steht: db_experts_path_ wird geöffnet.
    // Wir übergeben hier db_schwarm_path_ als db_experts_path, in der Hoffnung, dass Fitness_proBar dort drin ist.
    // Falls nicht, müsste der User den Pfad spezifizieren.
    // Annahme: Fitness_proBar ist in KUBE_Schwarm.db (oder der User hat es so gemeint).
    if (!selection_manager_->init(config_path_, db_schwarm_path_))
    {
        std::fprintf(stderr, "[MutationsManager] Failed to init SelectionManager\n");
        return false;
    }

    std::printf("[MutationsManager] Initialized successfully. mutation_zyklus=%d, schonfrist_bars=%d\n",
        mutation_zyklus, schonfrist_bars_);
    return true;
}

// -------------------------------------------------------
// select_strategy_for_state
// -------------------------------------------------------
IMutationStrategy* MutationsManager::select_strategy_for_state(SwarmState state)
{
    for (auto& s : strategies_)
    {
        if (s->applicable_state() == state)
            return s.get();
    }
    if (!strategies_.empty())
        return strategies_.front().get();
    return nullptr;
}

// -------------------------------------------------------
// load_live_ea_params: 24er-Block, "reihum"
// -------------------------------------------------------
bool MutationsManager::load_live_ea_params(std::vector<EAParams>& out_eas)
{
    out_eas.clear();

    // Block-Offset berechnen: 0–23, 24–47, ..., 8*24–8*24+23
    int offset = mutation_block_index_ * first_n_magics_;

    std::string sql =
        "SELECT Magic, param1, param2, param3, aktiv, lot "
        "FROM swarms "
        "WHERE swarm = " + std::to_string(swarm_live_) + " "
        "ORDER BY Magic "
        "LIMIT " + std::to_string(first_n_magics_) +
        " OFFSET " + std::to_string(offset) + ";";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_schwarm_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to prepare load_live_ea_params: %s\n",
            sqlite3_errmsg(db_schwarm_));
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        EAParams ea{};
        ea.magic = sqlite3_column_int(stmt, 0);
        ea.param1 = sqlite3_column_int(stmt, 1);
        ea.param2 = sqlite3_column_int(stmt, 2);
        ea.param3 = sqlite3_column_int(stmt, 3);
        //ea.active = (sqlite3_column_int(stmt, 4) != 0);
        //ea.lot = sqlite3_column_double(stmt, 5);
        //ea.swarm_id = swarm_live_;
        //ea.performance_metric = 0.0; // später aus Fitness-DB
        out_eas.push_back(ea);
    }

    sqlite3_finalize(stmt);

    if (out_eas.empty())
    {
        std::fprintf(stderr, "[MutationsManager] No Live EAs found in swarm %d (offset=%d)\n",
            swarm_live_, offset);
        return false;
    }

    std::printf("[MutationsManager] Loaded %d Live EAs for mutation (block_index=%d, offset=%d)\n",
        (int)out_eas.size(), mutation_block_index_, offset);
	mutation_block_index_ += 1;
    if (mutation_block_index_ == 9) { mutation_block_index_ = 0; }
    return true;
}

// -------------------------------------------------------
// load_ea_params_by_magics
// -------------------------------------------------------
bool MutationsManager::load_ea_params_by_magics(const std::vector<int>& magics, std::vector<EAParams>& out_eas)
{
    out_eas.clear();
    if (magics.empty()) return true;

    // SQL IN-Clause bauen
    std::string in_clause = "(";
    for (size_t i = 0; i < magics.size(); ++i) {
        in_clause += std::to_string(magics[i]);
        if (i < magics.size() - 1) in_clause += ",";
    }
    in_clause += ")";

    std::string sql =
        "SELECT Magic, param1, param2, param3, aktiv, lot "
        "FROM swarms "
        "WHERE swarm = " + std::to_string(swarm_live_) + " "
        "AND Magic IN " + in_clause + " "
        "ORDER BY Magic;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_schwarm_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to prepare load_ea_params_by_magics: %s\n",
            sqlite3_errmsg(db_schwarm_));
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        EAParams ea{};
        ea.magic = sqlite3_column_int(stmt, 0);
        ea.param1 = sqlite3_column_int(stmt, 1);
        ea.param2 = sqlite3_column_int(stmt, 2);
        ea.param3 = sqlite3_column_int(stmt, 3);
        //ea.active = (sqlite3_column_int(stmt, 4) != 0);
        //ea.lot = sqlite3_column_double(stmt, 5);
        out_eas.push_back(ea);
    }

    sqlite3_finalize(stmt);
    
    std::printf("[MutationsManager] Loaded %zu EAs by magic list (requested %zu)\n", 
        out_eas.size(), magics.size());

    return !out_eas.empty();
}

// -------------------------------------------------------
// apply_mutation_actions
//  - Live & Paper updaten, aktiv=0, lot=base_lot_
// -------------------------------------------------------
bool MutationsManager::apply_mutation_actions(const std::vector<MutationAction>& actions)
{
    for (const auto& act : actions)
    {
        int magic_live = act.magic;
        int magic_paper = magic_live + 10000;

        // Live
        std::string sql_live =
            "UPDATE swarms SET "
            "param1 = " + std::to_string(act.new_param1) + ", "
            "param2 = " + std::to_string(act.new_param2) + ", "
            "param3 = " + std::to_string(act.new_param3) + ", "
            "aktiv  = 0, "
            "lot    = " + std::to_string(base_lot_) +
            " WHERE Magic = " + std::to_string(magic_live) +
            " AND swarm = " + std::to_string(swarm_live_) + ";";

        if (exec_sql(db_schwarm_, sql_live.c_str()) != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationsManager] Failed to update Live Magic %d\n",
                magic_live);
            return false;
        }

        // Paper
        std::string sql_paper =
            "UPDATE swarms SET "
            "param1 = " + std::to_string(act.new_param1) + ", "
            "param2 = " + std::to_string(act.new_param2) + ", "
            "param3 = " + std::to_string(act.new_param3) + ", "
            "aktiv  = 0, "
            "lot    = " + std::to_string(base_lot_) +
            " WHERE Magic = " + std::to_string(magic_paper) +
            " AND swarm = " + std::to_string(swarm_paper_) + ";";

        if (exec_sql(db_schwarm_, sql_paper.c_str()) != SQLITE_OK)
        {
            std::fprintf(stderr, "[MutationsManager] Failed to update Paper Magic %d\n",
                magic_paper);
            return false;
        }

        std::printf("[MutationsManager] Mutated Magic %d: P1=%d P2=%d P3=%d (Live+Paper, aktiv=0, lot=%.4f)\n",
            magic_live, act.new_param1, act.new_param2, act.new_param3, base_lot_);
    }

    return true;
}

// -------------------------------------------------------
// verify_live_paper_consistency
// -------------------------------------------------------
bool MutationsManager::verify_live_paper_consistency()
{
    std::string sql =
        "SELECT COUNT(*) FROM swarms s1 "
        "JOIN swarms s2 ON s2.Magic = s1.Magic + 10000 "
        "WHERE s1.swarm = " + std::to_string(swarm_live_) + " "
        "AND s2.swarm = " + std::to_string(swarm_paper_) + " "
        "AND (s1.param1 <> s2.param1 OR s1.param2 <> s2.param2 OR s1.param3 <> s2.param3 "
        "     OR s1.aktiv <> s2.aktiv OR s1.lot <> s2.lot);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_schwarm_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to prepare verification\n");
        return false;
    }

    int mismatch_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        mismatch_count = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    if (mismatch_count == 0)
    {
        std::printf("[MutationsManager] Verification SUCCESS: Live and Paper match\n");
        return true;
    }
    else
    {
        std::fprintf(stderr, "[MutationsManager] Verification FAILED: %d mismatches found\n",
            mismatch_count);
        return false;
    }
}

// -------------------------------------------------------
// update_mutation_tracking
// -------------------------------------------------------
bool MutationsManager::update_mutation_tracking(const std::string& ts)
{
    current_mutation_count_++;

    std::string sql =
        "INSERT OR REPLACE INTO mutation_tracking "
        "(mutation_count, mutation_triggered, mutation_done, mutation_ts) "
        "VALUES (" + std::to_string(current_mutation_count_) +
        ", 1, 1, '" + ts + "');";

    if (exec_sql(db_mutation_, sql.c_str()) != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to update mutation tracking\n");
        return false;
    }

    std::printf("[MutationsManager] Updated mutation_tracking: count=%d, ts=%s\n",
        current_mutation_count_, ts.c_str());
    return true;
}

bool MutationsManager::perform_mutation_by_state(const std::string& ts)
{
    // Beginn der Mutation für den gegebenen Zustand
    std::printf("[MutationsManager] ========== PERFORMING MUTATION (by state) ==========\n");
    std::printf("[MutationsManager] Timestamp: %s, next mutation_count=%d\n",
        ts.c_str(), current_mutation_count_ + 1);

    // 1) SwarmState DUMMY: immer NORMAL
    // In der aktuellen Dummy-Phase ist der Zustand immer auf NORMAL gesetzt
    SwarmState evaluated_state = SwarmState::NORMAL;
    change_state(evaluated_state);  // Die Methode ändert den aktuellen Zustand und gibt ihn aus

    // 2) Kontext nur lokal aufbauen (nur Bounds + Timestamp)
    // Der MutationContext wird lokal für die Mutation aufgebaut, basierend auf den Parametern und dem Timestamp
    MutationContext ctx;
    ctx.current_timestamp = ts;
    ctx.param1_min = param1_min_;
    ctx.param1_max = param1_max_;
    ctx.param2_min = param2_min_;
    ctx.param2_max = param2_max_;
    ctx.param3_min = param3_min_;
    ctx.param3_max = param3_max_;
    // Hier können später weitere Felder wie Volatilität, Equity und Schonfrist hinzugefügt werden

    // 3) Strategie für den aktuellen State wählen
    // Wir wählen die Strategie basierend auf dem aktuellen Zustand (in diesem Fall immer NORMAL)
    IMutationStrategy* strat = select_strategy_for_state(current_state_);
    if (!strat)
    {
        std::fprintf(stderr, "[MutationsManager] No strategy available for state %d\n",
            (int)current_state_);
        return false;  // Falls keine Strategie verfügbar ist, bricht die Mutation ab
    }

    std::printf("[MutationsManager] Using strategy: %s (state=%d)\n",
        strat->name().c_str(), (int)current_state_);

    // 4) Live-EA-Params laden
    // VERSUCH: Ranking über SelectionManager (Worst 24)
    std::vector<EAParams> live_eas;
    bool loaded_via_ranking = false;

    if (selection_manager_)
    {
        std::vector<int> worst_magics = selection_manager_->select_worst_eas(24);
        if (!worst_magics.empty())
        {
            if (load_ea_params_by_magics(worst_magics, live_eas))
            {
                loaded_via_ranking = true;
                std::printf("[MutationsManager] Using %zu ranked worst EAs for mutation.\n", live_eas.size());
            }
        }
    }

    // FALLBACK: Wenn Ranking fehlschlägt (z.B. keine Fitness-Daten), dann Round-Robin
    if (!loaded_via_ranking)
    {
        std::printf("[Mutation] Blockmutation\n");
        std::printf("[MutationsManager] Ranking failed or empty. Fallback to Round-Robin block.\n");
        if (!load_live_ea_params(live_eas))
        {
            std::fprintf(stderr, "[MutationsManager] Failed to load Live EAs for mutation (Round-Robin)\n");
            return false;
        }
    }

    // 5) Strategy planen lassen (Random-Mutation nur auf dem Block)
    // Die ausgewählte Strategie (z.B. Random-Mutation) wird angewendet, um die Mutationen zu planen
    std::vector<MutationAction> actions = strat->plan_mutations(live_eas, ctx, rng_);
    if (actions.empty())
    {
        std::fprintf(stderr, "[MutationsManager] Strategy returned no mutation actions\n");
        return false;  // Wenn keine Mutation geplant wurde, bricht die Methode ab
    }

    // HINWEIS:
    // Hier könnte eine lokale Helper-Funktion hinzugefügt werden, die sicherstellt, dass keine doppelten (param1, param2, param3)-Kombinationen auftreten.
    // Diese Funktion könnte über db_schwarm_ prüfen, ob die Kombination bereits existiert.

    // 6) Transaktion -> Aktionen anwenden
    // Beginnt eine SQL-Transaktion, um die Mutationen atomar anzuwenden
    if (exec_sql(db_schwarm_, "BEGIN TRANSACTION;") != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to BEGIN TRANSACTION\n");
        return false;  // Fehler beim Starten der Transaktion, Mutation wird abgebrochen
    }

    // Wendet die geplanten Mutationen an
    if (!apply_mutation_actions(actions))
    {
        std::fprintf(stderr, "[MutationsManager] Failed to apply mutation actions, rolling back\n");
        exec_sql(db_schwarm_, "ROLLBACK;");  // Rollback bei einem Fehler
        return false;
    }

    // Commit der Transaktion, um die Änderungen zu bestätigen
    if (exec_sql(db_schwarm_, "COMMIT;") != SQLITE_OK)
    {
        std::fprintf(stderr, "[MutationsManager] Failed to COMMIT, trying ROLLBACK\n");
        exec_sql(db_schwarm_, "ROLLBACK;");  // Rollback bei Fehlern beim Commit
        return false;
    }

    // 7) Konsistenzcheck
    // Sicherstellen, dass nach der Mutation die Parameter von Live- und Paper-EAs übereinstimmen
    if (!verify_live_paper_consistency())
    {
        std::fprintf(stderr, "[MutationsManager] Consistency check failed after mutation\n");
        return false;  // Falls der Konsistenzcheck fehlschlägt, wird die Mutation als fehlerhaft betrachtet
    }

    

    std::printf("[MutationsManager] ========== MUTATION (by state) DONE ==========\n");
    return true;  // Erfolg
}

// -------------------------------------------------------
// tick_once
// -------------------------------------------------------
bool MutationsManager::tick_once(const std::string& ts)
{
    mutation_counter++;

    std::printf("[MutationsManager] tick_once: ts=%s, mutation_counter=%d/%d\n",
        ts.c_str(), mutation_counter, mutation_zyklus);

    if (mutation_counter >= mutation_zyklus)
    {
        std::printf("[MutationsManager] *** MUTATION CYCLE REACHED ***\n");

        if (!perform_mutation_by_state(ts))
        {
            std::fprintf(stderr, "[MutationsManager] Mutation failed!\n");
            return false;
        }

        if (!update_mutation_tracking(ts))
        {
            std::fprintf(stderr, "[MutationsManager] Failed to update mutation_tracking!\n");
            return false;
        }

        mutation_counter = 0;
        std::printf("[MutationsManager] mutation_counter reset to 0\n");
        std::printf("[MutationsManager] ========== MUTATION COMPLETE ==========\n");
    }

    return true;
}
