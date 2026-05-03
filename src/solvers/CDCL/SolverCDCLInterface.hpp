#pragma once

#include "containers/ClauseDatabase.hpp"
#include "sharing/SharingEntity.hpp"
#include "solvers/SolverInterface.hpp"

/**
 * @defgroup solving_cdcl CDCL Solvers
 * @brief Different Classes for CDCL (Conflict-Driven Clause Learning) solvers
 * interaction
 * @ingroup solving
 *
 * This group provides a common interface for all CDCL solvers.
 * @{
 */

/// Code  for the type of solvers
enum class SolverCDCLType
{
  GLUCOSE = 0,
  LINGELING = 1,
  CADICAL = 2,
  MINISAT = 3,
  KISSAT = 4,
  MAPLECOMSPS = 5,
};

/**
 * @brief Interface for CDCL (Conflict-Driven Clause Learning) solvers
 * This class provides a common interface for all CDCL solvers.
 * It inherits from SolverInterface for solving primitives and SharingEntity for
 * sharing primitives.
 * @warning Be sure to use the sharing_id for produced clauses as source id
 * (.from attribute), otherwise the default exportClauseToClient will behave
 * wrongly
 */
class SolverCDCLInterface
  : public SolverInterface
  , public SharingEntity
{
public:
  /// Structure for solver statistics
  struct Statistics
  {
    unsigned long propagations = 0; ///< Number of propagations.
    unsigned long decisions = 0;    ///< Number of decisions taken.
    unsigned long conflicts = 0;    ///< Number of reached conflicts.
    unsigned long restarts = 0;     ///< Number of restarts.
    double memPeak = 0.f;           ///< Maximum memory used in Ko.
    unsigned long importedUnits;
    unsigned long importedBinaries;
    unsigned long importedLarges;
    unsigned long exportedClauses;
    unsigned long filteredExportedClauses;

    /// Get the header row for the statistics table
    static std::string getHeader();

    /// Get the footer row for the statistics table
    static std::string getFooter();

    /// Convert statistics to a formatted string row
    std::string toString(int solverTypeId, const std::string& solverName) const;
  };

  /**
   * @brief Constructor for SolverCDCLInterface
   * @param solverId Unique identifier for the solver
   * @param solverCDCLType Type of CDCL solver
   */
  SolverCDCLInterface(int solverId, SolverCDCLType solverCDCLType)
    : SolverInterface(SolverInterface::Type::CDCL, solverId)
    , SharingEntity()
    , m_cdclType(solverCDCLType)
  {
  }

  /**
   * @brief Destructor for SolverCDCLInterface
   */
  virtual ~SolverCDCLInterface() {}

  // Variable Management
  // ===================

  /**
   * @brief Bump activity of a given variable
   * @param var Variable identifier
   * @param times Number of times to bump the activity
   */
  virtual void bumpVariableActivity(const var_t var, const int times) = 0;

  // Solution & Result
  // =================

  /**
   * @brief Get the final analysis in case of UNSAT result
   * @return Vector of integers representing the final analysis
   */
  virtual std::vector<int> getFinalAnalysis() = 0;

  /**
   * @brief Get current assumptions
   * @return Vector of integers representing the current assumptions
   */
  virtual cube_t getSatAssumptions() = 0;

  // Getters & Setters
  // =================

  /**
   * @brief Returns solver type for static cast
   */
  SolverCDCLType getSolverType() { return this->m_cdclType; }

  static void printCDCLStats(
    const std::vector<std::shared_ptr<SolverCDCLInterface>>& solvers);

protected:
  /// @brief Type of this CDCL solver
  SolverCDCLType m_cdclType;
};

/**
 * @} // end of cdcl group
 */
