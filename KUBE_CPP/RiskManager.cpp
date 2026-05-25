//
//#include "RiskManager.h"
//
//#include <cmath>
//#include <cstdio>
//#include <string>
//#include <vector>
//#include <unordered_set>
//
//// ------------------------------------------------------------
//// Logging-Helfer
//// ------------------------------------------------------------
//static inline void log_sql_err(sqlite3* db, const char* where) {
//    std::fprintf(stderr, "[RiskManager][sqlite] %s: %s\n", where, sqlite3_errmsg(db));
//}
//
//// ------------------------------------------------------------
//// ctor
//// ------------------------------------------------------------
//RiskManager::RiskManager(sqlite3* db_schwarm,
//    std::string balance_table,
//    std::string risk_state_table,
//    RiskParams params)
//    : m_db(db_schwarm)
//    , m_tbl_balance(std::move(balance_table))
//    , m_tbl_risk(std::move(risk_state_table))
//    , m_p(params)
//{
//}
//
//// ------------------------------------------------------------
//// Schema-Check für risk_state
//// ------------------------------------------------------------
//bool RiskManager::VerifyRiskStateSchema() {
//    if (!m_db) return false;
//
//    // 1) Existiert die Tabelle?
//    {
//        const char* q =
//            "SELECT 1 FROM sqlite_master "
//            "WHERE type='table' AND name=?1 LIMIT 1;";
//        sqlite3_stmt* st = nullptr;
//        if (sqlite3_prepare_v2(m_db, q, -1, &st, nullptr) != SQLITE_OK) {
//            log_sql_err(m_db, "prepare exists(risk_state)");
//            return false;
//        }
//        sqlite3_bind_text(st, 1, m_tbl_risk.c_str(), -1, SQLITE_TRANSIENT);
//        bool ok = (sqlite3_step(st) == SQLITE_ROW);
//        sqlite3_finalize(st);
//        if (!ok) {
//            std::fprintf(stderr,
//                "[RiskManager] Tabelle '%s' fehlt. Bitte über KUBE_SQLTools anlegen.\n",
//                m_tbl_risk.c_str());
//            return false;
//        }
//    }
//
//    // 2) Spalten prüfen
//    const char* q_info = "PRAGMA table_info(risk_state);";
//    sqlite3_stmt* st = nullptr;
//    if (sqlite3_prepare_v2(m_db, q_info, -1, &st, nullptr) != SQLITE_OK) {
//        log_sql_err(m_db, "prepare PRAGMA table_info(risk_state)");
//        return false;
//    }
//
//    std::unordered_set<std::string> have;
//    while (sqlite3_step(st) == SQLITE_ROW) {
//        const unsigned char* c = sqlite3_column_text(st, 1); // name
//        if (c) have.insert(reinterpret_cast<const char*>(c));
//    }
//    sqlite3_finalize(st);
//
//    const std::vector<std::string> must = {
//        "timestamp",
//        "schwarm",
//        "equity_cents",
//        "peak_cents",
//        "ath_peak_cents",
//        "floor_cents",
//        "range_cents",
//        "sigma_abs",
//        "dd_rel",
//        "lambda_dyn",
//        "cushion",
//        "active_frac"
//    };
//
//    for (const auto& col : must) {
//        if (!have.count(col)) {
//            std::fprintf(stderr,
//                "[RiskManager] Spalte '%s' fehlt in '%s'. Bitte Schema aktualisieren.\n",
//                col.c_str(), m_tbl_risk.c_str());
//            return false;
//        }
//    }
//    return true;
//}
//
//// ------------------------------------------------------------
//// Equity (Balance) aus balance-Tabelle holen
//// ------------------------------------------------------------
//bool RiskManager::fetch_equity_cents(const std::string& ts_key, int schwarm, long& out_cents) {
//    out_cents = 0;
//    std::string sql =
//        "SELECT balance_cents FROM " + m_tbl_balance +
//        " WHERE timestamp=? AND schwarm=? LIMIT 1;";
//
//    sqlite3_stmt* st = nullptr;
//    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
//        log_sql_err(m_db, "prepare fetch_equity");
//        return false;
//    }
//    sqlite3_bind_text(st, 1, ts_key.c_str(), -1, SQLITE_TRANSIENT);
//    sqlite3_bind_int(st, 2, schwarm);
//
//    bool ok = false;
//    if (sqlite3_step(st) == SQLITE_ROW) {
//        out_cents = (long)sqlite3_column_int64(st, 0);
//        ok = true;
//    }
//    sqlite3_finalize(st);
//    return ok;
//}
//
//// ------------------------------------------------------------
//// Letzten risk_state-Datensatz für Schwarm laden (für warm start)
//// ------------------------------------------------------------
//bool RiskManager::load_last_state_from_db(int schwarm, State& st) {
//    std::string sql =
//        "SELECT equity_cents, peak_cents, ath_peak_cents, sigma_abs, dd_rel, lambda_dyn "
//        "FROM " + m_tbl_risk +
//        " WHERE schwarm=? ORDER BY timestamp DESC LIMIT 1;";
//
//    sqlite3_stmt* stt = nullptr;
//    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stt, nullptr) != SQLITE_OK) {
//        log_sql_err(m_db, "prepare load_last_state");
//        return false;
//    }
//    sqlite3_bind_int(stt, 1, schwarm);
//
//    bool ok = false;
//    if (sqlite3_step(stt) == SQLITE_ROW) {
//        st.prev_equity_cents = (long)sqlite3_column_int64(stt, 0);
//        st.prev_peak_cents = (long)sqlite3_column_int64(stt, 1);
//        st.prev_ath_peak_cents = (long)sqlite3_column_int64(stt, 2);
//        st.prev_sigma_abs = sqlite3_column_double(stt, 3);
//        st.prev_dd_rel = sqlite3_column_double(stt, 4);
//        st.prev_lambda_dyn = sqlite3_column_double(stt, 5);
//        st.initialized = true;
//        ok = true;
//    }
//    sqlite3_finalize(stt);
//    return ok;
//}
//
//// ------------------------------------------------------------
//// Hilfsfunktionen
//// ------------------------------------------------------------
//double RiskManager::safe_log_ratio(long curr_cents, long prev_cents) {
//    // vermeidet log(0); benutzt Euro-Skala
//    double c = std::max(1.0, curr_cents / 100.0);
//    double p = std::max(1.0, prev_cents / 100.0);
//    return std::log(c / p);
//}
//
//double RiskManager::clip(double x, double lo, double hi) {
//    return std::max(lo, std::min(hi, x));
//}
//
//// ------------------------------------------------------------
//// Upsert in risk_state
//// ------------------------------------------------------------
//bool RiskManager::upsert_row(const std::string& ts_key, int schwarm,
//    long equity_cents, long peak_cents, long ath_peak_cents,
//    long floor_cents, long range_cents,
//    double sigma_abs, double dd_rel, double lambda_dyn,
//    double cushion, double active_frac)
//{
//    std::string sql =
//        "INSERT OR REPLACE INTO " + m_tbl_risk +
//        "(timestamp,schwarm,"
//        " equity_cents,peak_cents,ath_peak_cents,"
//        " floor_cents,range_cents,"
//        " sigma_abs,dd_rel,lambda_dyn,"
//        " cushion,active_frac)"
//        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?);";
//
//    sqlite3_stmt* st = nullptr;
//    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
//        log_sql_err(m_db, "prepare upsert risk_state");
//        return false;
//    }
//
//    int bi = 1;
//    sqlite3_bind_text(st, bi++, ts_key.c_str(), -1, SQLITE_TRANSIENT);
//    sqlite3_bind_int(st, bi++, schwarm);
//    sqlite3_bind_int64(st, bi++, equity_cents);
//    sqlite3_bind_int64(st, bi++, peak_cents);
//    sqlite3_bind_int64(st, bi++, ath_peak_cents);
//    sqlite3_bind_int64(st, bi++, floor_cents);
//    sqlite3_bind_int64(st, bi++, range_cents);
//    sqlite3_bind_double(st, bi++, sigma_abs);
//    sqlite3_bind_double(st, bi++, dd_rel);
//    sqlite3_bind_double(st, bi++, lambda_dyn);
//    sqlite3_bind_double(st, bi++, cushion);
//    sqlite3_bind_double(st, bi++, active_frac);
//
//    bool ok = (sqlite3_step(st) == SQLITE_DONE);
//    if (!ok) log_sql_err(m_db, "step upsert risk_state");
//    sqlite3_finalize(st);
//    return ok;
//}
//
//// ------------------------------------------------------------
//// Kernfunktion: risk_state für ts_key, schwarm updaten
//// ------------------------------------------------------------
//bool RiskManager::UpsertForTimestamp(const std::string& ts_key, int schwarm)
//{
//    long equity_cents = 0;
//    if (!fetch_equity_cents(ts_key, schwarm, equity_cents)) {
//        std::fprintf(stderr,
//            "[RiskDBG] ts=%s schwarm=%d  -> keine Balance-Zeile gefunden\n",
//            ts_key.c_str(), schwarm);
//        return false;
//    }
//
//    State& st = m_state[schwarm];
//
//    // ---------- Bootstrap-Zweig ----------
//    if (!st.initialized) {
//        const bool had_prev = load_last_state_from_db(schwarm, st);
//        if (!had_prev) {
//            // Erster Datensatz -> Equity als Peak & ATH
//            const long peak_cents = equity_cents;
//            const long ath_peak_cents = equity_cents;
//            const long range_cents = m_p.A_min_cents;  // Minimal-Range
//            const long floor_cents = peak_cents - range_cents;
//
//            // sigma aus Range rückrechnen (nur für Info) oder 0 lassen
//            double sigma_abs = 0.0;
//            if (m_p.gamma > 0.0 && peak_cents > 0) {
//                sigma_abs = std::max(
//                    0.0,
//                    (double)range_cents / (m_p.gamma * (double)peak_cents)
//                );
//            }
//
//            // Kein Drawdown, also dd_rel = 0, lambda_dyn = lambda0
//            double dd_rel = 0.0;
//            double lambda_dyn = m_p.lambda0;
//
//            // Cushion = 1 - dd_rel = 1
//            double cushion = 1.0;
//            double active_frac = cushion;
//
//            std::fprintf(stderr,
//                "[RiskDBG][BOOT] ts=%s schwarm=%d  E=%ld  P=%ld ATH=%ld  A=%ld F=%ld  sigma=%.8f  dd_rel=%.4f  lambda=%.6f  cushion=%.4f\n",
//                ts_key.c_str(), schwarm, equity_cents,
//                peak_cents, ath_peak_cents, range_cents, floor_cents,
//                sigma_abs, dd_rel, lambda_dyn, cushion);
//
//            if (!upsert_row(ts_key, schwarm,
//                equity_cents, peak_cents, ath_peak_cents,
//                floor_cents, range_cents,
//                sigma_abs, dd_rel, lambda_dyn,
//                cushion, active_frac))
//            {
//                std::fprintf(stderr, "[RiskDBG][BOOT] upsert_row FAILED\n");
//                return false;
//            }
//
//            st.prev_equity_cents = equity_cents;
//            st.prev_peak_cents = peak_cents;
//            st.prev_ath_peak_cents = ath_peak_cents;
//            st.prev_sigma_abs = sigma_abs;
//            st.prev_dd_rel = dd_rel;
//            st.prev_lambda_dyn = lambda_dyn;
//            st.initialized = true;
//
//            std::fprintf(stderr, "[RiskDBG][BOOT] INIT DONE\n");
//            return true;
//        }
//
//        // Wir haben historischen Zustand geladen:
//        st.initialized = true;
//        std::fprintf(stderr,
//            "[RiskDBG] ts=%s schwarm=%d  load_last_state: prev_E=%ld prev_P=%ld prev_ATH=%ld prev_sigma=%.8f prev_dd=%.4f prev_lambda=%.6f\n",
//            ts_key.c_str(), schwarm,
//            st.prev_equity_cents, st.prev_peak_cents, st.prev_ath_peak_cents,
//            st.prev_sigma_abs, st.prev_dd_rel, st.prev_lambda_dyn);
//    }
//
//    // ---------- Normalbetrieb ----------
//
//    const long prev_peak = (st.prev_peak_cents > 0 ? st.prev_peak_cents : equity_cents);
//    const long prev_ath_peak = (st.prev_ath_peak_cents > 0 ? st.prev_ath_peak_cents : prev_peak);
//
//    // 1) Drawdown vs. ATH (unverändert)
//    double dd_raw = 0.0;
//    if (prev_ath_peak > 0 && equity_cents < prev_ath_peak) {
//        dd_raw = (double)(prev_ath_peak - equity_cents) / (double)prev_ath_peak;
//    }
//    dd_raw = clip(dd_raw, 0.0, 1.0);
//
//    // 2) Asymmetrische Glättung: schneller hoch, langsam runter
//    double beta_up = m_p.dd_beta_up;
//    double beta_down = m_p.dd_beta_down;
//
//    double dd_rel;
//    if (dd_raw > st.prev_dd_rel) {
//        // Drawdown verschlechtert sich -> schnell anpassen
//        dd_rel = (1.0 - beta_up) * st.prev_dd_rel + beta_up * dd_raw;
//    }
//    else {
//        // Drawdown verbessert sich -> langsam „vergessen“
//        dd_rel = (1.0 - beta_down) * st.prev_dd_rel + beta_down * dd_raw;
//    }
//
//    dd_rel = clip(dd_rel, 0.0, 1.0);
//
//    // 3) Dynamisches Lambda: lambda = floor + (lambda0-floor)*exp(-a * dd_rel)
//    double lambda_dyn = m_p.lambda_floor +
//        (m_p.lambda0 - m_p.lambda_floor) * std::exp(-m_p.lambda_decay_a * dd_rel);
//
//    // 4) Peak-Update:
//    //    - Wenn Equity ein neues Hoch macht (E > prev_peak), soll Peak 1:1 folgen.
//    //    - Sonst "leaky" mit lambda_dyn.
//    long peak_cents = prev_peak;
//    if (equity_cents > prev_peak) {
//        peak_cents = equity_cents;
//    }
//    else {
//        double mixed = (1.0 - lambda_dyn) * (double)prev_peak
//            + lambda_dyn * (double)equity_cents;
//        peak_cents = (long)std::llround(std::max<double>((double)equity_cents, mixed));
//    }
//
//    // 5) ATH über Peak
//    long ath_peak_cents = prev_ath_peak;
//    if (peak_cents > ath_peak_cents)
//        ath_peak_cents = peak_cents;
//
//    // 6) Volatilität (EWMA der |log-Returns|)
//    const double r_abs = RiskManager::safe_log_ratio(equity_cents, st.prev_equity_cents);
//    const double sigma = m_p.alpha * std::fabs(r_abs)
//        + (1.0 - m_p.alpha) * st.prev_sigma_abs;
//
//    // 7) Range (vor Clamp) und Clamping
//    const long range_raw = (long)std::llround(m_p.gamma * sigma * (double)peak_cents);
//    long       range_cents = range_raw;
//    if (range_cents < m_p.A_min_cents)  range_cents = m_p.A_min_cents;
//    if (range_cents > m_p.A_max_cents)  range_cents = m_p.A_max_cents;
//
//    // 8) Floor
//    const long floor_cents = peak_cents - range_cents;
//
//    // 9) Cushion: aus dd_rel (1 - DD) statt aus (E-Floor)/Range
//    double cushion = 1.0 - dd_rel;
//    cushion = clip(cushion, 0.0, 1.0);
//
//    // Optional: active_frac = cushion (oder separates Modell)
//    double active_frac = cushion;
//
//    std::fprintf(stderr,
//        "[RiskDBG] ts=%s schwarm=%d  E=%ld  P_prev=%ld  P=%ld  ATH_prev=%ld ATH=%ld  dd_raw=%.4f dd_rel=%.4f lambda=%.6f  r_abs=%.8f sigma=%.8f  range_raw=%ld A=%ld(F=%ld)  cushion=%.4f act=%.4f\n",
//        ts_key.c_str(), schwarm,
//        equity_cents, prev_peak, peak_cents,
//        prev_ath_peak, ath_peak_cents,
//        dd_raw, dd_rel, lambda_dyn,
//        r_abs, sigma, range_raw, range_cents, floor_cents,
//        cushion, active_frac);
//
//    // 10) in DB schreiben
//    if (!upsert_row(ts_key, schwarm,
//        equity_cents, peak_cents, ath_peak_cents,
//        floor_cents, range_cents,
//        sigma, dd_rel, lambda_dyn,
//        cushion, active_frac))
//    {
//        std::fprintf(stderr, "[RiskDBG] upsert_row FAILED\n");
//        return false;
//    }
//
//    // 11) State aktualisieren
//    st.prev_equity_cents = equity_cents;
//    st.prev_peak_cents = peak_cents;
//    st.prev_ath_peak_cents = ath_peak_cents;
//    st.prev_sigma_abs = sigma;
//    st.prev_dd_rel = dd_rel;
//    st.prev_lambda_dyn = lambda_dyn;
//
//    return true;
//}
#include "RiskManager.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_set>

