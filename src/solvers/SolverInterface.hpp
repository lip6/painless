/**
 * @file SolverInterface.h
 * @brief Interface for SAT solvers with standard features.
 */

#pragma once

#include "containers/ClauseExchange.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Logger.hpp"

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
 * @todo Implement all the functions in the different solvers, in order to have all parallel strategies runnable
 * @{
 */

/* Forward Declaration */
class SolverInterface;

using SeedGenerator = std::function<int(SolverInterface*)>;

/**
 * @brief Enumeration for SAT solver results.
 */
enum class SatResult
{
	SAT = 10,	  /**< Satisfiable */
	UNSAT = 20,	  /**< Unsatisfiable */
	TIMEOUT = 30, /**< Timeout occurred */
	UNKNOWN = 0	  /**< Unknown result */
};

/**
 * @brief Enumeration for types of solver algorithms.
 */
enum class SolverAlgorithmType
{
	CDCL = 0,		  /**< Conflict-Driven Clause Learning */
	LOCAL_SEARCH = 1, /**< Local Search */
	LOOK_AHEAD = 2,	  /**< Look-Ahead */
	OTHER = 3,		  /**< Other algorithm types */
	UNKNOWN = 255	  /**< Unknown algorithm type */
};

/**
 * @brief Interface for a SAT solver with standard features.
 */
class SolverInterface
{
  public:
	/**
	 * @brief Get the current number of variables.
	 * @return The number of variables.
	 */
	virtual unsigned int getVariablesCount() = 0;

	/**
	 * @brief Get a variable suitable for search splitting.
	 * @return The division variable.
	 */
	virtual int getDivisionVariable() = 0;

	/**
	 * @brief Interrupt resolution, solving cannot continue until interrupt is unset.
	 */
	virtual void setSolverInterrupt() = 0;

	/**
	 * @brief Remove the SAT solving interrupt request.
	 */
	virtual void unsetSolverInterrupt() = 0;

	/**
	 * @brief Print the winning log.
	 */
	virtual void printWinningLog();

	/**
	 * @brief Solve the formula with a given cube.
	 * @param cube The cube to solve with.
	 * @return The result of the solving process.
	 */
	virtual SatResult solve(const std::vector<int>& cube) = 0;

	/**
	 * @brief Add a permanent clause to the formula.
	 * @param clause The clause to add.
	 */
	virtual void addClause(ClauseExchangePtr clause) = 0;

	/**
	 * @brief Add a list of permanent clauses to the formula.
	 * @param clauses The list of clauses to add.
	 */
	virtual void addClauses(const std::vector<ClauseExchangePtr>& clauses) = 0;

	/**
	 * @brief Add a list of initial clauses to the formula.
	 * @param clauses The list of initial clauses.
	 * @param nbVars The number of variables.
	 */
	virtual void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) = 0;

	/**
	 * @brief Add an initial list of zero terminated clauses
	 * @param literals The list of initial clauses.
	 * @param clsCount The number of clauses.
	 * @param nbVars The number of variables.
	 */
	virtual void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars) = 0;

	/**
	 * @brief Load formula from a given dimacs file.
	 * @param filename The name of the file to load from.
	 */
	virtual void loadFormula(const char* filename) = 0;

	/**
	 * @brief Print solver statistics.
	 */
	virtual void printStatistics();

	/**
	 * @brief Return the model in case of SAT result.
	 * @return The model as a vector of integers.
	 */
	virtual std::vector<int> getModel() = 0;

	/**
	 * @brief Print the parameters set for a solver.
	 */
	virtual void printParameters();

	/**
	 * @brief Perform native diversification. The Default lambda returns the solver ID.
	 */
	virtual void diversify(const SeedGenerator& getSeed = [](SolverInterface* s) { return s->getSolverId(); }) = 0;

	/**
	 * @brief Check if the solver is initialized.
	 * @return True if initialized, false otherwise.
	 */
	bool isInitialized() { return this->m_initialized; }

	/**
	 * @brief Set the initialization status of the solver.
	 * @param value The initialization status to set.
	 */
	void setInitialized(bool value) { this->m_initialized = value; }

	/**
	 * @brief Get the algorithm type of the solver.
	 * @return The algorithm type.
	 */
	SolverAlgorithmType getAlgoType() { return this->m_algoType; }

	/**
	 * @brief Get the solver type ID.
	 * @return The solver type ID.
	 */
	unsigned int getSolverTypeId() { return this->m_solverTypeId; }

	/**
	 * @brief Set the solver type ID.
	 * @param typeId The solver type ID to set.
	 */
	void setSolverTypeId(unsigned int typeId) { this->m_solverTypeId = typeId; }

	/**
	 * @brief Get the solver ID.
	 * @return The solver ID.
	 */
	unsigned int getSolverId() { return this->m_solverId; }

	/**
	 * @brief Set the solver ID.
	 * @param id The solver ID to set.
	 */
	void setSolverId(unsigned int id) { this->m_solverId = id; }

	/**
	 * @brief Get the current count of instances of this object's most-derived type.
	 *
	 * @return unsigned int The current count of instances of this object's most-derived type.
	 */
	unsigned int getSolverTypeCount() const
	{
		auto it = s_instanceCounts.find(std::type_index(typeid(*this)));
		return (it != s_instanceCounts.end()) ? it->second.load() : 0;
	}

	/**
	 * @brief Constructor for SolverInterface.
	 * @param algoType The algorithm type.
	 * @param solverId The solver ID.
	 */
	SolverInterface(SolverAlgorithmType algoType, int solverId);

	/**
	 * @brief Virtual destructor for SolverInterface.
	 */
	virtual ~SolverInterface();

  protected:
	/**
	 * @brief Get and increment the instance count for a specific derived type.
	 *
	 * @tparam Derived The type of the derived class.
	 * @return unsigned int The count of instances (including this one) of the specified type.
	 */
	template<typename Derived>
	static unsigned int getAndIncrementTypeCount()
	{
		auto [it, inserted] = s_instanceCounts.try_emplace(std::type_index(typeid(Derived)), 0);
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
		LOGDEBUG1("I am solver of type %s: id %d, typeId: %u", typeid(Derived).name(), m_solverId, m_solverTypeId);
	}

  protected:
	SolverAlgorithmType m_algoType;	 /**< Algorithm family of this solver. */
	std::atomic<bool> m_initialized; /**< Initialization status. */
	unsigned int m_solverTypeId;	 /**< ID local to the solver type. */
	int m_solverId;					 /**< Main ID of the solver. */

	/**
	 * @brief Number of existing instances of derived classes.
	 */
	static inline std::unordered_map<std::type_index, std::atomic<unsigned int>> s_instanceCounts;
};

/**
 * @} // end of solving group
 */