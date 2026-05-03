#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "utils/Parsers.hpp"
#include "utils/Threading.hpp"

#include <atomic>

#define LINGELING_

struct LGL;

/// Instance of a Lingeling solver
/// @ingroup solving_cdcl
class Lingeling : public SolverCDCLInterface
{
public:
  /// Constructor.
  Lingeling(int id,
            const std::shared_ptr<ClauseDatabase>& clauseDB,
            FullClauseReader fullReader);
  /// Destructor.
  ~Lingeling();

  /* Execution */
  SatAnswer solve(cube_view_t cube) override;
  void setSolverInterrupt() override;
  void unsetSolverInterrupt() override;
  void diversify(const SeedGenerator& getSeed) override;

  /* Clause Management */
  void loadFormula(const char* filename) override;
  /**
   * @brief Add an irredundant clause to the solver (not thread-safe!).
   * @details The clause is directly added to the backend solver.
   * @param clause The clause to add
   * @warning This method is not thread safe, i.e. multiple threads cannot add
   * clauses concurrently to this solver
   */
  bool addClause(clause_view_t clause) override;
  /**
   * @brief Load clauses into CaDiCaL using the FullReaderCallback function
   * @return The number of clauses loaded during this call
   */
  uint loadClauses() override;

  /* Sharing */
  bool importClause(const ClauseExchangePtr& clause) override;

  /* Variable Management */
  uint getVariableCount() override;
  var_t getDivisionVariable() override;
  void setPhase(const var_t var, const bool phase) override;
  void bumpVariableActivity(const var_t var, const int times) override;

  /* Result & Solution */
  clause_t getFinalAnalysis() override;
  cube_t getSatAssumptions() override;
  model_t getModel() override;

  /* Statistics And More */
  std::string statisticsToString() override;

private:
  /**
   * @brief Sets a MapleCOMSPS solver configuration option by name.
   *
   * Translates a string-based key/value pair into the corresponding solver
   * parameter.  Integer values are assigned directly to integer and boolean
   * options; for floating-point options the value is divided by 1 000 so that
   * callers can express fractions as integer thousandths
   * (e.g. pass 500 for 0.5, pass 1100 for 1.1).
   *
   * @param key   Option name (kebab-case).
   *
   * @param value Integer-encoded value.  For double options, the actual solver
   *              value is @p value / 1000.  For bool options, 0 is false and
   *              any non-zero value is true.
   *
   * @note Unrecognised keys are logged as a warning via LOGWARN and silently
   *       ignored.
   */
  void setOption(const std::string& key, int value) override;

  // Common Initialization in Constructors
  void initialize();

protected:
  /// Pointer to a Lingeling solver
  LGL* solver;

  /// Boolean used to determine if the resolution should stop
  std::atomic<bool> stopSolver;

  /// Buffer used to import units.
  boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>>
    m_unitsToImport;

  /// @brief Database used to import clauses. Can be common with other solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /// Size of the unit array used by Lingeling.
  size_t unitsBufferSize;

  /// Size of the clauses array used by Lingeling.
  size_t clsBufferSize;

  /// Unit array used by Lingeling.
  int* unitsBuffer;

  /// Clauses array used by Lingeling.
  int* clsBuffer;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

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
