#pragma once

#include "./Sharer.hpp"

#include "./utils/ErrorCodes.hpp"
#include "./utils/MpiUtils.hpp"
#include "./utils/Parameters.hpp"

#include "./sharing/GlobalStrategies/GlobalSharingStrategy.hpp"
#include "./sharing/SharingStrategy.hpp"

#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include <vector>

#define LOCAL_SHARING_STRATEGY_COUNT 5

/**
 * @brief Factory class for creating and managing sharing strategies.
 */
struct SharingStrategyFactory
{
    /// The selected local sharing strategy number.
    static int selectedLocal;

    /// The selected global sharing strategy number.
    static int selectedGlobal;

    /**
     * @brief Instantiate local sharing strategies.
     * @param strategyNumber The number of the strategy to instantiate.
     * @param localStrategies Vector to store the created local strategies.
     * @param cdclSolvers Vector of CDCL solvers to be used in the strategies.
     */
    static void instantiateLocalStrategies(int strategyNumber,
                                           std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
                                           std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers);

    /**
     * @brief Instantiate global sharing strategies.
     * @param strategyNumber The number of the strategy to instantiate.
     * @param globalStrategies Vector to store the created global strategies.
     */
    static void instantiateGlobalStrategies(int strategyNumber,
                                            std::vector<std::shared_ptr<GlobalSharingStrategy>>& globalStrategies);

    /**
     * @brief Launch sharer threads for the given sharing strategies.
     * @param sharingStrategies Vector of sharing strategies to be executed.
     * @param sharers Vector to store the created sharer objects.
     */
    static void launchSharers(std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies,
                              std::vector<std::unique_ptr<Sharer>>& sharers);

    /**
     * @brief Add new entities (solvers) to existing local strategies.
     * @param localStrategies Vector of existing local strategies.
     * @param newSolvers Vector of new CDCL solvers to be added.
     */
    static void addEntitiesToLocal(std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
                                   std::vector<std::shared_ptr<SolverCdclInterface>>& newSolvers);
};