
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "MultiObjectiveManager.h"
#include "sqlite3.h"

#include <iostream>
#include <algorithm>
#include <cmath>

// ---------------------------------------------
// ctor
// ---------------------------------------------
MultiObjectiveManager::MultiObjectiveManager(sqlite3* db,
    const std::string& fitness_table,
    int fitness_id)
    : m_db(db)
    , m_fitness_table(fitness_table)
    , m_fitness_id(fitness_id)
{
}

// ---------------------------------------------
// Kandidaten aus Fitness_proBar laden
// ---------------------------------------------
bool MultiObjectiveManager::fetch_candidates_for_ts(int                swarm_id,
    const std::string& ts_key,
    std::vector<Candidate>& out) const
{
    out.clear();

    if (!m_db) {
        std::cerr << "[MultiObj] db=null\n";
        return false;
    }

    // Wir nutzen die normalisierten Spalten:
    // NetProfitNorm, ActivityNorm, R
    std::string sql =
        "SELECT ea_magic, NetProfitNorm, ActivityNorm, R "
        "FROM " + m_fitness_table + " "
        "WHERE swarm_id = ? AND ts_key = ? AND fitness_id = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[MultiObj] prepare(fetch_candidates_for_ts): "
            << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    sqlite3_bind_int(st, 1, swarm_id);
    sqlite3_bind_text(st, 2, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, m_fitness_id);

    while (sqlite3_step(st) == SQLITE_ROW) {
        Candidate c;
        c.ea_magic = sqlite3_column_int(st, 0);
        c.f_profit = sqlite3_column_double(st, 1);
        c.f_activity = sqlite3_column_double(st, 2);
        c.f_risk = sqlite3_column_double(st, 3);
        out.push_back(c);
    }

    sqlite3_finalize(st);

    if (out.empty()) {
        std::cerr << "[MultiObj] fetch_candidates_for_ts: keine Zeilen fuer swarm_id="
            << swarm_id << " ts=" << ts_key << " fitness_id=" << m_fitness_id << "\n";
    }
    else {
       /* std::cerr << "[MultiObj] fetch_candidates_for_ts: " << out.size()
            << " Kandidaten geladen\n";*/
    }

    return true;
}

// ---------------------------------------------
// Dominanz-Check
// a dominiert b, wenn alle Ziele >= und mindestens eins > (mit eps)
// ---------------------------------------------
bool MultiObjectiveManager::dominates(const Candidate& a,
    const Candidate& b,
    double eps) const
{
    bool better_in_at_least_one = false;

    // Profit
    if (a.f_profit + eps < b.f_profit) return false;
    if (a.f_profit > b.f_profit + eps) better_in_at_least_one = true;

    // Activity
    if (a.f_activity + eps < b.f_activity) return false;
    if (a.f_activity > b.f_activity + eps) better_in_at_least_one = true;

    // Risk
    if (a.f_risk + eps < b.f_risk) return false;
    if (a.f_risk > b.f_risk + eps) better_in_at_least_one = true;

    return better_in_at_least_one;
}

