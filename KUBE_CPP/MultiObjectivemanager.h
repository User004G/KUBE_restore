
#ifndef MULTIOBJECTIVEMANAGER_H
#define MULTIOBJECTIVEMANAGER_H

#include <string>
#include <vector>

// Forward declaration, sqlite3 wird in der .cpp via "sqlite3.h" eingebunden
struct sqlite3;

// Kandidat mit normalisierten Fitness-Werten aus Fitness_proBar
struct Candidate
{
    int    ea_magic;
    double f_profit;    // NetProfitNorm (0..1, higher = better)
    double f_activity;  // ActivityNorm  (0..1, higher = better)
    double f_risk;      // R             (0..1, higher = better)
};

// Regime fuer TOPSIS-Gewichtung (Profit / Risk / Activity)
struct TopsisRegime
{
    double w_profit;
    double w_risk;
    double w_activity;
};

class MultiObjectiveManager
{
public:
    MultiObjectiveManager(sqlite3* db,
        const std::string& fitness_table,
        int fitness_id);

    // Kandidaten fuer einen Zeitstempel aus Fitness_proBar laden
    bool fetch_candidates_for_ts(int swarm_id,
        const std::string& ts_key,
        std::vector<Candidate>& out) const;

    // Dominanz a dominiert b?
    bool dominates(const Candidate& a,
        const Candidate& b,
        double eps) const;

    // Paretofronten berechnen, out_front_rank[i] = 1,2,3,...
    void compute_pareto_fronts(const std::vector<Candidate>& candidates,
        std::vector<int>& out_front_rank,
        double eps) const;

    // fetchen + ranken in einem Schritt
    bool fetch_and_rank(int swarm_id,
        const std::string& ts_key,
        std::vector<Candidate>& out_candidates,
        std::vector<int>& out_front_rank,
        double eps) const;

    // ---------- NEU: Front-1 Helfer ----------

    // Indizes der Kandidaten mit Front-Rank == 1
    void get_front1_indices(const std::vector<int>& front_rank,
        std::vector<int>& out_idx_front1) const;

    // Kandidaten der Front 1 extrahieren
    void extract_front1(const std::vector<Candidate>& candidates,
        const std::vector<int>& front_rank,
        std::vector<Candidate>& out_front1) const;

    // ---------- NEU: TOPSIS-Helfer ----------

    // TOPSIS-Scores (hier: gewichtete Summe) fuer alle Kandidaten
    void compute_topsis_scores(const std::vector<Candidate>& candidates,
        const TopsisRegime& regime,
        std::vector<double>& out_scores) const;

    // TOPSIS-Ranking fuer Front 1:
    //  - nutzt candidates + front_rank
    //  - berechnet Scores gemaess regime
    //  - liefert Indizes von Front 1 sortiert nach Score (absteigend)
    //  - optional: Scores fuer Front 1
    void compute_front1_topsis_ranking(const std::vector<Candidate>& candidates,
        const std::vector<int>& front_rank,
        const TopsisRegime& regime,
        std::vector<int>& out_idx_front1_sorted,
        std::vector<double>* out_scores_front1 = nullptr) const;

    // ---------- NEU: Pareto1-Flag in DB schreiben ----------

    // Schreibt Pareto1-Flags zurueck in Fitness_proBar
    // front_rank[i] == 1 -> Pareto1 = 1, sonst 0
    bool update_pareto1_flags(int swarm_id,
        const std::string& ts_key,
        const std::vector<Candidate>& candidates,
        const std::vector<int>& front_rank) const;

private:
    sqlite3* m_db;
    std::string m_fitness_table;
    int         m_fitness_id;
};

#endif // MULTIOBJECTIVEMANAGER_H
