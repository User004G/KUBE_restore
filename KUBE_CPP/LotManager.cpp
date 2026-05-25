#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "LotManager.h"
#include "MultiObjectiveManager.h"
#include "sqlite3.h"

#include <algorithm>
#include <cmath>
#include <iostream>

// Wir nutzen Candidate und TopsisRegime aus MultiObjectiveManager.h
// struct Candidate { ... };
// struct TopsisRegime { double w_profit, w_risk, w_activity; };

// ---------------------------------------------
// ctor
// ---------------------------------------------
LotManager::LotManager(sqlite3* db_swarms,
    MultiObjectiveManager* mom,
    const std::string& swarms_table,
    const LotManagerConfig& cfg)
    : m_db_swarms(db_swarms)
    , m_mom(mom)
    , m_swarms_table(swarms_table)
    , m_cfg(cfg)
{
}

//// ---------------------------------------------
//// Budget-Skalierung aus cushion
//// Hier simple lineare Variante: scale_B = clamp(c,0,1)
//// (leicht austauschbar gegen c^2 oder sqrt(c))
//// ---------------------------------------------
//double LotManager::compute_budget_scale(double cushion) const
//{
//    if (cushion < 0.0) cushion = 0.0;
//    if (cushion > 1.0) cushion = 1.0;
//    return cushion;
//}

// ---------------------------------------------
// TOPSIS-Regime je nach Cushion
// DEFENSIV / KONSERVATIV / NORMAL / EXPANSION
// (P/R/A-Gewichte)
// ---------------------------------------------
TopsisRegime LotManager::choose_topsis_regime(double cushion) const
{
    if (cushion < 0.0) cushion = 0.0;
    if (cushion > 1.0) cushion = 1.0;

    TopsisRegime regime;

    if (cushion < 0.2) {
        // DEFENSIV: 0.2 / 0.6 / 0.2
        regime.w_profit = 0.2;
        regime.w_risk = 0.6;
        regime.w_activity = 0.2;
    }
    else if (cushion < 0.5) {
        // KONSERVATIV: 0.3 / 0.5 / 0.2
        regime.w_profit = 0.3;
        regime.w_risk = 0.5;
        regime.w_activity = 0.2;
    }
    else if (cushion < 0.7) {
        // NORMAL: 0.5 / 0.3 / 0.2
        regime.w_profit = 0.5;
        regime.w_risk = 0.3;
        regime.w_activity = 0.2;
    }
    else {
        // EXPANSION: 0.6 / 0.2 / 0.2
        regime.w_profit = 0.6;
        regime.w_risk = 0.2;
        regime.w_activity = 0.2;
    }

    return regime;
}

// ---------------------------------------------
// Exponentielle Gewichte fuer Ranga i=0..N-1
// w_i ~ exp(-gamma * i), normalisiert auf Summe 1
// ---------------------------------------------
void LotManager::compute_exp_weights(size_t N,
    double gamma,
    std::vector<double>& out_weights) const
{
    out_weights.assign(N, 0.0);
    if (N == 0) return;

    double sum_w = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double w = std::exp(-gamma * static_cast<double>(i));
        out_weights[i] = w;
        sum_w += w;
    }
    if (sum_w <= 0.0) return;

    for (size_t i = 0; i < N; ++i) {
        out_weights[i] /= sum_w;
    }
}

