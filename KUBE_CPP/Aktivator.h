
#pragma once
#include <string>
#include <vector>

// Forward-Decl für sqlite
struct sqlite3;

class MultiObjectiveManager;

class Aktivator
{
public:
    struct Row {
        int   magic{ 0 };
        double pf_norm{ 0.0 };   // NetProfitNorm
        double act_norm{ 0.0 };  // ActivityNorm
        double r{ 0.0 };         // Risk (0..1)
        long  profit_cents{ 0 }; // raw Netto-Profite für Filter
    };

    enum class SelectionMode {
        Percentile = 0,
        ParetoFront1 = 1
    };

    Aktivator(sqlite3* db_experts,
        sqlite3* db_swarm,
        const std::string& fitness_table,
        const std::string& swarms_table,
        const std::string& config_path,
        SelectionMode     mode = SelectionMode::Percentile,
        int               fitness_id = 0);

    void set_top_percent(int p);   // 1..100 → m_top_frac
    void set_min_frac(double x);   // 0..1   → m_min_frac

    // Haupt-API
    bool apply_for_ts(const std::string& ts_key,
        int source_swarm_id,
        int target_swarm_value,
        int magic_delta);

private:
    // --- Members ---
    sqlite3* m_db_experts{ nullptr };
    sqlite3* m_db_swarm{ nullptr };
    std::string m_fitness_table;
    std::string m_swarms_table;
    int         m_fitness_id{ 0 };

    double      m_top_frac{ 0.50 };  // Default 50%
    double      m_min_frac{ 0.0 };
    bool        m_use_dynamic_frac{ true };
    SelectionMode m_mode{ SelectionMode::Percentile };

    MultiObjectiveManager* m_multi{ nullptr };

    // --- interne Helfer ---
    static int  read_top_eas_from_config(const std::string& path);

    bool fetch_rows(const std::string& ts_key,
        int swarm_id,
        std::vector<Row>& out) const;

    bool fetch_active_frac(const std::string& ts_key,
        int swarm_id,
        double& out_frac) const;

    static double quantile_cutoff(std::vector<double> v, double top_frac);

    bool apply_selection_atomic(const std::vector<int>& selected_live_magics,
        int target_swarm_value) const;

    // Diagnose: nach Commit die aktiven Live-EAs zählen
    int count_active_in_swarm(int target_swarm_value) const;

    // Varianten
    bool apply_percentile_for_ts(const std::string& ts_key,
        int source_swarm_id,
        int target_swarm_value,
        int magic_delta);

    bool apply_pareto_for_ts(const std::string& ts_key,
        int source_swarm_id,
        int target_swarm_value,
        int magic_delta);
};
