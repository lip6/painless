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

#define KISSAT_MAB_

// KissatMABSolver includes
extern "C"
{
#include "kissat_mab/src/kissat.h"
}

// Wrapping struct
typedef struct kissat kissat_mab;

/// Instance of a KissatMABSolver solver
/// @ingroup solving_cdcl
class KissatMABSolver : public SolverCdclInterface
{
  public:
	void setBumpVar(int v);

	/// Load formula from a given dimacs file, return false if failed.
	void loadFormula(const char* filename);

	/// Get the number of variables of the current resolution.
	unsigned int getVariablesCount();

	/// Get a variable suitable for search splitting.
	int getDivisionVariable();

	/// Set initial phase for a given variable.
	void setPhase(const unsigned int var, const bool phase);

	/// Bump activity of a given variable.
	void bumpVariableActivity(const int var, const int times);

	/// Interrupt resolution, solving cannot continue until interrupt is unset.
	void setSolverInterrupt();

	/// Remove the SAT solving interrupt request.
	void unsetSolverInterrupt();

	/// Solve the formula with a given cube.
	SatResult solve(const std::vector<int>& cube);

	/// Add a permanent clause to the formula.
	void addClause(ClauseExchangePtr clause);

	/// Add a list of permanent clauses to the formula.
	void addClauses(const std::vector<ClauseExchangePtr>& clauses);

	void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars);

	/// Add a list of initial clauses to the formula.
	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) override;

	/// Add a learned clause to the formula.
	bool importClause(const ClauseExchangePtr& clause) override;

	/// Add a list of learned clauses to the formula.
	void importClauses(const std::vector<ClauseExchangePtr>& clauses) override;

	/// Get solver statistics.
	void printStatistics();

	void printWinningLog() override;

	/// Set family from working strategy
	void setFamily(KissatFamily family) { this->family = family; };

	void computeFamily();

	/// Return the model in case of SAT result.
	std::vector<int> getModel();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	/// @brief Initializes the map @ref kissatOptions with the default configuration.
	void initkissatMABOptions();

	/// Constructor.
	KissatMABSolver(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~KissatMABSolver();

	std::vector<int> getFinalAnalysis();

	std::vector<int> getSatAssumptions();

	/// @brief A map mapping a kissat option name to its value.
	std::unordered_map<std::string, int> kissatMABOptions;

  protected:
	int bump_var;

	/// Pointer to a KissatMABSolver solver.
	kissat_mab* solver;

	/// Buffer used to import clauses (units included).

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Used to stop or continue the resolution.
	std::atomic<bool> stopSolver;

	KissatFamily family;

	/// Callback to export/import clauses used by real kissat.
	/* Decided to not use pointers to move because of c++ stl (cannot move an array into a vector,
	 * sharedPtr destruction) */
	friend char KissatMabImportUnit(void*, kissat*);
	/* With KissatMabImportClause(void *painless_interface, int *kclause, unsigned int *size): need to not
	 * use sharedPtr*/
	friend char KissatMabImportClause(void*, kissat*);
	friend char KissatMabExportClause(void*, kissat*);
};