static inline void log_sql_err(sqlite3* db, const char* where)
{
    std::fprintf(stderr, "[RiskManager][sqlite] %s: %s\n",
        where, sqlite3_errmsg(db));
}

RiskManager::RiskManager(sqlite3* db_schwarm,
    std::string balance_table,
    std::string risk_state_table,
    RiskParams params)
    : m_db(db_schwarm)
    , m_tbl_balance(std::move(balance_table))
    , m_tbl_risk(std::move(risk_state_table))
    , m_p(params)
{
}

// -----------------------------------------------------------------------------
// Schema-Prüfung
// -----------------------------------------------------------------------------
bool RiskManager::VerifyRiskStateSchema()
{
    if (!m_db)
        return false;

    // 1) Tabelle vorhanden?
    {
        const char* q =
            "SELECT 1 FROM sqlite_master "
            "WHERE type='table' AND name=?1 LIMIT 1;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, q, -1, &st, nullptr) != SQLITE_OK) {
            log_sql_err(m_db, "prepare exists(risk_state)");
            return false;
        }
        sqlite3_bind_text(st, 1, m_tbl_risk.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = (sqlite3_step(st) == SQLITE_ROW);
        sqlite3_finalize(st);
        if (!ok) {
            std::fprintf(stderr,
                "[RiskManager] Tabelle '%s' fehlt. Bitte über KUBE_SQLTools anlegen.\n",
                m_tbl_risk.c_str());
            return false;
        }
    }

    // 2) Spalten prüfen
    const char* q_info = "PRAGMA table_info(risk_state);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, q_info, -1, &st, nullptr) != SQLITE_OK) {
        log_sql_err(m_db, "prepare PRAGMA table_info(risk_state)");
        return false;
    }

    std::unordered_set<std::string> have;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* c = sqlite3_column_text(st, 1); // name
        if (c)
            have.insert(reinterpret_cast<const char*>(c));
    }
    sqlite3_finalize(st);

    const std::vector<std::string> must = {
        "timestamp",
        "schwarm",
        "equity_cents",
        "peak_cents",
        "ath_peak_cents",
        "floor_cents",
        "range_cents",
        "sigma_abs",
        "dd_rel",
        "lambda_dyn",
        "cushion",
        "active_frac"
    };

    for (const auto& col : must) {
        if (!have.count(col)) {
            std::fprintf(stderr,
                "[RiskManager] Spalte '%s' fehlt in '%s'. Bitte Schema aktualisieren.\n",
                col.c_str(), m_tbl_risk.c_str());
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// Hilfsfunktionen DB
// -----------------------------------------------------------------------------
bool RiskManager::fetch_equity_cents(const std::string& ts_key,
    int                schwarm,
    long& out_cents)
{
    out_cents = 0;
    std::string sql =
        "SELECT balance_cents FROM " + m_tbl_balance +
        " WHERE timestamp=? AND schwarm=? LIMIT 1;";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        log_sql_err(m_db, "prepare fetch_equity");
        return false;
    }

    sqlite3_bind_text(st, 1, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, schwarm);

    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out_cents = (long)sqlite3_column_int64(st, 0);
        ok = true;
    }
    sqlite3_finalize(st);
    return ok;
}

