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

#define KISSAT_INC_

// KissatINCSolver includes
extern "C"
{
#include "kissat-inc/src/kissat.h"
}

// Wrapping struct
typedef struct kissat kissat_mab;

/// Instance of a KissatINCSolver solver
/// @ingroup solving_cdcl
class KissatINCSolver : public SolverCdclInterface
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

	/// Add a list of initial clauses to the formula.
	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) override;

	/// Add a learned clause to the formula.
	bool importClause(const ClauseExchangePtr& clause) override;

	/// Add a list of learned clauses to the formula.
	void importClauses(const std::vector<ClauseExchangePtr>& clauses) override;

	/// Get solver statistics.
	void printStatistics();

	void printWinningLog() override;

	void setFamily(KissatFamily family) { this->family = family; };

	/// Return the model in case of SAT result.
	std::vector<int> getModel();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	/// Constructor.
	KissatINCSolver(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~KissatINCSolver();

	std::vector<int> getFinalAnalysis();

	std::vector<int> getSatAssumptions();

  protected:
	int bump_var;

	/// Pointer to a KissatINCSolver solver.
	kissat_mab* solver;
	
	KissatFamily family;

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Used to stop or continue the resolution.
	std::atomic<bool> stopSolver;

	/// Callback to export/import clauses used by real kissat.
	friend char KissatIncImportClause(void*, kissat*);
	friend char KissatIncExportClause(void*, kissat*);
};
