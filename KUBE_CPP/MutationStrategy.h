// MutationStrategy.h
#pragma once

#include <string>
#include <vector>
#include <random>

// Vorwärtsdeklarationen, um Zyklen zu vermeiden
enum class SwarmState;
struct EAParams;
struct MutationContext;
struct MutationAction;

// sqlite3 nur als Pointer benutzt -> Vorwärtsdeklaration reicht,
// die eigentliche Definition kommt in der .cpp über #include <sqlite3.h>
struct sqlite3;

// -------------------------------------------------------
// Basisklasse für Mutationsstrategien
// -------------------------------------------------------
class IMutationStrategy
{
public:
    virtual ~IMutationStrategy() = default;

    // Kurzer Name der Strategie (für Logging)
    virtual std::string name() const = 0;

    // In welchem SwarmState diese Strategie "gedacht" ist
    virtual SwarmState applicable_state() const = 0;

    // Kern-Interface:
    //  - eas : Kandidaten-EAs (typisch: erste N Live-EAs)
    //  - ctx : Mutationskontext (Bounds, Timestamp, etc.)
    //  - rng : Zufallsgenerator
    // Rückgabe:
    //  - Liste von Mutations-Aktionen (welche Magic bekommt welche neuen P1–P3)
    virtual std::vector<MutationAction> plan_mutations(
        const std::vector<EAParams>& eas,
        const MutationContext& ctx,
        std::mt19937_64& rng
    ) const = 0;

protected:
    // Helper der Basisklasse:
    // Prüft in KUBE_Schwarm.db, ob (p1,p2,p3) im gegebenen swarm_id
    // bereits existiert. Gibt true zurück, wenn die Kombination
    // noch NICHT vorhanden ist (also "unique").
    bool is_unique_param_triple(
        sqlite3* db_schwarm,
        int      swarm_id,
        int      p1,
        int      p2,
        int      p3
    ) const;
};