bool RiskManager::load_last_state_from_db(int schwarm, State& st)
{
    std::string sql =
        "SELECT equity_cents, peak_cents, ath_peak_cents, sigma_abs, dd_rel, lambda_dyn "
        "FROM " + m_tbl_risk +
        " WHERE schwarm=? ORDER BY timestamp DESC LIMIT 1;";

    sqlite3_stmt* stt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stt, nullptr) != SQLITE_OK) {
        log_sql_err(m_db, "prepare load_last_state");
        return false;
    }
    sqlite3_bind_int(stt, 1, schwarm);

    bool ok = false;
    if (sqlite3_step(stt) == SQLITE_ROW) {
        st.prev_equity_cents = (long)sqlite3_column_int64(stt, 0);
        st.prev_peak_cents = (long)sqlite3_column_int64(stt, 1);
        st.prev_ath_peak_cents = (long)sqlite3_column_int64(stt, 2);
        st.prev_sigma_abs = sqlite3_column_double(stt, 3);
        st.prev_dd_rel = sqlite3_column_double(stt, 4);
        st.prev_lambda_dyn = sqlite3_column_double(stt, 5);
        st.initialized = true;
        ok = true;
    }
    sqlite3_finalize(stt);
    return ok;
}

// -----------------------------------------------------------------------------
// Mathematische Helfer
// -----------------------------------------------------------------------------
double RiskManager::safe_log_ratio(long curr_cents, long prev_cents)
{
    double c = std::max(1.0, curr_cents / 100.0);
    double p = std::max(1.0, prev_cents / 100.0);
    return std::log(c / p);
}

