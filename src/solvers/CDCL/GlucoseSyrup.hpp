#pragma once

#include "SolverCDCLInterface.hpp"
#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Threading.hpp"

#include <memory>

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
class GlucoseSyrup
  : public SolverCDCLInterface
{
public:
  /// Constructor.
  GlucoseSyrup(int id,
               const std::shared_ptr<ClauseDatabase>& clauseDB,
               FullClauseReader fullReader);

  /// Destructor.
  ~GlucoseSyrup();

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

  void getHeuristicData(std::vector<int>** flipActivity,
                        std::vector<int>** nbPropagations,
                        std::vector<int>** nbDecisionVar);

  void setHeuristicData(std::vector<int>* flipActivity,
                        std::vector<int>* nbPropagations,
                        std::vector<int>* nbDecisionVar);

protected:
  /**
   * @brief Sets a GlucoseSyrup solver configuration option by name.
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

  /// Pointer to a Glucose solver.
  Glucose::ParallelSolver* solver;

  /// Buffer used to import units.
  boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>>
    m_unitsToImport;

  /// @brief Database used to import shared clauses. Can be common with other
  /// solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /// The same vector is used to add clauses into minisat to reduce the number
  /// of allocations
  std::unique_ptr<Glucose::vec<Glucose::Lit>> gcls;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  /// Heuristic chosen
  int heuristic;

  /// Callback to export unit clauses.
  friend void glucoseExportUnary(void*, Glucose::Lit&);

  /// Callback to export clauses.
  friend void glucoseExportClause(void*, Glucose::Clause&);

  /// Callback to import unit clauses.
  friend Glucose::Lit glucoseImportUnary(void*);

  /// Callback to import clauses.
  friend bool glucoseImportClause(void*,
                                  int*,
                                  int*,
                                  Glucose::vec<Glucose::Lit>&);
};