// ---------------------------------------------
// Non-dominated Sorting (O(N^2); N≈216 -> unkritisch)
// out_front_rank[i] = 1,2,3,... fuer Kandidat i
// ---------------------------------------------
void MultiObjectiveManager::compute_pareto_fronts(const std::vector<Candidate>& candidates,
    std::vector<int>& out_front_rank,
    double eps) const
{
    const size_t N = candidates.size();
    out_front_rank.assign(N, 0);
    if (N == 0) return;

    // Indizes der noch nicht zugeordneten Kandidaten
    std::vector<int> remaining(N);
    for (size_t i = 0; i < N; ++i)
        remaining[i] = static_cast<int>(i);

    int current_front = 1;

    while (!remaining.empty()) {
        std::vector<int> front;

        // Alle Kandidaten in remaining pruefen, ob sie von einem anderen in remaining dominiert werden
        for (int idx_i : remaining) {
            const Candidate& ci = candidates[idx_i];
            bool dominated_by_someone = false;

            for (int idx_j : remaining) {
                if (idx_i == idx_j) continue;
                const Candidate& cj = candidates[idx_j];
                if (dominates(cj, ci, eps)) {
                    dominated_by_someone = true;
                    break;
                }
            }

            if (!dominated_by_someone) {
                front.push_back(idx_i);
            }
        }

        if (front.empty()) {
            // numerischer Notfall: wenn wegen eps nichts mehr gefunden wird,
            // packen wir alle remaining in die naechste Front
            for (int idx : remaining) {
                out_front_rank[idx] = current_front;
            }
            break;
        }

        // Frontnummer zuweisen
        for (int idx : front) {
            out_front_rank[idx] = current_front;
        }

        // diese Indizes aus remaining entfernen
        std::vector<int> new_remaining;
        new_remaining.reserve(remaining.size());
        for (int idx : remaining) {
            if (out_front_rank[idx] == 0) {
                new_remaining.push_back(idx);
            }
        }
        remaining.swap(new_remaining);

        ++current_front;
    }

    /*std::cerr << "[MultiObj] compute_pareto_fronts: "
        << current_front - 1 << " Fronten erzeugt\n";*/
}

// ---------------------------------------------
// Kombinierter Helfer: fetchen + ranken
// ---------------------------------------------
bool MultiObjectiveManager::fetch_and_rank(int                swarm_id,
    const std::string& ts_key,
    std::vector<Candidate>& out_candidates,
    std::vector<int>& out_front_rank,
    double eps) const
{
    if (!fetch_candidates_for_ts(swarm_id, ts_key, out_candidates))
        return false;

    compute_pareto_fronts(out_candidates, out_front_rank, eps);
    return true;
}

// =============================================================
// NEU: Front-1 Helfer
// =============================================================

// ---------------------------------------------
// Indizes der Paretofront 1 bestimmen
// ---------------------------------------------
void MultiObjectiveManager::get_front1_indices(const std::vector<int>& front_rank,
    std::vector<int>& out_idx_front1) const
{
    out_idx_front1.clear();
    const size_t N = front_rank.size();
    for (size_t i = 0; i < N; ++i) {
        if (front_rank[i] == 1) {
            out_idx_front1.push_back(static_cast<int>(i));
        }
    }
}

// ---------------------------------------------
// Kandidaten der Paretofront 1 extrahieren
// ---------------------------------------------
void MultiObjectiveManager::extract_front1(const std::vector<Candidate>& candidates,
    const std::vector<int>& front_rank,
    std::vector<Candidate>& out_front1) const
{
    out_front1.clear();
    const size_t N = candidates.size();
    if (front_rank.size() != N) return;

    for (size_t i = 0; i < N; ++i) {
        if (front_rank[i] == 1) {
            out_front1.push_back(candidates[i]);
        }
    }
}

// =============================================================
// NEU: TOPSIS-Helfer
// =============================================================

// Regime-Struct kommt z.B. aus der Header-Datei:
// struct TopsisRegime { double w_profit, w_risk, w_activity; };

// ---------------------------------------------
// TOPSIS-Scores fuer alle Kandidaten nach gegebenem Regime
// (hier: einfache gewichtete Summe, da alle Ziele bereits 0..1
//  und "je groesser desto besser" sind)
// ---------------------------------------------
void MultiObjectiveManager::compute_topsis_scores(const std::vector<Candidate>& candidates,
    const TopsisRegime& regime,
    std::vector<double>& out_scores) const
{
    out_scores.clear();
    const size_t N = candidates.size();
    out_scores.resize(N);

    // Gewichte auf Summe 1 normalisieren (robust)
    double wP = regime.w_profit;
    double wR = regime.w_risk;
    double wA = regime.w_activity;
    double wsum = wP + wR + wA;
    if (wsum <= 0.0) {
        // Fallback: alle gleich behandeln
        wP = wR = wA = 1.0 / 3.0;
    }
    else {
        wP /= wsum;
        wR /= wsum;
        wA /= wsum;
    }

    for (size_t i = 0; i < N; ++i) {
        const Candidate& c = candidates[i];
        out_scores[i] =
            wP * c.f_profit
            + wR * c.f_risk
            + wA * c.f_activity;
    }
}

