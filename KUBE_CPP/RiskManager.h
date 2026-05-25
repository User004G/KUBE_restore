//
//#pragma once
//#include <string>
//#include <unordered_map>
//#include <sqlite3.h>
//
//struct RiskParams {
//    double lambda0 = 0.004;
//    double alpha = 0.03;
//    double gamma = 12.0;
//    long   A_min_cents = 50000;
//    long   A_max_cents = 1500000;
//
//    // asymmetrische Glättung
//    double dd_beta_up = 0.10;  // schneller hoch bei schlechterem DD
//    double dd_beta_down = 0.02;  // langsam runter bei Erholung
//
//    double lambda_floor = 0.0;
//    double lambda_decay_a = 5.0;
//};
//
//class RiskManager {
//public:
//    RiskManager(sqlite3* db_schwarm,
//        std::string balance_table = "balance",
//        std::string risk_state_table = "risk_state",
//        RiskParams params = {});
//
//    // Prüft, ob risk_state die erwarteten Spalten hat (kein CREATE!)
//    bool VerifyRiskStateSchema();
//
//    // Berechnet und upsertet risk_state für (ts_key, schwarm).
//    // false, wenn keine Balance-Zeile existiert oder SQL-Fehler.
//    bool UpsertForTimestamp(const std::string& ts_key, int schwarm);
//
//private:
//    struct State {
//        long   prev_equity_cents = 0;
//        long   prev_peak_cents = 0;
//        long   prev_ath_peak_cents = 0;
//        double prev_sigma_abs = 0.0;
//        double prev_dd_rel = 0.0;
//        double prev_lambda_dyn = 0.0;
//        bool   initialized = false;
//    };
//
//    sqlite3* m_db = nullptr;
//    std::string m_tbl_balance;
//    std::string m_tbl_risk;
//    RiskParams  m_p;
//
//    // Zwischenspeicher pro Schwarm
//    std::unordered_map<int, State> m_state;
//
//    // Hilfen
//    bool  fetch_equity_cents(const std::string& ts_key, int schwarm, long& out_cents);
//    bool  load_last_state_from_db(int schwarm, State& st);
//    bool  upsert_row(const std::string& ts_key, int schwarm,
//        long equity_cents, long peak_cents, long ath_peak_cents,
//        long floor_cents, long range_cents,
//        double sigma_abs, double dd_rel, double lambda_dyn,
//        double cushion, double active_frac);
//
//    static double safe_log_ratio(long curr_cents, long prev_cents);
//    static double clip(double x, double lo, double hi);
//};
#pragma once
#include <string>
#include <unordered_map>
#include <sqlite3.h>

// Parameter des RiskManagers
struct RiskParams
{
    // Maximaler Lambda-Wert (bei dd_rel = 0)
    double lambda0 = 0.00001;

    // EWMA-Glättung für Volatilität (|log(E_t/E_{t-1})|)
    double alpha = 0.03;

    // Range: A_t = clamp( gamma * sigma_t * Peak_t, [A_min, A_max] )
    /*double gamma = 12.0;*/
    double gamma = 0.0; 
    long   A_min_cents = 50000;     // 500 €
    //long   A_max_cents = 1500000;   // 15 000 €
    long   A_max_cents = 50000;   // 15 000 €

    // Asymmetrische Glättung des relativen Drawdowns dd_rel (0..1)
    //   dd_inst > dd_prev  -> "schlechter":  dd_beta_up  (schneller hoch)
    //   dd_inst < dd_prev  -> "besser"  :  dd_beta_down (langsamer runter)
    double dd_beta_up = 0.8;
    double dd_beta_down = 0.8;

    // Minimaler Lambda-Wert (bei hohem DD)
    double lambda_floor = 0.0;

    // Steilheit der Lambda-Kurve:
    // lambda_dyn = lambda_floor + (lambda0 - lambda_floor) * exp(-a * dd_rel)
    double lambda_decay_a = 5.0;
};

class RiskManager
{
public:
    RiskManager(sqlite3* db_schwarm,
        std::string balance_table = "balance",
        std::string risk_state_table = "risk_state",
        RiskParams params = {});

    // Prüft, ob risk_state die erwarteten Spalten hat (kein CREATE!)
    bool VerifyRiskStateSchema();

    // Berechnet und upsertet risk_state für (ts_key, schwarm).
    // false, wenn keine Balance-Zeile existiert oder SQL-Fehler.
    bool UpsertForTimestamp(const std::string& ts_key, int schwarm);

private:
    struct State
    {
        long   prev_equity_cents = 0;
        long   prev_peak_cents = 0;
        long   prev_ath_peak_cents = 0;
        double prev_sigma_abs = 0.0;
        double prev_dd_rel = 0.0;
        double prev_lambda_dyn = 0.0;
        bool   initialized = false;
    };

    sqlite3* m_db = nullptr;
    std::string m_tbl_balance;
    std::string m_tbl_risk;
    RiskParams  m_p;

    // Zwischenspeicher pro Schwarm
    std::unordered_map<int, State> m_state;

    // Hilfsfunktionen
    bool  fetch_equity_cents(const std::string& ts_key, int schwarm, long& out_cents);
    bool  load_last_state_from_db(int schwarm, State& st);

    bool  upsert_row(const std::string& ts_key, int schwarm,
        long equity_cents, long peak_cents, long ath_peak_cents,
        long floor_cents, long range_cents,
        double sigma_abs, double dd_rel, double lambda_dyn,
        double cushion, double active_frac);

    static double safe_log_ratio(long curr_cents, long prev_cents);
    static double clip(double x, double lo, double hi);
};
