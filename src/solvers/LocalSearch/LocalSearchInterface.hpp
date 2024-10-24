#pragma once

#include "solvers/SolverInterface.hpp"

/**
 * @defgroup localsearch_solving  Local Search Solvers
 * @ingroup solving
 * @brief Different Classes for Local Search solver interaction
 * @{
 */

/// @brief Local Search solvers type
enum LocalSearchType
{
	YALSAT = 0,
};

/// Local search statistics
struct LocalSearchStats
{
	unsigned int numberUnsatClauses;
	unsigned int numberFlips;
	/*TODO add all needed stats */
};

/**
 * @brief Interface for Local Search solvers
 *
 * This class specializes SolverInterface to provide a common interface for all Local Search solvers.
 */
class LocalSearchInterface : public SolverInterface
{
  public:
	/**
	 * @brief Constructor for LocalSearchInterface
	 * @param solverId Unique identifier for the solver
	 * @param _lsType Type of local search algorithm
	 */
	LocalSearchInterface(int solverId, LocalSearchType _lsType)
		: SolverInterface(SolverAlgorithmType::LOCAL_SEARCH, solverId)
		, lsType(_lsType)
	{
	}

	/**
	 * @brief Virtual destructor
	 */
	virtual ~LocalSearchInterface() = default;

	/**
	 * @brief Set initial phase for a given variable
	 * @param var Variable identifier
	 * @param phase Boolean phase to set
	 */
	virtual void setPhase(const unsigned int var, const bool phase) = 0;

	/**
	 * @brief Print winning log information
	 *
	 * This method extends the base class implementation by adding solver-specific information.
	 */
	void printWinningLog()
	{
		this->SolverInterface::printWinningLog();
		LOGSTAT("The winner is %u.", this->getSolverId());
	}

	/**
	 * @brief Get the number of unsatisfied clauses
	 * @return Number of unsatisfied clauses
	 */
	unsigned int getNbUnsat() { return this->lsStats.numberUnsatClauses; }

  protected:
	/// @brief Type of the local search
	LocalSearchType lsType;

	/// @brief Statistics on the local search
	LocalSearchStats lsStats;

	/// @brief Vector holding the model or the final trail
	std::vector<int> finalTrail;
};

/**
 * @} // end of localsearch_solving group
 */