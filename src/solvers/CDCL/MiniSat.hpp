#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "utils/Threading.hpp"

#define MINISAT_

// Some forward declatarations for Minisat
namespace Minisat {
class SimpSolver;
class Lit;
template<class T, class _Size>
class vec;
}

/// Instance of a Minisat solver
/// @ingroup solving_cdcl
class MiniSat : public SolverCdclInterface
{
  public:
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
	bool importClause(const ClauseExchangePtr& clause);

	/// Add a list of learned clauses to the formula.
	void importClauses(const std::vector<ClauseExchangePtr>& clauses);

	/// Get solver statistics.
	void printStatistics();

	void printWinningLog() override;

	/// Return the model in case of SAT result.
	std::vector<int> getModel();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	std::vector<int> getFinalAnalysis();

	std::vector<int> getSatAssumptions();

	/// Constructor.
	MiniSat(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~MiniSat();

  protected:
	/// Pointer to a Minisat solver.
	Minisat::SimpSolver* solver;

	/// Database used to import units.
	std::unique_ptr<ClauseDatabase> unitsToImport;

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Size limit used to share clauses.
	std::atomic<int> sizeLimit;

	/// Callback to export/import clauses.
	friend void minisatExportClause(void*, Minisat::vec<Minisat::Lit, int>&);
	friend Minisat::Lit minisatImportUnit(void*);
	friend bool minisatImportClause(void*, Minisat::vec<Minisat::Lit, int>&);
};
