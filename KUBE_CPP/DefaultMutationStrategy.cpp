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

// -------------------------------------------------------
// CrossoverMutationStrategy
// -------------------------------------------------------
std::string CrossoverMutationStrategy::name() const
{
    return "CrossoverMutationStrategy";
}

SwarmState CrossoverMutationStrategy::applicable_state() const
{
    return SwarmState::NORMAL;
}

std::vector<MutationAction> CrossoverMutationStrategy::plan_mutations(
    const std::vector<EAParams>& eas,
    const MutationContext& ctx,
    std::mt19937_64& rng
) const
{
    std::vector<MutationAction> actions;
    if (eas.empty() || ctx.best_eas.size() < 4)
    {
        std::fprintf(stderr, "[CrossoverMutationStrategy] Insufficient EAs: need 4 best, got %zu best and %zu targets.\n", 
            ctx.best_eas.size(), eas.size());
        return actions;
    }

    std::printf("[CrossoverMutationStrategy] Planning crossover mutations for %zu targets using %zu best parents.\n",
        eas.size(), ctx.best_eas.size());

    // Wir wollen 12 Kinder (6 pro Elternpaar) generieren.
    // Falls weniger als 12 Ziel-EAs vorhanden sind, iterieren wir nur bis eas.size().
    size_t target_idx = 0;

    // Wir bilden 2 Paare: (0,1), (2,3)
    for (int pair_idx = 0; pair_idx < 2; ++pair_idx)
    {
        const EAParams& parentA = ctx.best_eas[pair_idx * 2];
        const EAParams& parentB = ctx.best_eas[pair_idx * 2 + 1];

        // Wir generieren die 6 Zwischen-Kombinationen (Bitmaske 1 bis 6)
        // Bit 0 => param1
        // Bit 1 => param2
        // Bit 2 => param3
        for (int mask = 1; mask <= 6; ++mask)
        {
            if (target_idx >= eas.size()) break; // Kein Ziel-EA mehr übrig

            int p1 = (mask & 1) ? parentB.param1 : parentA.param1;
            int p2 = (mask & 2) ? parentB.param2 : parentA.param2;
            int p3 = (mask & 4) ? parentB.param3 : parentA.param3;

            MutationAction act;
            act.magic = eas[target_idx].magic;
            act.new_param1 = p1;
            act.new_param2 = p2;
            act.new_param3 = p3;
            
            actions.push_back(act);
            target_idx++;
        }
        if (target_idx >= eas.size()) break;
    }

    std::printf("[CrossoverMutationStrategy] Planned %zu crossover actions.\n", actions.size());
    return actions;
}
