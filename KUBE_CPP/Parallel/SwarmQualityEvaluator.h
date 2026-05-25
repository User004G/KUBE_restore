// SwarmQualityEvaluator.h
#pragma once

#include <sqlite3.h>
#include <string>
#include "../MutationsManager.h"  // für SwarmState und MutationContext

class SwarmQualityEvaluator
{
public:
    SwarmQualityEvaluator(sqlite3* db_schwarm, int swarm_live_id);

    // Dummy: gibt immer NORMAL zurück
    SwarmState evaluate_state(const std::string& ts) const;

    // Dummy: baut nur einen einfachen Kontext mit Timestamp + Bounds
    MutationContext build_context(const std::string& ts,
        int p1_min, int p1_max,
        int p2_min, int p2_max,
        int p3_min, int p3_max) const;

private:
    sqlite3* db_schwarm_;  // im Dummy nicht genutzt
    int      swarm_live_id_;
};
