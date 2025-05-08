#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Threading.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "KissatFamily.hpp"
#include "SolverCdclInterface.hpp"

#define KISSAT_

// Kissat includes
extern "C"
{
#include <kissat/src/kissat.h>
}

/// Instance of a Kissat solver
/// @ingroup solving_cdcl
class Kissat : public SolverCdclInterface
{
  public:
	/// Constructor.
	Kissat(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~Kissat();

	/* Execution */

	/// Solve the formula with a given cube.
	SatResult solve(const std::vector<int>& cube) override;

	/// Interrupt resolution, solving cannot continue until interrupt is unset.
	void setSolverInterrupt() override;

	/// Remove the SAT solving interrupt request.
	void unsetSolverInterrupt() override;

	/// @brief Initializes the map @ref KissatOptions with the default configuration.
	void initKissatOptions();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	/* Clause Management */

	/// Load formula from a given dimacs file, return false if failed.
	void loadFormula(const char* filename) override;

	void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars) override;

	/// Add a list of initial clauses to the formula.
	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) override;

	/// Add a permanent clause to the formula.
	void addClause(ClauseExchangePtr clause) override;

	/// Add a list of permanent clauses to the formula.
	void addClauses(const std::vector<ClauseExchangePtr>& clauses) override;

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

	/// @brief A map mapping a kissat option name to its value.
	std::unordered_map<std::string, int> kissatOptions;

	/// Set family from working strategy
	void setFamily(KissatFamily family) { this->family = family; };


  protected:
	/// Compute kissat family for diversification
	void computeFamily();
	
  protected:
	/// Pointer to a Kissat solver.
	kissat* solver;

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Used to stop or continue the resolution.
	std::atomic<bool> stopSolver;

	KissatFamily family;

	unsigned int originalVars;

	/// Termination callback.
	friend int kissatTerminate(void* solverPtr);

	/// Callback to export/import clauses used by real kissat.
	/* Decided to not use pointers to move because of c++ stl (cannot move an array into a vector, sharedPtr
	 * destruction) */
	friend char kissatImportClause(void*, kissat*);
	friend char kissatExportClause(void*, kissat*);
};
