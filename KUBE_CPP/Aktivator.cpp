
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Aktivator.h"
#include "MultiObjectiveManager.h"
#include "sqlite3.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <string>
#include <cmath>
#include <cfloat>

// ----------------------------------------------------
// Config lesen: c_top_EAs
// ----------------------------------------------------
int Aktivator::read_top_eas_from_config(const std::string& path)
{
    std::ifstream in(path);
    if (!in) return -1;

    std::string txt((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    std::smatch m;
    std::regex re(R"(c_top_EAs\s*=\s*([0-9]+))");
    if (std::regex_search(txt, m, re)) {
        int p = std::stoi(m[1].str());
        if (p < 1)   p = 1;
        if (p > 100) p = 100;
        return p;
    }
    return -1;
}

// ----------------------------------------------------
// Konstruktor
// ----------------------------------------------------
Aktivator::Aktivator(sqlite3* db_experts,
    sqlite3* db_swarm,
    const std::string& fitness_table,
    const std::string& swarms_table,
    const std::string& config_path,
    SelectionMode     mode,
    int               fitness_id)
    : m_db_experts(db_experts)
    , m_db_swarm(db_swarm)
    , m_fitness_table(fitness_table)
    , m_swarms_table(swarms_table)
    , m_fitness_id(fitness_id)
    , m_mode(mode)
{
    int p = read_top_eas_from_config(config_path);
    if (p > 0)
        m_top_frac = static_cast<double>(p) / 100.0; // sonst Default 0.50

    if (m_mode == SelectionMode::ParetoFront1 && m_db_experts) {
        m_multi = new MultiObjectiveManager(m_db_experts, m_fitness_table, m_fitness_id);
    }
    else {
        m_multi = nullptr;
    }
}

// ----------------------------------------------------
// Setter
// ----------------------------------------------------
void Aktivator::set_top_percent(int p)
{
    if (p < 1)   p = 1;
    if (p > 100) p = 100;
    m_top_frac = static_cast<double>(p) / 100.0;
}

void Aktivator::set_min_frac(double x)
{
    if (x < 0.0) x = 0.0;
    else if (x > 1.0) x = 1.0;
    m_min_frac = x;
}

// ----------------------------------------------------
// Fitness-Zeilen holen (Perzentilen-Modus)
// ----------------------------------------------------
bool Aktivator::fetch_rows(const std::string& ts_key,
    int swarm_id,
    std::vector<Row>& out) const
{
    out.clear();

    std::string sql =
        "SELECT ea_magic, "
        "       NetProfitNorm, ActivityNorm, R, "
        "       NetProfitCents "
        "FROM " + m_fitness_table + " "
        "WHERE swarm_id = ? AND ts_key = ? AND fitness_id = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db_experts, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[sqlite] prepare(fetch_rows): " << sqlite3_errmsg(m_db_experts) << "\n";
        return false;
    }

    sqlite3_bind_int(st, 1, swarm_id);
    sqlite3_bind_text(st, 2, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, m_fitness_id);

    while (sqlite3_step(st) == SQLITE_ROW) {
        Row row;
        row.magic = sqlite3_column_int(st, 0);
        row.pf_norm = sqlite3_column_double(st, 1);
        row.act_norm = sqlite3_column_double(st, 2);
        row.r = sqlite3_column_double(st, 3);
        row.profit_cents = static_cast<long>(sqlite3_column_int64(st, 4));
        out.push_back(row);
    }

    sqlite3_finalize(st);
    return true;
}

// ----------------------------------------------------
// risk_state.active_frac lesen
// ----------------------------------------------------
bool Aktivator::fetch_active_frac(const std::string& ts_key,
    int swarm_id,
    double& out_frac) const
{
    const char* q =
        "SELECT active_frac "
        "FROM risk_state "
        "WHERE schwarm=? AND timestamp=? "
        "LIMIT 1;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db_swarm, q, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[sqlite] prepare(fetch_active_frac): " << sqlite3_errmsg(m_db_swarm) << "\n";
        return false;
    }
    sqlite3_bind_int(st, 1, swarm_id);
    sqlite3_bind_text(st, 2, ts_key.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out_frac = sqlite3_column_double(st, 0);
        if (std::isnan(out_frac) || !std::isfinite(out_frac))
            out_frac = 0.0;
        if (out_frac < 0.0) out_frac = 0.0;
        if (out_frac > 1.0) out_frac = 1.0;
        ok = true;
    }
    sqlite3_finalize(st);
    return ok;
}