// ---------------------------------------------
// Lots in swarms-Tabelle schreiben
// (Magic ist PRIMARY KEY in swarms)
// ---------------------------------------------
bool LotManager::update_lots_in_db(const std::vector<int>& magics,
    const std::vector<double>& lots)
{
    if (!m_db_swarms) {
        std::cerr << "[LotManager] db_swarms=null\n";
        return false;
    }
    if (magics.size() != lots.size()) {
        std::cerr << "[LotManager] magics.size != lots.size\n";
        return false;
    }

    char* err = nullptr;
    if (sqlite3_exec(m_db_swarms, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[LotManager] BEGIN failed: " << (err ? err : "") << "\n";
        if (err) sqlite3_free(err);
        return false;
    }

    std::string sql =
        "UPDATE " + m_swarms_table + " SET lot = ? WHERE Magic = ?;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db_swarms, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[LotManager] prepare(update lot): "
            << sqlite3_errmsg(m_db_swarms) << "\n";
        sqlite3_exec(m_db_swarms, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < magics.size(); ++i) {
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);

        double lot = lots[i];

        // Broker-MinLot / MaxLot clamp (falls konfiguriert)
        if (m_cfg.min_lot > 0.0 && lot < m_cfg.min_lot)
            lot = m_cfg.min_lot;
        if (m_cfg.max_lot > 0.0 && lot > m_cfg.max_lot)
            lot = m_cfg.max_lot;

        // auf 0.01 runden (2 Nachkommastellen)
        lot = std::round(lot * 100.0) / 100.0;

        sqlite3_bind_double(st, 1, lot);
        sqlite3_bind_int(st, 2, magics[i]);

        if (sqlite3_step(st) != SQLITE_DONE) {
            std::cerr << "[LotManager] step(update lot) failed for Magic="
                << magics[i] << ": " << sqlite3_errmsg(m_db_swarms) << "\n";
            ok = false;
            break;
        }
    }

    sqlite3_finalize(st);

    if (sqlite3_exec(m_db_swarms, ok ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[LotManager] tx end failed: " << (err ? err : "") << "\n";
        if (err) sqlite3_free(err);
        ok = false;
    }

    return ok;
}
// ---------------------------------------------
bool LotManager::assign_lots_for_ts(int                     swarm_id,
    const std::string& ts_key,
    double                  cushion,
    const std::vector<int>* /*active_magics*/)
{
    if (!m_mom) {
        std::cerr << "[LotManager] m_mom=null\n";
        return false;
    }

    // 1) Kandidaten + Paretofronten holen (Basis: Paper-Schwarm 'swarm_id')
    std::vector<Candidate> cand;
    std::vector<int>       front_rank;

    const double eps = 1e-6;
    if (!m_mom->fetch_and_rank(swarm_id, ts_key, cand, front_rank, eps)) {
        std::cerr << "[LotManager] fetch_and_rank failed\n";
        return false;
    }

    if (cand.empty()) {
        std::cerr << "[LotManager] no candidates for ts=" << ts_key << "\n";
        return false;
    }

    if (front_rank.size() != cand.size()) {
        std::cerr << "[LotManager] front_rank.size != cand.size\n";
        return false;
    }

    // 2) Alle Kandidaten der Paretofront 1 einsammeln
    std::vector<int> idx_pf1;
    idx_pf1.reserve(cand.size());
    for (size_t i = 0; i < cand.size(); ++i) {
        if (front_rank[i] == 1) {
            idx_pf1.push_back(static_cast<int>(i));
        }
    }

    if (idx_pf1.empty()) {
        std::cerr << "[LotManager] no Front-1 candidates, fallback: use ALL\n";
        // Falls aus numerischen Gruenden keine PF1 erkannt wurde,
        // nutzen wir einfach alle Kandidaten
        idx_pf1.resize(cand.size());
        for (size_t i = 0; i < cand.size(); ++i)
            idx_pf1[i] = static_cast<int>(i);
    }

    const size_t N = idx_pf1.size();
    if (N == 0) {
        std::cerr << "[LotManager] idx_pf1 empty after fallback\n";
        return false;
    }

    // 3) TOPSIS-Scores fuer PF1 berechnen + nach Score sortieren (absteigend)
//    -> die besten EAs bekommen den hoechsten Lot-Anteil
    TopsisRegime regime = choose_topsis_regime(cushion);

    // Scores fuer alle Kandidaten berechnen
    std::vector<double> scores_all;
    m_mom->compute_topsis_scores(cand, regime, scores_all);

    // Indizes innerhalb PF1 nach Score sortieren
    std::sort(idx_pf1.begin(), idx_pf1.end(),
        [&](int a, int b) {
            return scores_all[a] > scores_all[b];  // absteigend
        });

    // 4) Budget berechnen:
    //    Basis: swarm_size * base_lot * volume_mult
    //    Risiko: linear mit cushion skaliert (0 -> 0, 1 -> volles Budget)
    if (cushion < 0.0) cushion = 0.0;
    if (cushion > 1.0) cushion = 1.0;

    //double base_budget = static_cast<double>(m_cfg.swarm_size)
    //    * m_cfg.base_lot
    //    * m_cfg.volume_mult;   // z.B. 216 * 0.01 * 5 = 10.8
    //double budget_live = base_budget * cushion; // z.B. cushion=0.5 -> 5.4
    // 4) Budget berechnen (quadratisch/parametrisch via budget_exp)
    double scale_B = compute_budget_scale(cushion);   // cushion^budget_exp

    // --- Dynamische Skalierung anhand Live-Balance ---
    double live_balance = get_live_balance();
    double dynamic_mult = m_cfg.volume_mult;

    if (live_balance > 0.1 && m_cfg.ref_capital > 0.1) {
        // Skalierungsfaktor = AktuelleBalance / ReferenzKapital
        double cap_scale = live_balance / m_cfg.ref_capital;
        dynamic_mult = m_cfg.volume_mult * cap_scale;

        std::cout << "[LotManager] Dynamic Sizing: Balance=" << live_balance
            << " Ref=" << m_cfg.ref_capital
            << " -> Scale=" << cap_scale
            << " -> Mult=" << dynamic_mult << "\n";
    }
    else {
        std::cout << "[LotManager] Dynamic Sizing fallback (Bal=" << live_balance
            << "), using default Mult=" << dynamic_mult << "\n";
    }

    double base_budget = static_cast<double>(m_cfg.swarm_size)
        * m_cfg.base_lot
        * dynamic_mult;         // z.B. 10.8 Lot (bei Mult 5.0 und Scale 1.0)

    double budget_live = base_budget * scale_B;       // z.B. cushion=0.5, exp=2 -> scale=0.25 -> 2.7 Lot

    if (budget_live <= 0.0) {
        std::cerr << "[LotManager] budget_live <= 0 -> all lots 0\n";
        std::vector<int> magics_zero;
        std::vector<double> lots_zero;
        magics_zero.reserve(N);
        lots_zero.assign(N, 0.0);
        for (size_t i = 0; i < N; ++i) {
            int idx = idx_pf1[i];
            int paper_magic = cand[idx].ea_magic;
            int live_magic = paper_magic + m_cfg.magic_delta;
            magics_zero.push_back(live_magic);
        }
        return update_lots_in_db(magics_zero, lots_zero);
    }

    // 5) Exponentielle Gewichte ueber Rang 0..N-1
    std::vector<double> w;
    compute_exp_weights(N, m_cfg.gamma_exp, w);
    // w ist normiert: Summe(w_i) = 1

   /* std::cerr << "[LotManager] PF1-Alloc: N=" << N
        << " budget_live=" << budget_live
        << " gamma=" << m_cfg.gamma_exp
        << " cushion=" << cushion << "\n";*/

    // 6) Lots zuweisen: lot_i = budget_live * w_i
    std::vector<int>    magics;
    std::vector<double> lots;
    magics.reserve(N);
    lots.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        int idx = idx_pf1[i];
        int paper_magic = cand[idx].ea_magic;
        int live_magic = paper_magic + m_cfg.magic_delta;

        double lot = budget_live * w[i];

        magics.push_back(live_magic);
        lots.push_back(lot);
    }

    // 7) In DB schreiben
    return update_lots_in_db(magics, lots);
}

