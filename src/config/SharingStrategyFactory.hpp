#pragma once

#include "utils/ErrorCodes.hpp"

#include "sharing/GlobalStrategies/GlobalSharingStrategy.hpp"
#include "sharing/Sharer.hpp"
#include "sharing/SharingStrategy.hpp"

#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include "core/painless.hpp"

#include <vector>

#define LOCAL_SHARING_STRATEGY_COUNT 3

/**
 * @brief Factory class for creating and managing sharing strategies.
 *
 * Two construction paths coexist:
 *  - The **topology path** (preferred), driven by the static
 *    ::createStrategy(name, db) overload used by the topology builder
 *    (see @ref topologies).
 *  - The **legacy parameters path** driven by ::instantiateLocalStrategies
 *    and ::instantiateGlobalStrategies, which use numeric strategy codes
 *    coming from CLI parameters.
 *
 * Local strategy numbers (legacy):
 * - 0 - Random selection from strategies 1-3
 * - 1 - HordeSatSharing with single group
 * - 2 - HordeSatSharing with two groups of producers
 * - 3 - SimpleSharing
 *
 * Global strategy numbers (legacy):
 * - 0 - Default to AllGatherSharing (same as 1)
 * - 1 - AllGatherSharing
 * - 2 - MallobSharing
 * - 3 - GenericGlobalSharing configured as RingSharing
 */
struct SharingStrategyFactory
{
  SharingStrategyFactory(const Parameters& parameters);
  /**
   * @brief Instantiate local sharing strategies.
   * @param strategyNumber The number of the strategy to instantiate:
   *        0: Random selection (1-3)
   *        1: HordeSatSharing (1 group)
   *        2: HordeSatSharing (2 groups)
   *        3: SimpleSharing (1 group)
   * @param[out] localStrategies Vector to store the created local strategies.
   * @param sharingEntities Vector of entities to be used in the strategies.
   */
  void instantiateLocalStrategies(
    int strategyNumber,
    std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
    std::vector<std::shared_ptr<SharingEntity>>& sharingEntities);

#ifndef NDIST
  /**
   * @brief Instantiate global sharing strategies.
   * @param strategyNumber The number of the strategy to instantiate:
   *        0: Default to AllGatherSharing
   *        1: AllGatherSharing
   *        2: MallobSharing
   *        3: GenericGlobalSharing configured as RingSharing
   * @param globalStrategies Vector to store the created global strategies.
   * @param mpiRank rank of this process
   * @param mpiWorldSize number of mpi processes in this process' world
   */
  void instantiateGlobalStrategies(
    const int strategyNumber,
    const int mpiRank,
    const int mpiWorldSize,
    std::vector<std::shared_ptr<GlobalSharingStrategy>>& globalStrategies);
#endif

  /**
   * @brief Configure the sharing strategies using the parameters referenced by
   * this factory
   * @param sharingEntities the sharing entities to dispatch accordingly to the
   * chosen sharing strategy
   * @return a vector containing the sharing strategies instantiated
   */
  std::vector<std::shared_ptr<SharingStrategy>> configureSharing(
    const int mpiRank,
    const int mpiWorldSize,
    std::vector<std::shared_ptr<SharingEntity>>& sharingEntities);

  /**
   * @brief Create a sharing strategy from a name (topology path).
   * @ingroup topology
   *
   * Accepted names (case-insensitive): `hordesat`, `simple`. Aborts with
   * PERR_NOT_SUPPORTED on an unknown name.
   *
   * The strategy takes ownership of @p database (one DB per strategy
   * instance).
   * The returned strategy is *not* yet `markConfigured()`'d: the caller is
   * expected to set parameters and wire producers/clients first.
   */
  static std::shared_ptr<SharingStrategy> createStrategy(
    const std::string& name,
    std::shared_ptr<ClauseDatabase>& database);

  /**
   * @brief Launch sharer threads for the given sharing strategies.
   * @param sharingStrategies Vector of sharing strategies to be executed.
   * @param sharers Vector to store the created sharer objects.
   */
  void createSharers(
    PainlessImpl& manager,
    std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies,
    std::vector<std::shared_ptr<Sharer>>& sharers);

private:
  const Parameters& m_parameters;
};