#pragma once

#include "SolverCdclInterface.hpp"
#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Threading.hpp"

#define GLUCOSE_

// Some forward declatarations for Glucose
namespace Glucose {
class ParallelSolver;
class Lit;
template<class T>
class vec;
class Clause;
}

/// Instance of a Glucose solver
/// @ingroup solving_cdcl
class GlucoseSyrup : public SolverCdclInterface
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

	void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars) override;

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

	void getHeuristicData(std::vector<int>** flipActivity,
						  std::vector<int>** nbPropagations,
						  std::vector<int>** nbDecisionVar);

	void setHeuristicData(std::vector<int>* flipActivity,
						  std::vector<int>* nbPropagations,
						  std::vector<int>* nbDecisionVar);

	std::vector<int> getFinalAnalysis();

	std::vector<int> getSatAssumptions();

	/// Constructor.
	GlucoseSyrup(int id,
				 const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Copy constructor.
	GlucoseSyrup(const GlucoseSyrup& other,
				 int id,
				 const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	virtual ~GlucoseSyrup();

  protected:
	/// Pointer to a Glucose solver.
	Glucose::ParallelSolver* solver;

	/// Database used to import units. (TODO use a lockfree queue as in lingeling)
	std::unique_ptr<ClauseDatabase> unitsToImport;	

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Callback to export unit clauses.
	friend void glucoseExportUnary(void*, Glucose::Lit&);

	/// Callback to export clauses.
	friend void glucoseExportClause(void*, Glucose::Clause&);

	/// Callback to import unit clauses.
	friend Glucose::Lit glucoseImportUnary(void*);

	/// Callback to import clauses.
	friend bool glucoseImportClause(void*, int*, int*, Glucose::vec<Glucose::Lit>&);

  public:
	;
};