// ----------------------------------------------------
// Quantils-Schwelle (Top-p%)
// ----------------------------------------------------
double Aktivator::quantile_cutoff(std::vector<double> v, double top_frac)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end(), std::greater<double>());
    const size_t n = v.size();
    size_t k = static_cast<size_t>(std::ceil(n * top_frac));
    if (k < 1) k = 1;
    if (k > n) k = n;
    return v[k - 1];
}

// ----------------------------------------------------
// Diagnose: aktive EAs nach Commit zählen
// ----------------------------------------------------
int Aktivator::count_active_in_swarm(int target_swarm_value) const
{
    if (!m_db_swarm) return 0;

    const char* q = "SELECT COUNT(*) FROM swarms WHERE swarm=? AND aktiv=1;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db_swarm, q, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[AKTIVATOR][CHECK] prepare(count_active) failed: "
            << sqlite3_errmsg(m_db_swarm) << "\n";
        return 0;
    }
    sqlite3_bind_int(st, 1, target_swarm_value);

    int cnt = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        cnt = sqlite3_column_int(st, 0);

    sqlite3_finalize(st);
    return cnt;
}

// ----------------------------------------------------
// Atomar in swarms.aktiv schreiben
// ----------------------------------------------------
bool Aktivator::apply_selection_atomic(const std::vector<int>& selected_live_magics,
    int target_swarm_value) const
{
    char* err = nullptr;
    if (sqlite3_exec(m_db_swarm, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[sqlite] begin(swarm): " << (err ? err : "") << "\n";
        if (err) sqlite3_free(err);
        return false;
    }

    bool ok = true;

    if (selected_live_magics.empty()) {
        const char* q = "UPDATE swarms SET aktiv=0 WHERE swarm=? AND aktiv=1;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db_swarm, q, -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(st, 1, target_swarm_value);
            if (sqlite3_step(st) != SQLITE_DONE) {
                std::cerr << "[sqlite] step(deactivate-all): " << sqlite3_errmsg(m_db_swarm) << "\n";
                ok = false;
            }
            //std::cerr << "[AKTIVATOR] deactivated_count=" << sqlite3_changes(m_db_swarm) << "\n";
            sqlite3_finalize(st);
        }
        else {
            std::cerr << "[sqlite] prepare(deactivate-all): " << sqlite3_errmsg(m_db_swarm) << "\n";
            ok = false;
        }
    }
    else {
        // 1) Alles deaktivieren, was NICHT in der Liste ist
        std::ostringstream os;
        os << "UPDATE swarms SET aktiv=0 WHERE swarm=? AND aktiv=1 AND Magic NOT IN (";
        for (size_t i = 0; i < selected_live_magics.size(); ++i) {
            if (i) os << ",";
            os << "?";
        }
        os << ");";
        std::string sql = os.str();

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db_swarm, sql.c_str(), -1, &st, nullptr) == SQLITE_OK) {
            int bi = 1;
            sqlite3_bind_int(st, bi++, target_swarm_value);
            for (int m : selected_live_magics)
                sqlite3_bind_int(st, bi++, m);
            if (sqlite3_step(st) != SQLITE_DONE) {
                std::cerr << "[sqlite] step(deactivate-not-in): " << sqlite3_errmsg(m_db_swarm) << "\n";
                ok = false;
            }
            //std::cerr << "[AKTIVATOR] deactivated_count=" << sqlite3_changes(m_db_swarm) << "\n";
            sqlite3_finalize(st);
        }
        else {
            std::cerr << "[sqlite] prepare(deactivate-not-in): " << sqlite3_errmsg(m_db_swarm) << "\n";
            ok = false;
        }
    }

    if (ok && !selected_live_magics.empty()) {
        // 2) Alles in der Liste aktivieren
        std::ostringstream os;
        os << "UPDATE swarms SET aktiv=1 WHERE swarm=? AND Magic IN (";
        for (size_t i = 0; i < selected_live_magics.size(); ++i) {
            if (i) os << ",";
            os << "?";
        }
        os << ");";
        std::string sql = os.str();

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db_swarm, sql.c_str(), -1, &st, nullptr) == SQLITE_OK) {
            int bi = 1;
            sqlite3_bind_int(st, bi++, target_swarm_value);
            for (int m : selected_live_magics)
                sqlite3_bind_int(st, bi++, m);
            if (sqlite3_step(st) != SQLITE_DONE) {
                std::cerr << "[sqlite] step(activate-selected): " << sqlite3_errmsg(m_db_swarm) << "\n";
                ok = false;
            }
            //std::cerr << "[AKTIVATOR] activated_count=" << sqlite3_changes(m_db_swarm) << "\n";
            sqlite3_finalize(st);
        }
        else {
            std::cerr << "[sqlite] prepare(activate-selected): " << sqlite3_errmsg(m_db_swarm) << "\n";
            ok = false;
        }
    }

    if (sqlite3_exec(m_db_swarm, ok ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[sqlite] commit/rollback(swarm): " << (err ? err : "") << "\n";
        if (err) sqlite3_free(err);
        ok = false;
    }
    return ok;
}

