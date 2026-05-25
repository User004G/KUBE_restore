// SwarmContextEvaluator.cpp
#include "SwarmContextEvaluator.h"
#include "../MutationsManager.h"   // für MutationContext
#include <cstdio>

SwarmContextEvaluator::SwarmContextEvaluator(sqlite3* db_schwarm, int swarm_id)
    : db_schwarm_(db_schwarm),
    swarm_id_(swarm_id)
{
    std::printf("[SwarmContextEvaluator] created (swarm_id=%d)\n", swarm_id_);
}

void SwarmContextEvaluator::enrich_context(const std::string& ts,
    MutationContext& ctx) const
{
    // Vollständiger Dummy – aktuell machen wir GAR NICHTS.
    // Hier könntest du später z.B. Marktregime, andere Schwärme etc. einbauen.
    (void)ts;
    (void)ctx;
}
