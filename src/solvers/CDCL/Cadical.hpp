#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseUtils.hpp"

#include "utils/Threading.hpp"

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "SolverCdclInterface.hpp"

#define CADICAL_

// Cadical includes
#include "cadical/src/cadical.hpp"

/*---------------------Main Class----------------------*/

/**
 * @brief Implements the different interfaces to interact with the Cadical CDCL solver
 * @ingroup solving_cdcl
 */
class Cadical
	: public SolverCdclInterface
	, public CaDiCaL::Learner
	, public CaDiCaL::Terminator
{
  public:
	/// Constructor.
	Cadical(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~Cadical();

	/* Execution */

	/// Solve the formula with a given cube.
	SatResult solve(const std::vector<int>& cube) override;

	/// Interrupt resolution, solving cannot continue until interrupt is unset.
	void setSolverInterrupt() override;

	/// Remove the SAT solving interrupt request.
	void unsetSolverInterrupt() override;

	/// @brief Initializes the map @ref CadicalOptions with the default configuration.
	void initCadicalOptions();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	/* Clause Management */

	/// Load formula from a given dimacs file, return false if failed.
	void loadFormula(const char* filename) override;

	/// Add a list of initial clauses to the formula.
	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) override;

	void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars);

	/// Add a permanent clause to the formula.
	void addClause(ClauseExchangePtr clause) override;

	/// Add a list of permanent clauses to the formula.
	void addClauses(const std::vector<ClauseExchangePtr>& clauses) override;

	// /// Set clauseToImport Database
	// void setClausesToImportDatabase(std::unique_ptr<ClauseDatabase>&& db) { this->clausesToImport = std::move(db); }

	/* Sharing */

	/// Add a learned clause to the formula.
	bool importClause(const ClauseExchangePtr& clause) override;

	/// Add a list of learned clauses to the formula.
	void importClauses(const std::vector<ClauseExchangePtr>& clauses) override;

	/* Variable Management */

	/// Get the number of variables of the current resolution.
	unsigned int getVariablesCount() override;

	/// Get a variable suitable for search splitting.
	int getDivisionVariable() override;

	/// Set initial phase for a given variable.
	void setPhase(const unsigned int var, const bool phase) override;

	/// Bump activity of a given variable.
	void bumpVariableActivity(const int var, const int times) override;

	/* Statistics And More */

	/// Get solver statistics.
	void printStatistics() override;

	void printWinningLog() override;

	std::vector<int> getFinalAnalysis() override;

	std::vector<int> getSatAssumptions() override;

	/// Return the model in case of SAT result.
	std::vector<int> getModel() override;

	/// @brief A map mapping a Cadical option name to its value.
	std::unordered_map<std::string, int> cadicalOptions;

  protected:
	/// Pointer to a Cadical solver.
	std::unique_ptr<CaDiCaL::Solver> solver;

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Used to stop or continue the resolution.
	std::atomic<bool> stopSolver;

	/*----------------------Learner------------------------*/
	/// @details It is important to note that the methods are not multi-thread safe
  public:
	/* Export */
	/// @brief  Tells if CaDiCaL should export a clause
	/// @param size size of the clause to export
	/// @param glue lbd value of the clause to export
	/// @return true if the clause is to be exported, false otherwise
	bool learning(int size, int glue) override;

	/**
	 * @brief Used by the base solver to export a clause
	 * @param lit a literal of the clause to export (non zero integer), 0 if it is the end
	 */
	void learn(int lit) override;

	/* Import */
	/**
	 * @brief Tells if CaDiCaL has a clause to import
	 * @return true if there is one, false otherwise
	 */
	bool hasClauseToImport() override;

	/**
	 * @brief Loads the clause to import data in the parameters
	 * @param clause holds the clause literals
	 * @param glue holds the lbd value of the clause
	 */
	void getClauseToImport(std::vector<int>& clause, int& glue) override;

  private:
	/// A vector to store the clause to export
	simpleClause tempClause;
	
	/// Stores the lbd value of the clause to export (loaded in learning)
	int lbd;
	
	/// A pointer pointing to the next clause to be exported (loaded in hasClauseToImprot)
	ClauseExchangePtr tempClauseToImport;

	/*-----------------------Terminator----------------------*/
	/**
	 * @brief Callback for the base solver to check if it should terminate or not
	 * @return true if the base solver should terminate, false otherwise
	 */
	bool terminate() { return this->stopSolver; }
};