// ----------------------------------------------------
// Variante A: Perzentilen-Schnittmenge
// ----------------------------------------------------
bool Aktivator::apply_percentile_for_ts(const std::string& ts_key,
    int source_swarm_id,
    int target_swarm_value,
    int magic_delta)
{
    std::vector<Row> rows;
    if (!fetch_rows(ts_key, source_swarm_id, rows)) return false;
    if (rows.empty()) {
        std::cerr << "[AKTIVATOR] (Percentile) keine Fitness-Zeilen für swarm_id="
            << source_swarm_id << " ts=" << ts_key << "\n";
        return true;
    }

    double frac = m_top_frac;
    if (m_use_dynamic_frac) {
        double f = frac;
        if (fetch_active_frac(ts_key, source_swarm_id, f))
            frac = f;
        if (frac < m_min_frac)
            frac = m_min_frac;
    }

    if (frac <= 0.0) {
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            std::cerr << "[AKTIVATOR][Percentile][CHECK] ts=" << ts_key
                << " frac<=0 live_count_after_commit=" << live_count << "\n";
        }
        return ok;
    }

    std::vector<double> v_pf, v_act, v_r;
    v_pf.reserve(rows.size());
    v_act.reserve(rows.size());
    v_r.reserve(rows.size());

    for (auto& r : rows) {
        if (r.profit_cents < 0)
            continue;
        v_pf.push_back(r.pf_norm);
        v_act.push_back(r.act_norm);
        v_r.push_back(r.r);
    }

    if (v_pf.empty() || v_act.empty() || v_r.empty()) {
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            std::cerr << "[AKTIVATOR][Percentile][CHECK] ts=" << ts_key
                << " after-empty-filter live_count_after_commit=" << live_count << "\n";
        }
        return ok;
    }

    const double cut_pf = quantile_cutoff(v_pf, frac);
    const double cut_act = quantile_cutoff(v_act, frac);
    const double cut_r = quantile_cutoff(v_r, frac);

    std::vector<int> selected_live_magics;
    selected_live_magics.reserve(rows.size());

    for (auto& r : rows) {
        if (r.profit_cents < 0)
            continue;
        if (r.pf_norm >= cut_pf &&
            r.act_norm >= cut_act &&
            r.r >= cut_r)
        {
            const int live_magic = r.magic + magic_delta;
            selected_live_magics.push_back(live_magic);
        }
    }

    //std::cerr << "[AKTIVATOR][Percentile] ts=" << ts_key
       /* << " frac=" << frac
        << " selected=" << selected_live_magics.size()
        << "\n";*/

    bool ok = apply_selection_atomic(selected_live_magics, target_swarm_value);
    if (ok) {
        int live_count = count_active_in_swarm(target_swarm_value);
        /*std::cerr << "[AKTIVATOR][Percentile][CHECK] ts=" << ts_key
            << " live_count_after_commit=" << live_count << "\n";*/
    }
    return ok;
}

