#pragma once

#include "containers/ClauseDatabase.hpp"
#include "sharing/SharingEntity.hpp"
#include "solvers/SolverInterface.hpp"

/**
 * @defgroup solving_cdcl CDCL Solvers
 * @brief Different Classes for CDCL (Conflict-Driven Clause Learning) solvers interaction
 * @ingroup solving
 *
 * This group provides a common interface for all CDCL solvers.
 * @{
 */

/// Code  for the type of solvers
enum class SolverCdclType
{
	GLUCOSE = 0,
	LINGELING = 1,
	CADICAL = 2,
	MINISAT = 3,
	KISSAT = 4,
	MAPLECOMSPS = 5,
	KISSATMAB = 6,
	KISSATINC = 7
};

/// Structure for solver statistics
struct SolvingCdclStatistics
{
	/// Constructor
	SolvingCdclStatistics()
	{
		propagations = 0;
		decisions = 0;
		conflicts = 0;
		restarts = 0;
		memPeak = 0;
	}

	unsigned long propagations; ///< Number of propagations.
	unsigned long decisions;	///< Number of decisions taken.
	unsigned long conflicts;	///< Number of reached conflicts.
	unsigned long restarts;		///< Number of restarts.
	double memPeak;				///< Maximum memory used in Ko.
};

/**
 * @brief Interface for CDCL (Conflict-Driven Clause Learning) solvers
 * This class provides a common interface for all CDCL solvers.
 * It inherits from SolverInterface for solving primitives and SharingEntity for sharing primitives.
 * @warning Be sure to use the sharing_id for producer clauses, otherwise the default exportClauseToClient will behave wrongly
 */
class SolverCdclInterface
	: public SolverInterface
	, public SharingEntity
{
  public:
	/**
	 * @brief Set initial phase for a given variable
	 * @param var Variable identifier
	 * @param phase Boolean phase to set
	 */
	virtual void setPhase(const unsigned int var, const bool phase) = 0;

	/**
	 * @brief Bump activity of a given variable
	 * @param var Variable identifier
	 * @param times Number of times to bump the activity
	 */
	virtual void bumpVariableActivity(const int var, const int times) = 0;

	/**
	 * @brief Get the final analysis in case of UNSAT result
	 * @return Vector of integers representing the final analysis
	 */
	virtual std::vector<int> getFinalAnalysis() = 0;

	/**
	 * @brief Get current assumptions
	 * @return Vector of integers representing the current assumptions
	 */
	virtual std::vector<int> getSatAssumptions() = 0;

	/**
	 * @brief Print winning log information
	 */
	void printWinningLog() { this->SolverInterface::printWinningLog(); }

	/**
	 * @brief Returns solver type for static cast
	 */
	SolverCdclType getSolverType() { return this->m_cdclType; }

	/**
	 * @brief Constructor for SolverCdclInterface
	 * @param solverId Unique identifier for the solver
	 * @param solverCdclType Type of CDCL solver
	 */
	SolverCdclInterface(int solverId, const std::shared_ptr<ClauseDatabase>& clauseDB, SolverCdclType solverCdclType)
		: SolverInterface(SolverAlgorithmType::CDCL, solverId)
		, m_clausesToImport(clauseDB)
		, m_cdclType(solverCdclType)
		, SharingEntity()
	{
	}

	/**
	 * @brief Virtual destructor
	 */
	virtual ~SolverCdclInterface() { LOGDEBUG2("Destroying solver %d", this->getSolverId()); }

	/// @brief Type of this CDCL solver
	SolverCdclType m_cdclType;

  protected:
	/// @brief Database used to import clauses. Can be common with other solvers
	std::shared_ptr<ClauseDatabase> m_clausesToImport;
};

/**
 * @} // end of cdcl group
 */
