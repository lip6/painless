#pragma once

#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include <atomic>
#include <vector>

using IDScaler = std::function<unsigned(const std::shared_ptr<SolverInterface>&)>;

/**
 * @brief Factory class for creating and managing SAT solvers.
 * @ingroup solving
 */
class SolverFactory
{
  public:
	/**
	 * @brief Creates a solver of the specified type.
	 * @param type Character representing the solver type (e.g., 'g' for Glucose, 'l' for Lingeling).
	 * @param importDBType Character representing the import database type ('s' for single buffer, 'm' for Mallob, 'd'
	 * for per size).
	 * @param createdSolver Shared pointer to store the created solver.
	 * @return The algorithm type of the created solver.
	 */
	static SolverAlgorithmType createSolver(char type,
											char importDbType,
											std::shared_ptr<SolverInterface>& createdSolver);

	/**
	 * @brief Creates a solver of the specified type and adds it to the appropriate vector.
	 * @param type Character representing the solver type.
	 * @param importDbType Character representing the import database type.
	 * @param cdclSolvers Vector to store created CDCL solvers.
	 * @param localSolvers Vector to store created local search solvers.
	 * @return The algorithm type of the created solver (CDCL, LOCAL_SEARCH, or UNKNOWN).
	 */
	static SolverAlgorithmType createSolver(char type,
											char importDBType,
											std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
											std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers);

	/**
	 * @brief Creates multiple solvers based on a portfolio.
	 * @param count Maximum number of solvers to create.
	 * @param importDBType Character representing the import database type.
	 * @param portfolio String representing the types of solvers to create.
	 * @param cdclSolvers Vector to store created CDCL solvers.
	 * @param localSolvers Vector to store created local search solvers.
	 */
	static void createSolvers(int count,
							  char importDBType,
							  std::string portfolio,
							  std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
							  std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers);

	/**
	 * @brief Prints statistics for a group of solvers.
	 * @param cdclSolvers Vector of CDCL solvers.
	 * @param localSolvers Vector of local search solvers.
	 */
	static void printStats(const std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
						   const std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers);

	/**
	 * @brief Applies diversification to a group of solvers.
	 * @param cdclSolvers Vector of CDCL solvers.
	 * @param localSolvers Vector of local search solvers.
	 * @param generalIdScaler Function to scale the general solver ID (default uses solver's ID).
	 * @param typeIdScaler Function to scale the solver type ID (default uses solver's type ID).
	 */
	static void diversification(
		const std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
		const std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers,
		const IDScaler& generalIdScaler =
			[](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverId(); },
		const IDScaler& typeIdScaler =
			[](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverTypeId(); });

  public:
	/**
	 * @brief Atomic counter for assigning unique IDs to solvers.
	 */
	static std::atomic<int> currentIdSolver;
};