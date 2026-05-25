// SwarmQualityEvaluator.cpp
#include "SwarmQualityEvaluator.h"
#include <cstdio>

SwarmQualityEvaluator::SwarmQualityEvaluator(sqlite3* db_schwarm, int swarm_live_id)
    : db_schwarm_(db_schwarm),
    swarm_live_id_(swarm_live_id)
{
    std::printf("[SwarmQualityEvaluator] created (swarm_live_id=%d)\n", swarm_live_id_);
}

SwarmState SwarmQualityEvaluator::evaluate_state(const std::string& ts) const
{
    // Dummy: Immer NORMAL
    (void)ts;
    return SwarmState::NORMAL;
}

MutationContext SwarmQualityEvaluator::build_context(const std::string& ts,
    int p1_min, int p1_max,
    int p2_min, int p2_max,
    int p3_min, int p3_max) const
{
    MutationContext ctx;
    ctx.current_timestamp = ts;

    ctx.param1_min = p1_min;
    ctx.param1_max = p1_max;
    ctx.param2_min = p2_min;
    ctx.param2_max = p2_max;
    ctx.param3_min = p3_min;
    ctx.param3_max = p3_max;

    // Dummy-Werte
    ctx.current_volatility = 0.0;
    ctx.system_equity = 0.0;

    return ctx;
}