// ---------------------------------------------
// TOPSIS-Ranking fuer Front 1:
//  - nimmt alle Kandidaten + Front-Ranks
//  - extrahiert Front 1
//  - berechnet TOPSIS-Scores fuer Front 1
//  - liefert Indizes (im Original-Array) sortiert nach Score absteigend
//  - optional: Scores fuer Front-1 zurueck
// ---------------------------------------------
void MultiObjectiveManager::compute_front1_topsis_ranking(
    const std::vector<Candidate>& candidates,
    const std::vector<int>& front_rank,
    const TopsisRegime& regime,
    std::vector<int>& out_idx_front1_sorted,
    std::vector<double>* out_scores_front1 /*=nullptr*/) const
{
    out_idx_front1_sorted.clear();

    // 1) Indizes von Front 1
    std::vector<int> idx_f1;
    get_front1_indices(front_rank, idx_f1);
    if (idx_f1.empty())
        return;

    // 2) separate Scores fuer Front-1-Kandidaten
    std::vector<double> scores_all;
    compute_topsis_scores(candidates, regime, scores_all);

    // 3) Indizes von Front 1 nach Score sortieren (absteigend)
    std::sort(idx_f1.begin(), idx_f1.end(),
        [&](int a, int b) {
            return scores_all[a] > scores_all[b];
        });

    out_idx_front1_sorted = idx_f1;

    // optional: Scores fuer Front 1 zurueckgeben
    if (out_scores_front1) {
        out_scores_front1->clear();
        out_scores_front1->reserve(idx_f1.size());
        for (int idx : idx_f1) {
            out_scores_front1->push_back(scores_all[idx]);
        }
    }
}

// =============================================================
// NEU: Pareto1-Flag in DB schreiben
// =============================================================

// ---------------------------------------------
// Schreibt Pareto1-Flags zurueck in Fitness_proBar
// ---------------------------------------------
bool MultiObjectiveManager::update_pareto1_flags(int swarm_id,
    const std::string& ts_key,
    const std::vector<Candidate>& candidates,
    const std::vector<int>& front_rank) const
{
    if (!m_db) {
        std::cerr << "[MultiObj] update_pareto1_flags: db=null\n";
        return false;
    }

    if (candidates.size() != front_rank.size()) {
        std::cerr << "[MultiObj] update_pareto1_flags: size mismatch\n";
        return false;
    }

    // SQL: UPDATE Fitness_proBar SET Pareto1 = ? WHERE swarm_id=? AND ts_key=? AND ea_magic=? AND fitness_id=?
    std::string sql =
        "UPDATE " + m_fitness_table + " "
        "SET Pareto1 = ? "
        "WHERE swarm_id = ? AND ts_key = ? AND ea_magic = ? AND fitness_id = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[MultiObj] prepare(update_pareto1_flags): "
            << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    bool all_ok = true;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const int pareto1_flag = (front_rank[i] == 1) ? 1 : 0;
        const int ea_magic = candidates[i].ea_magic;

        sqlite3_reset(st);
        sqlite3_clear_bindings(st);

        sqlite3_bind_int(st, 1, pareto1_flag);
        sqlite3_bind_int(st, 2, swarm_id);
        sqlite3_bind_text(st, 3, ts_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 4, ea_magic);
        sqlite3_bind_int(st, 5, m_fitness_id);

        if (sqlite3_step(st) != SQLITE_DONE) {
            std::cerr << "[MultiObj] step(update_pareto1_flags) failed for ea_magic="
                << ea_magic << ": " << sqlite3_errmsg(m_db) << "\n";
            all_ok = false;
            break;
        }
    }

    sqlite3_finalize(st);

    if (all_ok) {
        // Zähle wie viele Pareto1=1 gesetzt wurden
        int count_front1 = 0;
        for (size_t i = 0; i < front_rank.size(); ++i) {
            if (front_rank[i] == 1) count_front1++;
        }
        std::cerr << "[MultiObj] update_pareto1_flags: " << count_front1
            << " EAs marked as Pareto Front 1 for ts=" << ts_key << "\n";
    }

    return all_ok;
}
