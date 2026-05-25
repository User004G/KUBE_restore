// SwarmContextEvaluator.h
#pragma once

#include <sqlite3.h>
#include <string>

struct MutationContext;  // Vorwärtsdeklaration

class SwarmContextEvaluator
{
public:
    SwarmContextEvaluator(sqlite3* db_schwarm, int swarm_id);

    // Dummy: macht (noch) nichts mit ctx
    void enrich_context(const std::string& ts, MutationContext& ctx) const;

private:
    sqlite3* db_schwarm_;
    int      swarm_id_;
};