double LotManager::compute_budget_scale(double cushion) const
{
    // clamp auf [0,1]
    if (cushion < 0.0) cushion = 0.0;
    if (cushion > 1.0) cushion = 1.0;

    // exponent > 0 absichern
    double exp = (m_cfg.budget_exp > 0.0 ? m_cfg.budget_exp : 1.0);

    // quadratisch / allgemein: scale = cushion^exp
    // exp = 1.0 -> linear
    // exp = 2.0 -> quadratisch
    // exp > 2.0 -> noch konservativer
    return std::pow(cushion, exp);
}

// ---------------------------------------------
// Liest aktuelle Balance (Live, schwarm=0) aus 'balance'-Tabelle
// Retourniert Euro (Cents / 100.0)
// ---------------------------------------------
double LotManager::get_live_balance()
{
    if (!m_db_swarms) return 0.0;

    // Wir suchen den neuesten Eintrag fuer schwarm=0 (Live)
    // Tabelle: balance(schwarm, timestamp, balance_cents)
    // timestamp ist TEXT (ISO8601) oder INTEGER? In MasterLoop.py sah es nach String aus,
    // aber ORDER BY timestamp funktioniert meistens trotzdem lexikographisch korrekt.
    // Sicherheitshalber: ORDER BY timestamp DESC LIMIT 1

    const char* sql = "SELECT balance_cents FROM balance WHERE schwarm=0 ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt* st = nullptr;

    if (sqlite3_prepare_v2(m_db_swarms, sql, -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[LotManager] prepare(get_live_balance) failed: "
            << sqlite3_errmsg(m_db_swarms) << "\n";
        return 0.0;
    }

    double bal_euro = 0.0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        // balance_cents ist INTEGER
        long long cents = sqlite3_column_int64(st, 0);
        bal_euro = static_cast<double>(cents) / 100.0;
    }
    else {
        // Keine Zeile gefunden oder Fehler -> 0.0
        // (Koennte am Anfang passieren, wenn noch keine Balance da ist)
        // std::cerr << "[LotManager] get_live_balance: no row found\n";
    }

    sqlite3_finalize(st);
    return bal_euro;
}
