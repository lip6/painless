/**
 * @file SolverInterface.h
 * @brief Interface for SAT solvers with standard features.
 */

#pragma once

#include <painless/solver.hpp>

#include "containers/CSRMatrix.hpp"
#include "containers/ClauseExchange.hpp"
#include "containers/ClauseUtils.hpp"

#include "utils/Logger.hpp"
#include "config/Configurable.hpp"

#include <atomic>
#include <memory>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

/**
 * @defgroup solving SAT Solvers
 * @ingroup solving
 * @brief Different Classes for SAT formula processing
 * @todo Implement all the functions in the different solvers, in order to have
 * all parallel strategies runnable
 * @{
 */

/* Forward Declarations */
class SolverInterface;

using SeedGenerator = std::function<int(SolverInterface*)>;
using ClauseReader = std::function<bool(clause_view_t)>;
using FullClauseReader = std::function<uint(ClauseReader, uint)>;
using formula_t = pl::csr_matrix_t<lit_t>;

/**
 * @brief Interface for a SAT solver with standard features.
 */
class SolverInterface : public Configurable
{
public:
  /**
   * @brief Enumeration for types of solver algorithms.
   */
  enum class Type
  {
    CDCL = 0,         /**< Conflict-Driven Clause Learning */
    LOCAL_SEARCH = 1, /**< Local Search */
    LOOK_AHEAD = 2,   /**< Look-Ahead */
    PREPROCESS = 3,   /**< Preprocess algorithm types */
    UNKNOWN = 255     /**< Unknown algorithm type */
  };

  /**
   * @brief Constructor for SolverInterface.
   * @param algoType The algorithm type.
   * @param solverId The solver ID.
   */
  SolverInterface(SolverInterface::Type algoType, plid_t solverId)
    : m_algoType(algoType)
    , m_initialized(false)
    , m_solverId(solverId)
  {
  }

  /**
   * @brief Virtual destructor for SolverInterface.
   */
  virtual ~SolverInterface()
  {
    std::string algoTypeStr;
    switch (m_algoType) {
      case Type::CDCL:
        algoTypeStr = "CDCL";
        break;
      case Type::LOCAL_SEARCH:
        algoTypeStr = "LOCAL SEARCH";
        break;
      case Type::LOOK_AHEAD:
        algoTypeStr = "LOOK AHEAD";
        break;
      case Type::PREPROCESS:
        algoTypeStr = "PREPROCESS";
        break;
      default:
        algoTypeStr = "UNKNOWN";
        break;
    }
    LOGD2("Destroying %s solver %u", algoTypeStr.c_str(), this->getSolverId());
    auto it = s_instanceCounts.find(std::type_index(typeid(*this)));
    if (it != s_instanceCounts.end())
      it->second--;
  }

  // Execution
  // =========

  /**
   * @brief Solve the formula with a given cube.
   * @param cube The cube to solve with.
   * @return The result of the solving process.
   */
  virtual SatAnswer solve(cube_view_t cube) = 0;

  /**
   * @brief Interrupt resolution, solving cannot continue until interrupt is
   * unset.
   */
  virtual void setSolverInterrupt() = 0;

  /**
   * @brief Remove the SAT solving interrupt request.
   */
  virtual void unsetSolverInterrupt() = 0;

  // Clause Management
  // =================

  /**
   * @brief Add a permanent clause to the solver.
   * @param clause The clause to add.
   * @return True if the addition was successful, False otherwise.
   */
  virtual bool addClause(clause_view_t clause) = 0;

  /**
   * @brief Load formula from a given dimacs file.
   * @param filename The name of the file to load from.
   */
  virtual void loadFormula(const char* filename) = 0;

  /**
   * @brief Load permanent clauses into the backend solver using internal
   * structures (example the painless reader callback).
   * @return Number of clauses added
   */
  virtual uint loadClauses() = 0;

  // Variable Management
  // ===================

  /**
   * @brief Get the current number of variables.
   * @return The number of variables.
   */
  virtual uint getVariableCount() = 0;