double RiskManager::clip(double x, double lo, double hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// -----------------------------------------------------------------------------
// Upsert
// -----------------------------------------------------------------------------
bool RiskManager::upsert_row(const std::string& ts_key,
    int               schwarm,
    long              equity_cents,
    long              peak_cents,
    long              ath_peak_cents,
    long              floor_cents,
    long              range_cents,
    double            sigma_abs,
    double            dd_rel,
    double            lambda_dyn,
    double            cushion,
    double            active_frac)
{
    std::string sql =
        "INSERT OR REPLACE INTO " + m_tbl_risk +
        "(timestamp,schwarm,equity_cents,peak_cents,ath_peak_cents,"
        " floor_cents,range_cents,sigma_abs,dd_rel,lambda_dyn,cushion,active_frac)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        log_sql_err(m_db, "prepare upsert risk_state");
        return false;
    }

    sqlite3_bind_text(st, 1, ts_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, schwarm);
    sqlite3_bind_int64(st, 3, equity_cents);
    sqlite3_bind_int64(st, 4, peak_cents);
    sqlite3_bind_int64(st, 5, ath_peak_cents);
    sqlite3_bind_int64(st, 6, floor_cents);
    sqlite3_bind_int64(st, 7, range_cents);
    sqlite3_bind_double(st, 8, sigma_abs);
    sqlite3_bind_double(st, 9, dd_rel);
    sqlite3_bind_double(st, 10, lambda_dyn);
    sqlite3_bind_double(st, 11, cushion);
    sqlite3_bind_double(st, 12, active_frac);

    bool ok = (sqlite3_step(st) == SQLITE_DONE);
    if (!ok)
        log_sql_err(m_db, "step upsert risk_state");
    sqlite3_finalize(st);
    return ok;
}

