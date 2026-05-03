#pragma once

#include "core/painless.hpp"
#include "solvers/SolverInterface.hpp"
#include "working/WorkingStrategy.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Single place mapping working-strategy name (case-insensitive) to a
 * concrete instance, shared by both the parameters-driven and topology-driven
 * builders.
 * @ingroup topology
 */
namespace WorkingStrategyRegistry {

/**
 * @brief Create a working strategy by name.
 * @ingroup topology
 *
 * Accepted names (case-insensitive):
 *  - `PortfolioSimple` &rarr; ::PortfolioSimple
 *  - `DivideAndConquer` &rarr; ::DivideAndConquer
 *
 * Aborts with PERR_NOT_SUPPORTED on any other name. The returned strategy
 * is *not* yet `markConfigured()`'d: the caller is expected to apply
 * parameters first.
 */
std::shared_ptr<WorkingStrategy>
create(const std::string& name,
       PainlessImpl& painless,
       std::vector<std::shared_ptr<SolverInterface>>& solvers);

} // namespace WorkingStrategyRegistry
