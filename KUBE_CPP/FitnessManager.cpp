
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>  // std::accumulate

#include "sqlite3.h"
#include "FitnessManager.h"

// ----------------------------------------------------------
// ctor
// ----------------------------------------------------------
FitnessDataManager::FitnessDataManager(sqlite3* db,
    const std::string& balance_table,
    const std::string& fitness_table,
    int lookback_L,
    int lookback_id)
    : m_db(db)
    , m_balance_table(balance_table)
    , m_fitness_table(fitness_table)
    , m_lookback_L(lookback_L)
    , m_lookback_id(lookback_id)
{
}

// ----------------------------------------------------------
// kleine util clamp + zscore+sigmoid
// ----------------------------------------------------------
namespace util {

    template <typename T>
    inline T clamp(T v, T lo, T hi)
    {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    inline double zscore_sigmoid_normalize(double x,
        double mean,
        double std_dev,
        double k = 1.0)
    {
        const double eps = 1e-9;
        if (std_dev < eps)
        {
            // alle Werte praktisch gleich -> mittlerer Wert
            return 0.5;
        }
        const double z = (x - mean) / std_dev;
        const double s = 1.0 / (1.0 + std::exp(-k * z));
        return s; // in (0,1)
    }

} // namespace util

// ----------------------------------------------------------
// Metriken: Profit, Trades, R (ROH)
// ----------------------------------------------------------
void FitnessDataManager::compute_metrics(const std::vector<long>& profits,
    const std::vector<long>& balances,
    double& Profit,
    double& Trades,
    double& R)
{
    Profit = 0.0;
    Trades = 0.0;
    R = 1.0;

    const size_t N = balances.size();
    if (N == 0)
        return;

    // A) Profit & Trades
    long sum_gain_abs = 0;
    long sum_loss_abs = 0;
    int  n_trades_window = 0;

    for (size_t i = 0; i < profits.size(); ++i)
    {
        const long p = profits[i];
        if (p != 0)
        {
            ++n_trades_window;
            if (p > 0)
                sum_gain_abs += p;
            else
                sum_loss_abs += -p;
        }
    }

    const long profit_raw = sum_gain_abs - sum_loss_abs;
    Profit = static_cast<double>(profit_raw);
    Trades = static_cast<double>(n_trades_window);

    // B) Risiko R = 1 - MAD_DD / |MaxDD|
    long peak = balances.front();
    long max_dd_abs = 0;
    long sum_dd_abs = 0;

    for (size_t i = 0; i < N; ++i)
    {
        const long eq = balances[i];
        if (eq > peak)
            peak = eq;

        const long dd = peak - eq;
        sum_dd_abs += dd;
        if (dd > max_dd_abs)
            max_dd_abs = dd;
    }

    const double dd_mad = static_cast<double>(sum_dd_abs) / static_cast<double>(N);

    if (max_dd_abs <= 0)
    {
        R = 1.0;
    }
    else
    {
        double ratio = dd_mad / static_cast<double>(max_dd_abs);
        ratio = util::clamp(ratio, 0.0, 1.0);
        R = 1.0 - ratio;
    }
}

// ----------------------------------------------------------
// EA-Liste für Snapshot-Zeitpunkt
// ----------------------------------------------------------
bool FitnessDataManager::fetch_ea_magics(int swarm_id,
    const std::string& ts_key,
    std::vector<int>& magics) const
{
    magics.clear();

    std::string sql =
        "SELECT DISTINCT ea_magic "
        "FROM " + m_balance_table + " "
        "WHERE swarm_id = ? AND ts_key = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
    {
        std::cerr << "[sqlite] prepare(fetch_ea_magics): "
            << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    sqlite3_bind_int(st, 1, swarm_id);
    sqlite3_bind_text(st, 2, ts_key.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW)
    {
        magics.push_back(sqlite3_column_int(st, 0));
    }

    sqlite3_finalize(st);
    return true;
}

// ----------------------------------------------------------
// Window für einen EA holen (letzte L Zeilen bis ts_key)
// ----------------------------------------------------------
bool FitnessDataManager::fetch_window_for_ea(int swarm_id,
    int ea_magic,
    const std::string& ts_key,
    int L,
    std::vector<long>& profits,
    std::vector<long>& balances) const
{
    profits.clear();
    balances.clear();

    std::string sql =
        "SELECT ts_key, profit_cents, balance_cents "
        "FROM ("
        "  SELECT ts_key, profit_cents, balance_cents "
        "  FROM " + m_balance_table + " "
        "  WHERE swarm_id = ? AND ea_magic = ? AND ts_key <= ? "
        "  ORDER BY ts_key DESC LIMIT ?"
        ") ORDER BY ts_key ASC;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
    {
        std::cerr << "[sqlite] prepare(fetch_window_for_ea): "
            << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    sqlite3_bind_int(st, 1, swarm_id);
    sqlite3_bind_int(st, 2, ea_magic);
    sqlite3_bind_text(st, 3, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, L);

    while (sqlite3_step(st) == SQLITE_ROW)
    {
        const long p = static_cast<long>(sqlite3_column_int64(st, 1));
        const long eq = static_cast<long>(sqlite3_column_int64(st, 2));
        profits.push_back(p);
        balances.push_back(eq);
    }

    sqlite3_finalize(st);
    return true;
}

// ----------------------------------------------------------
// Upsert in Fitness_proBar (neues Schema)
// ----------------------------------------------------------
bool FitnessDataManager::upsert_fitness_row(int                swarm_id,
    const std::string& ts_key,
    int                ea_magic,
    int                fitness_id,
    double             Profit,
    double             ProfitNorm,
    double             Trades,
    double             ActivityNorm,
    double             R,
    int                pareto_front)
{
    auto r6 = [](double v) { return std::round(v * 1e6) / 1e6; };

    std::string sql =
        "INSERT OR REPLACE INTO " + m_fitness_table + " ("
        "  swarm_id, ts_key, ea_magic, fitness_id,"
        "  NetProfitCents, NetProfitNorm,"
        "  Activity, ActivityNorm, R, Pareto1"
        ") VALUES (?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
    {
        std::cerr << "[sqlite] prepare(upsert_fitness_row): "
            << sqlite3_errmsg(m_db) << "\n";
        return false;
    }

    int bi = 1;
    sqlite3_bind_int(st, bi++, swarm_id);
    sqlite3_bind_text(st, bi++, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, bi++, ea_magic);
    sqlite3_bind_int(st, bi++, fitness_id);
    sqlite3_bind_double(st, bi++, r6(Profit));       // NetProfitCents (als double gespeichert)
    sqlite3_bind_double(st, bi++, r6(ProfitNorm));   // NetProfitNorm  (0..1)
    sqlite3_bind_double(st, bi++, r6(Trades));       // Activity (Trades roh)
    sqlite3_bind_double(st, bi++, r6(ActivityNorm)); // ActivityNorm (0..1)
    sqlite3_bind_double(st, bi++, r6(R));            // R (0..1)
    sqlite3_bind_int(st, bi++, pareto_front);        // Pareto1 (0 oder 1)

    const bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok)
    {
        std::cerr << "[sqlite] step(upsert_fitness_row): "
            << sqlite3_errmsg(m_db) << "\n";
    }

    sqlite3_finalize(st);
    return ok;
}

// ----------------------------------------------------------
// Hauptschritt: alle EAs zum ts_key verarbeiten
// ----------------------------------------------------------
bool FitnessDataManager::compute_and_upsert_for_ts(const std::string& ts_key,
    int swarm_id,
    const std::vector<int>* ea_whitelist,
    bool manage_tx)
{
    if (!m_db)
    {
        std::cerr << "[fitness] db=null\n";
        return false;
    }

    // --- EA-Liste bestimmen ---
    std::vector<int> magics;
    if (ea_whitelist && !ea_whitelist->empty())
        magics = *ea_whitelist;
    else if (!fetch_ea_magics(swarm_id, ts_key, magics))
        return false;

    if (magics.empty())
    {
        // Nichts zu tun (kein EA aktiv/geloggt)
        return true;
    }

    if (manage_tx)
    {
        char* err = nullptr;
        if (sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::cerr << "[sqlite] begin: " << (err ? err : "") << "\n";
            if (err) sqlite3_free(err);
            return false;
        }
    }

    bool all_ok = true;

    const size_t N = magics.size();
    std::vector<double> vProfit(N), vTrades(N), vR(N);

    // ---------- 1. Pass: Roh-Metriken pro EA sammeln ----------
    for (size_t i = 0; i < N; ++i)
    {
        int ea_magic = magics[i];

        std::vector<long> profits, balances;
        if (!fetch_window_for_ea(swarm_id, ea_magic, ts_key, m_lookback_L,
            profits, balances))
        {
            all_ok = false;
            break;
        }

        double P = 0.0, T = 0.0, R = 1.0;
        compute_metrics(profits, balances, P, T, R);

        vProfit[i] = P;
        vTrades[i] = T;
        vR[i] = R;
    }

    if (!all_ok)
    {
        if (manage_tx)
        {
            char* err = nullptr;
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, &err);
            if (err) sqlite3_free(err);
        }
        return false;
    }

    // ---------- 2. Normalisierung über alle EAs ----------

    // 2a) Profit-Normalisierung (z-Score -> Sigmoid)
    double meanP = 0.0;
    double stdP = 0.0;
    {
        double sumP = std::accumulate(vProfit.begin(), vProfit.end(), 0.0);
        meanP = sumP / static_cast<double>(N);

        double sq_sum = 0.0;
        for (double p : vProfit)
        {
            double d = p - meanP;
            sq_sum += d * d;
        }
        if (N > 1)
            stdP = std::sqrt(sq_sum / static_cast<double>(N - 1));
        else
            stdP = 0.0;
    }

    std::vector<double> vProfitNorm(N);
    for (size_t i = 0; i < N; ++i)
    {
        vProfitNorm[i] = util::zscore_sigmoid_normalize(
            vProfit[i], meanP, stdP, /*k=*/1.0
        );
    }

    // 2b) Activity-Normalisierung:
    //     0 Trades -> 0.0
    //     max Trades im Fenster (über alle EAs) -> 1.0
    double maxT = 0.0;
    for (double t : vTrades)
    {
        if (t > maxT)
            maxT = t;
    }

    std::vector<double> vActNorm(N);
    if (maxT <= 0.0)
    {
        // alle EAs haben 0 Trades -> Norm = 0.0
        for (size_t i = 0; i < N; ++i)
            vActNorm[i] = 0.0;
    }
    else
    {
        for (size_t i = 0; i < N; ++i)
        {
            vActNorm[i] = vTrades[i] / maxT; // 0..1
        }
    }

    // ---------- 3. Pass: Upserts mit Roh- + Normwerten ----------
    for (size_t i = 0; i < N; ++i)
    {
        int ea_magic = magics[i];
        if (!upsert_fitness_row(swarm_id,
            ts_key,
            ea_magic,
            m_lookback_id,
            vProfit[i],
            vProfitNorm[i],
            vTrades[i],
            vActNorm[i],
            vR[i]))
        {
            all_ok = false;
            break;
        }
    }

    if (manage_tx)
    {
        char* err = nullptr;
        if (sqlite3_exec(m_db,
            all_ok ? "COMMIT;" : "ROLLBACK;",
            nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::cerr << "[sqlite] tx end: " << (err ? err : "") << "\n";
            if (err) sqlite3_free(err);
            all_ok = false;
        }
    }

    return all_ok;
}
