#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "utils/Parsers.hpp"
#include "utils/Threading.hpp"

#include <atomic>

#define LINGELING_

struct LGL;

/// Instance of a Lingeling solver
/// @ingroup solving_cdcl
class Lingeling : public SolverCdclInterface
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

	/// Print solver statistics.
	void printStatistics();

	void printWinningLog() override;

	/// Return the model in case of SAT result.
	std::vector<int> getModel();

	/// Native diversification.
	void diversify(const SeedGenerator& getSeed) override;

	std::vector<int> getFinalAnalysis();

	std::vector<int> getSatAssumptions();

	/// Constructor.
	Lingeling(int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Copy constructor.
	Lingeling(const Lingeling& other, int id, const std::shared_ptr<ClauseDatabase>& clauseDB);

	/// Destructor.
	~Lingeling();

  private:
	// Common Initialization in Constructors
	void initialize();

  protected:
	/// Pointer to a Lingeling solver
	LGL* solver;

	/// Boolean used to determine if the resolution should stop
	std::atomic<bool> stopSolver;

	/// Buffer used to add permanent clauses.
	ClauseBuffer clausesToAdd;

	/// Buffer used to import units.
	boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>> unitsToImport;

	/// Size of the unit array used by Lingeling.
	size_t unitsBufferSize;

	/// Size of the clauses array used by Lingeling.
	size_t clsBufferSize;

	/// Unit array used by Lingeling.
	int* unitsBuffer;

	/// Clauses array used by Lingeling.
	int* clsBuffer;

	/// Termination callback.
	friend int termCallback(void* solverPtr);

	/// Callback to export units.
	friend void produceUnit(void* sp, int lit);

	/// Callback to export clauses.
	friend void produce(void* sp, int* cls, int glue);

	/// Callback to import units.
	friend void consumeUnits(void* sp, int** start, int** end);

	/// Callback to import clauses.
	friend void consumeCls(void* sp, int** clause, int* glue);
};
