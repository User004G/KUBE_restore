#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include <random>

// Vorwärtsdeklarationen
class MutationsManager;
class IMutationStrategy;

// -------------------------------------------------------
// I. Datenstrukturen und Kontext
// -------------------------------------------------------

// EA-Parameter-Struktur (unverändert)
struct EAParams {
    int magic;
    int param1;
    int param2;
    int param3;
    double performance_metric; 
};

// Mutationskontext-Struktur (unverändert)
struct MutationContext {
    std::string current_timestamp;
    double current_volatility;
    double system_equity;
    int param1_min, param1_max;
    int param2_min, param2_max;
    int param3_min, param3_max;
};

// -------------------------------------------------------
// II. I_MutationStrategy (Abstrakte Basisklasse für Strategien)
// Unverändert. Die konkreten Strategien (z.B. RandomMutationStrategy) 
// sind jetzt intern in den Zustandsklassen definiert oder in einer separaten Datei.
// -------------------------------------------------------
class IMutationStrategy {
public:
    virtual ~IMutationStrategy() = default;
    virtual std::vector<EAParams> select_eas(sqlite3* db_schwarm, 
                                             int swarm_live_id, 
                                             const MutationContext& context) = 0;
    virtual EAParams generate_new_params(const EAParams& old_params, 
                                         const MutationContext& context) = 0;
    virtual int get_mutation_scope() const = 0; // Neu: Definiert den Umfang (z.B. Anzahl EAs)
};

// Beispiel für eine konkrete Strategie
class RandomMutationStrategy : public IMutationStrategy {
public:
    RandomMutationStrategy(int scope) : scope_(scope) {}
    std::vector<EAParams> select_eas(sqlite3* db_schwarm, int swarm_live_id, const MutationContext& context) override;
    EAParams generate_new_params(const EAParams& old_params, const MutationContext& context) override;
    int get_mutation_scope() const override { return scope_; }
private:
    int scope_;
};


// -------------------------------------------------------
// III. I_MutationState (Abstrakte Basisklasse für Zustände)
// -------------------------------------------------------
class IMutationState {
public:
    virtual ~IMutationState() = default;

    // Definiert den Mutationszyklus für diesen Zustand (z.B. 120 Ticks)
    virtual int get_cycle_length() const = 0; 
    
    // Wählt die spezifische Mutationsstrategie für diesen Zustand
    virtual std::unique_ptr<IMutationStrategy> get_strategy(const MutationContext& context) const = 0;

    // Methode zum Ändern des Zustands im Manager (Context)
    virtual void transition_to(MutationsManager* manager, const std::string& ts) const = 0;
    
    virtual std::string get_name() const = 0;
};


// -------------------------------------------------------
// IV. Konkrete Zustände
// -------------------------------------------------------

class NormalState : public IMutationState {
public:
    int get_cycle_length() const override { return 120; } // Langsamer Zyklus
    std::unique_ptr<IMutationStrategy> get_strategy(const MutationContext& context) const override;
    void transition_to(MutationsManager* manager, const std::string& ts) const override;
    std::string get_name() const override { return "NORMAL"; }
};

class SuccessState : public IMutationState {
public:
    int get_cycle_length() const override { return 60; } // Schneller Zyklus (aggressivere Exploration)
    std::unique_ptr<IMutationStrategy> get_strategy(const MutationContext& context) const override;
    void transition_to(MutationsManager* manager, const std::string& ts) const override;
    std::string get_name() const override { return "SUCCESS"; }
};

class StressState : public IMutationState {
public:
    int get_cycle_length() const override { return 30; } // Sehr schneller Zyklus (schnelle Rettung/Anpassung)
    std::unique_ptr<IMutationStrategy> get_strategy(const MutationContext& context) const override;
    void transition_to(MutationsManager* manager, const std::string& ts) const override;
    std::string get_name() const override { return "STRESS"; }
};