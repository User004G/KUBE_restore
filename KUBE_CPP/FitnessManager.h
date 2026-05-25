
#pragma once
#include <string>
#include <vector>

// nur Pointer-Typ im Header -> Forward-Decl reicht
struct sqlite3;

class FitnessDataManager {
public:
    FitnessDataManager(sqlite3* db,
        const std::string& balance_table,
        const std::string& fitness_table,
        int lookback_L,
        int lookback_id = 0);

    // Rechnet für einen ts_key
    // und schreibt NetProfitCents, NetProfitNorm,
    // Activity, ActivityNorm, R in Fitness_proBar
    bool compute_and_upsert_for_ts(const std::string& ts_key,
        int swarm_id,
        const std::vector<int>* ea_whitelist = nullptr,
        bool manage_tx = true);

private:
    // --- Members ---
    sqlite3* m_db{ nullptr };
    std::string m_balance_table;
    std::string m_fitness_table;
    int         m_lookback_L{ 0 };
    int         m_lookback_id{ 0 };

    // --- Datenzugriff ---
    bool fetch_ea_magics(int swarm_id,
        const std::string& ts_key,
        std::vector<int>& magics) const;

    bool fetch_window_for_ea(int swarm_id,
        int ea_magic,
        const std::string& ts_key,
        int L,
        std::vector<long>& profits,
        std::vector<long>& balances) const;

    // --- Metriken: Profit, Trades, R (ROH) ---
    // Profit  = Netto-Profit im Lookback (Cents, als double)
    // Trades  = Anzahl Trades im Lookback
    // R       = [0..1], wie gehabt
    static void compute_metrics(const std::vector<long>& profits,
        const std::vector<long>& balances,
        double& Profit,
        double& Trades,
        double& R);

    // --- Upsert in neues Schema
    //     (swarm_id, ts_key, ea_magic, fitness_id,
    //      NetProfitCents, NetProfitNorm,
    //      Activity, ActivityNorm, R, Pareto1)
    bool upsert_fitness_row(int                swarm_id,
        const std::string& ts_key,
        int                ea_magic,
        int                fitness_id,
        double             Profit,
        double             ProfitNorm,
        double             Trades,
        double             ActivityNorm,
        double             R,
        int                pareto_front = 0);
};