// ----------------------------------------------------
// Variante B: Pareto-Front-1
// ----------------------------------------------------
bool Aktivator::apply_pareto_for_ts(const std::string& ts_key,
    int source_swarm_id,
    int target_swarm_value,
    int magic_delta)
{
    if (!m_multi) {
        std::cerr << "[AKTIVATOR][Pareto] m_multi=null, fallback: alles aus\n";
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            /*std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
                << " m_multi=null live_count_after_commit=" << live_count << "\n";*/
        }
        return ok;
    }

    /*std::vector<MultiObjectiveManager::Candidate> cand;*/
    std::vector<Candidate> cand;
    std::vector<int> front_rank;
    double eps = 1e-6;  // oder was du willst
    

    if (!m_multi->fetch_and_rank(source_swarm_id, ts_key, cand, front_rank, eps)) {
        std::cerr << "[AKTIVATOR][Pareto] fetch_and_rank fehlgeschlagen\n";
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            /*std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
                << " fetch_fail live_count_after_commit=" << live_count << "\n";*/
        }
        return ok;
    }

    if (cand.empty() || front_rank.size() != cand.size()) {
        //std::cerr << "[AKTIVATOR][Pareto] keine Kandidaten oder Rank-Mismatch\n";
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            /*std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
                << " empty/mismatch live_count_after_commit=" << live_count << "\n";*/
        }
        return ok;
    }

    // *** NEU: Pareto1-Flags in DB schreiben ***
    if (!m_multi->update_pareto1_flags(source_swarm_id, ts_key, cand, front_rank)) {
        std::cerr << "[AKTIVATOR][Pareto] update_pareto1_flags fehlgeschlagen\n";
        // Trotzdem weitermachen mit der Aktivierung
    }

    std::vector<int> idx_front1;
    idx_front1.reserve(cand.size());
    for (size_t i = 0; i < cand.size(); ++i)
        if (front_rank[i] == 1)
            idx_front1.push_back(static_cast<int>(i));

    if (idx_front1.empty()) {
        std::cerr << "[AKTIVATOR][Pareto] Front 1 leer -> alles aus\n";
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            /*std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
                << " front1_empty live_count_after_commit=" << live_count << "\n";    */
        }
        return ok;
    }

    double frac = 1.0;
    if (m_use_dynamic_frac) {
        double f = 1.0;
        if (fetch_active_frac(ts_key, source_swarm_id, f)) {
            frac = f;
            if (frac < m_min_frac)
                frac = m_min_frac;
        }
    }

    if (frac <= 0.0) {
        //std::cerr << "[AKTIVATOR][Pareto] active_frac<=0 -> alles aus\n";
        std::vector<int> none;
        bool ok = apply_selection_atomic(none, target_swarm_value);
        if (ok) {
            int live_count = count_active_in_swarm(target_swarm_value);
            /*std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
                << " frac<=0 live_count_after_commit=" << live_count << "\n";*/
        }
        return ok;
    }

    std::vector<int> selected_live_magics;
    selected_live_magics.reserve(idx_front1.size());
    for (int idx : idx_front1) {
        const auto& c = cand[idx];
        const int live_magic = c.ea_magic + magic_delta;
        selected_live_magics.push_back(live_magic);
    }

    std::cerr << "[AKTIVATOR][Pareto] ts=" << ts_key
        << " front1_size=" << idx_front1.size()
        << " active_frac=" << frac
        << " selected=" << selected_live_magics.size()
        << "\n";

    bool ok = apply_selection_atomic(selected_live_magics, target_swarm_value);
    if (ok) {
        int live_count = count_active_in_swarm(target_swarm_value);
        std::cerr << "[AKTIVATOR][Pareto][CHECK] ts=" << ts_key
            << " live_count_after_commit=" << live_count << "\n";
    }
    return ok;
}

// ----------------------------------------------------
// Haupt-API
// ----------------------------------------------------
bool Aktivator::apply_for_ts(const std::string& ts_key,
    int source_swarm_id,
    int target_swarm_value,
    int magic_delta)
{
    if (m_mode == SelectionMode::ParetoFront1)
        return apply_pareto_for_ts(ts_key, source_swarm_id, target_swarm_value, magic_delta);
    else
        return apply_percentile_for_ts(ts_key, source_swarm_id, target_swarm_value, magic_delta);
}
