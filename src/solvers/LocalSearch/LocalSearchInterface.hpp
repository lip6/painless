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
  TASSAT = 1,
};

/// Local search statistics
struct LocalSearchStats
{
  unsigned int numberUnsatClauses;
  unsigned int numberFlips;
  unsigned int tries;
};

/**
 * @brief Interface for Local Search solvers
 *
 * This class specializes SolverInterface to provide a common interface for all
 * Local Search solvers.
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
    : SolverInterface(SolverInterface::Type::LOCAL_SEARCH, solverId)
    , lsType(_lsType)
  {
  }

  /**
   * @brief Virtual destructor
   */
  virtual ~LocalSearchInterface()
  {
    LOGD1("LocalSearcher %d deleted!", this->getSolverId());
  }

  /**
   * @brief Get the number of unsatisfied clauses
   * @return Number of unsatisfied clauses
   */
  unsigned int getNumUnsats() { return this->lsStats.numberUnsatClauses; }

protected:
  /// @brief Type of the local search
  LocalSearchType lsType;

  /// @brief Statistics on the local search
  LocalSearchStats lsStats;
};

/**
 * @} // end of localsearch_solving group
 */