  /**
   * @brief Set initial phase for a given variable
   * @param var Variable identifier
   * @param phase Boolean phase to set
   */
  virtual void setPhase(const var_t var, const bool phase) = 0;

  // Result & Solution
  // =================

  /**
   * @brief Return the model in case of SAT result.
   * @return The model as a vector of integers.
   */
  virtual model_t getModel() = 0;

  // Statistics
  // ==========

  /**
   * @brief Print solver statistics.
   */
  virtual std::string statisticsToString() = 0;

    /**
   * @brief Prints statistics for a group of solvers.
   * @param solvers Vector of solvers.

   */
  static void printStats(
    const std::vector<std::shared_ptr<SolverInterface>>& solvers);

  // Getters & Setters
  // =================

    /**
   * @brief Perform native diversification. The Default lambda returns the
   * solver ID.
   */
  virtual void diversify(const SeedGenerator& getSeed = [](SolverInterface* s) {
    return s->getSolverId();
  }) = 0;

  /**
   * @brief Check if the solver is initialized.
   * @return True if initialized, false otherwise.
   */
  bool isInitialized() { return this->m_initialized; }

  /**
   * @brief Get the algorithm type of the solver.
   * @return The algorithm type.
   */
  SolverInterface::Type getAlgoType() const { return this->m_algoType; }

  /**
   * @brief Get the solver type ID.
   * @return The solver type ID.
   */
  plid_t getSolverTypeId() const { return this->m_solverTypeId; }

  /**
   * @brief Set the solver type ID.
   * @param typeId The solver type ID to set.
   */
  void setSolverTypeId(plid_t typeId) { this->m_solverTypeId = typeId; }

  /**
   * @brief Get the solver ID.
   * @return The solver ID.
   */
  plid_t getSolverId() const { return this->m_solverId; }

  /**
   * @brief Set the solver ID.
   * @param id The solver ID to set.
   */
  void setSolverId(plid_t id) { this->m_solverId = id; }

  /**
   * @brief Get the current count of instances of this object's most-derived
   * type.
   *
   * @return plid_t The current count of instances of this object's most-derived
   * type.
   */
  plid_t getSolverTypeCount() const
  {
    auto it = s_instanceCounts.find(std::type_index(typeid(*this)));
    return (it != s_instanceCounts.end()) ? it->second.load() : 0;
  }

  // Diverse methods to be rethinked
  // ===============================

    /**
   * @brief Get a variable suitable for search splitting.
   * @return The division variable.
   */
  virtual var_t getDivisionVariable() = 0;

protected:
  /**
   * @brief Get and increment the instance count for a specific derived type.
   *
   * @tparam Derived The type of the derived class.
   * @return unsigned int The count of instances (including this one) of the
   * specified type.
   */
  template<typename Derived>
  static unsigned int getAndIncrementTypeCount()
  {
    auto [it, inserted] =
      s_instanceCounts.try_emplace(std::type_index(typeid(Derived)), 0);
    return it->second.fetch_add(1);
  }

  /**
   * @brief Initialize the type ID for a derived class.
   *
   * This method should be called in the constructor of each derived class
   * to properly initialize the type-specific instance count.
   *
   * @tparam Derived The type of the derived class.
   */
  template<typename Derived>
  void initializeTypeId()
  {
    m_solverTypeId = getAndIncrementTypeCount<Derived>();
    LOGD1("I am solver of type %s: id %d, typeId: %u",
          typeid(Derived).name(),
          m_solverId,
          m_solverTypeId);
  }

protected:
  SolverInterface::Type m_algoType; /**< Algorithm family of this solver. */
  std::atomic<bool> m_initialized;  /**< Initialization status. */
  plid_t m_solverTypeId;            /**< ID local to the solver type. */
  plid_t m_solverId;                /**< Main ID of the solver. */

  /**
   * @brief Number of existing instances of derived classes.
   */
  static inline std::unordered_map<std::type_index, std::atomic<plid_t>>
    s_instanceCounts;
};

/**
 * @} // end of solving group
 */