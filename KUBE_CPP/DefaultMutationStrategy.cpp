// DefaultMutationStrategy.cpp
#include "DefaultMutationStrategy.h"

// HIER brauchen wir die konkreten Definitionen von
// SwarmState, EAParams, MutationContext, MutationAction.
// Die stecken bei dir aktuell in MutationsManager.h.
#include "MutationsManager.h"

#include <random>
#include <cstdio>

std::string DefaultMutationStrategy::name() const
{
    return "DefaultMutationStrategy";
}

SwarmState DefaultMutationStrategy::applicable_state() const
{
    // Dummy: nur im NORMAL-Zustand verwendet
    return SwarmState::NORMAL;
}

std::vector<MutationAction> DefaultMutationStrategy::plan_mutations(
    const std::vector<EAParams>& eas,
    const MutationContext& ctx,
    std::mt19937_64& rng
) const
{
    std::vector<MutationAction> actions;
    if (eas.empty())
        return actions;

    // Zufallsbereiche aus dem Kontext
    std::uniform_int_distribution<int> dist_p1(ctx.param1_min, ctx.param1_max);
    std::uniform_int_distribution<int> dist_p2(ctx.param2_min, ctx.param2_max);
    std::uniform_int_distribution<int> dist_p3(ctx.param3_min, ctx.param3_max);

    std::printf(
        "[DefaultMutationStrategy] Planning mutations for %zu EAs (ts=%s)\n",
        eas.size(),
        ctx.current_timestamp.c_str()
    );

    for (const auto& ea : eas)
    {
        MutationAction act;
        act.magic = ea.magic;
        act.new_param1 = dist_p1(rng);
        act.new_param2 = dist_p2(rng);
        act.new_param3 = dist_p3(rng);
        actions.push_back(act);
    }

    return actions;
}