// -----------------------------------------------------------------------------
// Hauptlogik
// -----------------------------------------------------------------------------

bool RiskManager::UpsertForTimestamp(const std::string& ts_key, int schwarm)
{
    long equity_cents = 0;
    if (!fetch_equity_cents(ts_key, schwarm, equity_cents)) {
        std::fprintf(stderr,
            "[RiskDBG] ts=%s schwarm=%d -> keine Balance-Zeile gefunden\n",
            ts_key.c_str(), schwarm);
        return false;
    }

    // Parameter einmal loggen
    /*std::fprintf(stderr,
        "[RiskDBG][PARAMS] ts=%s schwarm=%d lambda0=%.6f lambda_floor=%.6f a=%.3f "
        "dd_beta_up=%.3f dd_beta_down=%.3f alpha=%.4f gamma=%.3f A_min=%ld A_max=%ld\n",
        ts_key.c_str(), schwarm,
        m_p.lambda0, m_p.lambda_floor, m_p.lambda_decay_a,
        m_p.dd_beta_up, m_p.dd_beta_down,
        m_p.alpha, m_p.gamma,
        m_p.A_min_cents, m_p.A_max_cents
    );*/

    State& st = m_state[schwarm];

    // ---------------- Bootstrap ----------------
    if (!st.initialized) {
        std::fprintf(stderr,
            "[RiskDBG][BOOT] ts=%s schwarm=%d INIT-START (prev_initial=false)\n",
            ts_key.c_str(), schwarm);

        const bool had_prev = load_last_state_from_db(schwarm, st);
        std::fprintf(stderr,
            "[RiskDBG][BOOT] ts=%s schwarm=%d load_last_state_from_db -> had_prev=%d "
            "(prev_E=%ld prev_P=%ld prev_ATH=%ld prev_dd_rel=%.6f prev_lambda=%.6f)\n",
            ts_key.c_str(), schwarm, (int)had_prev,
            st.prev_equity_cents, st.prev_peak_cents, st.prev_ath_peak_cents,
            st.prev_dd_rel, st.prev_lambda_dyn);

        if (!had_prev) {
            const long peak_cents = equity_cents;
            const long ath_peak_cents = equity_cents;

            const long range_cents = m_p.A_min_cents;
            const long floor_cents = peak_cents - range_cents;

            const double sigma_abs = 0.0;
            const double dd_rel = 0.0;
            const double lambda_dyn = m_p.lambda0;

            const double cushion = (range_cents > 0)
                ? clip((double)(equity_cents - floor_cents) /
                    (double)range_cents, 0.0, 1.0)
                : 0.0;
            const double active_frac = cushion;

            std::fprintf(stderr,
                "[RiskDBG][BOOT] ts=%s schwarm=%d E=%ld P=%ld ATH=%ld range=%ld floor=%ld "
                "sigma=%.6f dd_rel=%.6f lambda_dyn=%.6f cushion=%.4f act=%.4f\n",
                ts_key.c_str(), schwarm,
                equity_cents, peak_cents, ath_peak_cents,
                range_cents, floor_cents,
                sigma_abs, dd_rel, lambda_dyn,
                cushion, active_frac);

            if (!upsert_row(ts_key, schwarm,
                equity_cents, peak_cents, ath_peak_cents,
                floor_cents, range_cents,
                sigma_abs, dd_rel, lambda_dyn,
                cushion, active_frac))
            {
                std::fprintf(stderr, "[RiskDBG][BOOT] upsert_row FAILED\n");
                return false;
            }

            st.prev_equity_cents = equity_cents;
            st.prev_peak_cents = peak_cents;
            st.prev_ath_peak_cents = ath_peak_cents;
            st.prev_sigma_abs = sigma_abs;
            st.prev_dd_rel = dd_rel;
            st.prev_lambda_dyn = lambda_dyn;
            st.initialized = true;

            std::fprintf(stderr,
                "[RiskDBG][BOOT] ts=%s schwarm=%d INIT DONE (prev_E=%ld prev_P=%ld prev_ATH=%ld)\n",
                ts_key.c_str(), schwarm,
                st.prev_equity_cents, st.prev_peak_cents, st.prev_ath_peak_cents);
            return true;
        }

        // wir haben historischen Zustand geladen
        st.initialized = true;
        std::fprintf(stderr,
            "[RiskDBG] ts=%s schwarm=%d load_last_state: prev_E=%ld prev_P=%ld prev_ATH=%ld "
            "prev_dd_rel=%.6f prev_lambda=%.6f prev_sigma=%.8f\n",
            ts_key.c_str(), schwarm,
            st.prev_equity_cents, st.prev_peak_cents, st.prev_ath_peak_cents,
            st.prev_dd_rel, st.prev_lambda_dyn, st.prev_sigma_abs);
    }

    // ---------------- Normalbetrieb ----------------

    std::fprintf(stderr,
        "[RiskDBG][STEP0] ts=%s schwarm=%d E_curr=%ld prev_E=%ld prev_P=%ld prev_ATH=%ld "
        "prev_dd_rel=%.6f prev_lambda=%.6f prev_sigma=%.8f\n",
        ts_key.c_str(), schwarm,
        equity_cents,
        st.prev_equity_cents,
        st.prev_peak_cents,
        st.prev_ath_peak_cents,
        st.prev_dd_rel,
        st.prev_lambda_dyn,
        st.prev_sigma_abs);

    // 1) ATH-Peak aktualisieren
    long ath_peak_cents = st.prev_ath_peak_cents;
    if (ath_peak_cents <= 0) {
        ath_peak_cents = (st.prev_peak_cents > 0
            ? st.prev_peak_cents
            : equity_cents);
        std::fprintf(stderr,
            "[RiskDBG][ATH] ts=%s schwarm=%d ATH init from prev_peak/equity: ATH=%ld\n",
            ts_key.c_str(), schwarm, ath_peak_cents);
    }
    if (st.prev_peak_cents > ath_peak_cents) {
        std::fprintf(stderr,
            "[RiskDBG][ATH] ts=%s schwarm=%d prev_peak(%ld) > old_ATH(%ld) -> update ATH\n",
            ts_key.c_str(), schwarm,
            st.prev_peak_cents, ath_peak_cents);
        ath_peak_cents = st.prev_peak_cents;
    }

    std::fprintf(stderr,
        "[RiskDBG][ATH] ts=%s schwarm=%d ATH_FINAL=%ld\n",
        ts_key.c_str(), schwarm, ath_peak_cents);

    // 2) instantaner relativer Drawdown zum ATH
    double dd_inst = 0.0;
    if (ath_peak_cents > 0 && equity_cents < ath_peak_cents) {
        dd_inst = (double)(ath_peak_cents - equity_cents) /
            (double)ath_peak_cents; // 0..1
        dd_inst = clip(dd_inst, 0.0, 1.0);
    }

    std::fprintf(stderr,
        "[RiskDBG][DD] ts=%s schwarm=%d dd_inst_raw=%.6f (ATH=%ld, E=%ld)\n",
        ts_key.c_str(), schwarm,
        dd_inst, ath_peak_cents, equity_cents);

    // 3) asymmetrisches EWMA auf dd_rel
    double dd_prev = st.prev_dd_rel;
    double beta = (dd_inst > dd_prev) ? m_p.dd_beta_up : m_p.dd_beta_down;
    double beta_clamped = clip(beta, 0.0, 1.0);

    double dd_rel = dd_prev + beta_clamped * (dd_inst - dd_prev);
    dd_rel = clip(dd_rel, 0.0, 1.0);

    std::fprintf(stderr,
        "[RiskDBG][DD] ts=%s schwarm=%d dd_prev=%.6f dd_inst=%.6f beta=%.6f beta_clamped=%.6f dd_rel=%.6f\n",
        ts_key.c_str(), schwarm,
        dd_prev, dd_inst, beta, beta_clamped, dd_rel);

    // 4) dynamisches Lambda aus dd_rel
    const double lambda0 = m_p.lambda0;
    const double lambda_floor = m_p.lambda_floor;
    const double a = m_p.lambda_decay_a;

    double lambda_dyn_raw =
        lambda_floor +
        (lambda0 - lambda_floor) * std::exp(-a * dd_rel);

    double lambda_dyn = lambda_dyn_raw;
    if (lambda_dyn < lambda_floor) lambda_dyn = lambda_floor;
    if (lambda_dyn > lambda0)      lambda_dyn = lambda0;

    std::fprintf(stderr,
        "[RiskDBG][LAMBDA] ts=%s schwarm=%d lambda_raw=%.8f lambda_clamped=%.8f "
        "(lambda0=%.6f floor=%.6f a=%.3f dd_rel=%.6f)\n",
        ts_key.c_str(), schwarm,
        lambda_dyn_raw, lambda_dyn,
        lambda0, lambda_floor, a, dd_rel);

    // 5) Volatilität (sigma_abs) als EWMA der log-Returns
    const double r_abs = safe_log_ratio(equity_cents, st.prev_equity_cents);
    const double sigma =
        m_p.alpha * std::fabs(r_abs) +
        (1.0 - m_p.alpha) * st.prev_sigma_abs;

    std::fprintf(stderr,
        "[RiskDBG][SIGMA] ts=%s schwarm=%d r_abs=%.8f alpha=%.4f sigma_prev=%.8f sigma_new=%.8f\n",
        ts_key.c_str(), schwarm,
        r_abs, m_p.alpha, st.prev_sigma_abs, sigma);

    // 6) Peak mit dynamischem Lambda
    const long prev_peak = (st.prev_peak_cents > 0)
        ? st.prev_peak_cents
        : equity_cents;

    const double peak_raw = std::max<double>(
        (double)equity_cents,
        (1.0 - lambda_dyn) * (double)prev_peak +
        lambda_dyn * (double)equity_cents);

    const long peak_cents = (long)std::llround(peak_raw);

    std::fprintf(stderr,
        "[RiskDBG][PEAK] ts=%s schwarm=%d prev_P=%ld E=%ld lam=%.8f peak_raw=%.4f P_new=%ld\n",
        ts_key.c_str(), schwarm,
        prev_peak, equity_cents, lambda_dyn,
        peak_raw, peak_cents);

    // 7) Range & Floor
    const long range_raw = (long)std::llround(m_p.gamma * sigma *
        (double)peak_cents);
    long range_cents = range_raw;
    if (range_cents < m_p.A_min_cents) range_cents = m_p.A_min_cents;
    if (range_cents > m_p.A_max_cents) range_cents = m_p.A_max_cents;

    const long floor_cents = peak_cents - range_cents;

    const double cushion = (range_cents > 0)
        ? clip((double)(equity_cents - floor_cents) /
            (double)range_cents, 0.0, 1.0)
        : 0.0;

    const double active_frac = cushion;

    std::fprintf(stderr,
        "[RiskDBG][RANGE] ts=%s schwarm=%d range_raw=%ld range=%ld floor=%ld "
        "cushion=%.4f act=%.4f\n",
        ts_key.c_str(), schwarm,
        range_raw, range_cents, floor_cents,
        cushion, active_frac);

    // 8) Gesamtes Debug-Log in einer Zeile (zusammengefasst)
    std::fprintf(stderr,
        "[RiskDBG][SUMMARY] ts=%s schwarm=%d E=%ld P_prev=%ld P=%ld ATH=%ld "
        "dd_inst=%.4f dd_rel=%.4f lam=%.6f sigma=%.8f A=%ld floor=%ld cushion=%.4f act=%.4f\n",
        ts_key.c_str(), schwarm,
        equity_cents, prev_peak, peak_cents, ath_peak_cents,
        dd_inst, dd_rel, lambda_dyn, sigma,
        range_cents, floor_cents,
        cushion, active_frac);

    // 9) DB upsert
    if (!upsert_row(ts_key, schwarm,
        equity_cents, peak_cents, ath_peak_cents,
        floor_cents, range_cents,
        sigma, dd_rel, lambda_dyn,
        cushion, active_frac))
    {
        std::fprintf(stderr, "[RiskDBG] upsert_row FAILED\n");
        return false;
    }

    // 10) State aktualisieren
    st.prev_equity_cents = equity_cents;
    st.prev_peak_cents = peak_cents;
    st.prev_ath_peak_cents = ath_peak_cents;
    st.prev_sigma_abs = sigma;
    st.prev_dd_rel = dd_rel;
    st.prev_lambda_dyn = lambda_dyn;

    std::fprintf(stderr,
        "[RiskDBG][STATE] ts=%s schwarm=%d NEW_STATE: prev_E=%ld prev_P=%ld prev_ATH=%ld "
        "prev_dd_rel=%.6f prev_lambda=%.8f prev_sigma=%.8f\n",
        ts_key.c_str(), schwarm,
        st.prev_equity_cents, st.prev_peak_cents, st.prev_ath_peak_cents,
        st.prev_dd_rel, st.prev_lambda_dyn, st.prev_sigma_abs);

    return true;
}
