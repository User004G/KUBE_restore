#pragma once
#ifndef LOT_MANAGER_H
#define LOT_MANAGER_H

#include <string>
#include <vector>

// Forward declarations
struct sqlite3;
class MultiObjectiveManager;
struct Candidate;
struct TopsisRegime;

// Konfiguration fuer den LotManager
struct LotManagerConfig
{
    int    swarm_size = 216;   // Anzahl EAs im Schwarm
    double base_lot = 0.01;  // Basis-Lot pro EA (Paper-Referenz)
    double volume_mult = 10.0;   // Multiplikator vm (z.B. 5)
    double gamma_exp = 0.1;   // Gamma fuer exponentielle Degression
    double min_lot = 0.01;  // Broker-MinLot
    double max_lot = 100.0;  // optionaler Deckel; 0.0 => kein Limit
    int    magic_delta = -10000; // z.B. 30173 (Paper) → 20173 (Live)
    double budget_exp = 2.0;   // 2.0 = quadratisch, 1.0 = linear, 3.0 = "super-konservativ"
    double ref_capital = 10000.0; // Referenzkapital fuer Skalierung (Live)
};

// LotManager: nutzt MultiObjectiveManager fuer PF1 + TOPSIS
// und schreibt die Lot-Groessen in die swarms-Tabelle (Spalte "lot").
class LotManager
{
public:
    LotManager(sqlite3* db_swarms,
        MultiObjectiveManager* mom,
        const std::string& swarms_table,
        const LotManagerConfig& cfg);

    // Haupt-API:
    //
    //  - swarm_id: welcher Paperschwarm (z.B. 10)
    //  - ts_key  : Timestamp-String, fuer den FitnessproBar schon gefuellt ist
    //  - cushion : relativer Puffer [0..1]
    //  - active_magics: optional Liste aktiver EA-Magics.
    //    Wenn nullptr oder leer: es werden alle PF1-Kandidaten verwendet.
    //
    // Rueckgabe: true bei Erfolg, false bei Fehler.
    bool assign_lots_for_ts(int                     swarm_id,
        const std::string& ts_key,
        double                  cushion,
        const std::vector<int>* active_magics = nullptr);

private:
    sqlite3* m_db_swarms;
    MultiObjectiveManager* m_mom;
    std::string           m_swarms_table;
    LotManagerConfig      m_cfg;

    // Liest aktuelle Balance (Live, schwarm=0) aus 'balance'-Tabelle
    // Retourniert Euro (Cents / 100.0)
    double get_live_balance();

    // Budget-Faktor aus cushion (hier linear, leicht austauschbar)
    //double compute_budget_scale(double cushion) const;

    // Regime-Auswahl fuer TOPSIS (DEFENSIV / KONSERVATIV / NORMAL / EXPANSION)
    TopsisRegime choose_topsis_regime(double cushion) const;

    // Exponentielle Gewichte fuer N EAs (Rang 0..N-1)
    void compute_exp_weights(size_t N,
        double gamma,
        std::vector<double>& out_weights) const;

    // Lots in swarms-Tabelle schreiben (im SQL-Transaction)
    bool update_lots_in_db(const std::vector<int>& magics,
        const std::vector<double>& lots);
    double compute_budget_scale(double cushion) const;
};

#endif // LOT_MANAGER_H

