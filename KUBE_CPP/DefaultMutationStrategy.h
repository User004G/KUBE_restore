// DefaultMutationStrategy.h
#pragma once

#include "MutationStrategy.h"

// -------------------------------------------------------
// DefaultMutationStrategy
//  - Dummy-Strategie für die aktuelle Phase:
//    * Mutiert alle übergebenen EAs zufällig in den Bounds.
//    * Kennt Schonfrist/Blick auf Fitness NOCH NICHT.
// -------------------------------------------------------
class DefaultMutationStrategy : public IMutationStrategy
{
public:
    std::string name() const override;
    SwarmState  applicable_state() const override;

    std::vector<MutationAction> plan_mutations(
        const std::vector<EAParams>& eas,
        const MutationContext& ctx,
        std::mt19937_64& rng
    ) const override;
};
