#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "utils/Threading.hpp"

#include <memory>

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
class MiniSat : public SolverCDCLInterface
{
public:
  /// Constructor.
  MiniSat(int id,
          const std::shared_ptr<ClauseDatabase>& clauseDB,
          FullClauseReader fullReader);

  /// Destructor.
  ~MiniSat();

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

  /// @brief Database used to import clauses. Can be common with other solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /* Sharing */
  bool importClause(const ClauseExchangePtr& clause) override;

  /* Variable Management */
  uint getVariableCount() override;
  uint getDivisionVariable() override;
  void setPhase(const var_t var, const bool phase) override;
  void bumpVariableActivity(const var_t var, const int times) override;

  /* Result & Solution */
  clause_t getFinalAnalysis() override;
  cube_t getSatAssumptions() override;
  model_t getModel() override;

  /* Statistics And More */
  std::string statisticsToString() override;

protected:
  /**
 * @brief Sets a MiniSat solver configuration option by name.
 *
 * Translates a string-based key/value pair into the corresponding solver
 * parameter.  Integer values are assigned directly to integer and boolean
 * options; for floating-point options the value is divided by 1 000 so that
 * callers can express fractions as integer thousandths
 * (e.g. pass 500 for 0.5, pass 1100 for 1.1).
 *
 * @param key   Option name (kebab-case). Recognised keys:
 *
 *   | Key                            | Solver field                        | Type   | Notes                                                        |
 *   |--------------------------------|-------------------------------------|--------|--------------------------------------------------------------|
 *   | "verbosity"                    | verbosity                           | int    | Logging verbosity level.                                     |
 *   | "var-decay"                    | var_decay                           | double | Variable activity decay factor (÷1 000).                     |
 *   | "clause-decay"                 | clause_decay                        | double | Clause activity decay factor (÷1 000).                       |
 *   | "random-var-freq"              | random_var_freq                     | double | Frequency of random variable selection (÷1 000).             |
 *   | "seed"                         | random_seed                         | double | Random seed (assigned as-is).                                |
 *   | "luby-restart"                 | luby_restart                        | bool   | Use Luby restart sequence (0 = off, non-zero = on).          |
 *   | "ccmin-mode"                   | ccmin_mode                          | int    | Conflict clause minimisation (0=none, 1=basic, 2=deep).      |
 *   | "phase-saving"                 | phase_saving                        | int    | Phase saving level (0=none, 1=limited, 2=full).              |
 *   | "rnd-pol"                      | rnd_pol                             | bool   | Random branching polarities (0 = off, non-zero = on).        |
 *   | "rnd-init-act"                 | rnd_init_act                        | bool   | Randomise initial variable activities (0 = off, non-zero = on). |
 *   | "garbage-frac"                 | garbage_frac                        | double | Wasted-memory fraction before GC is triggered (÷1 000).      |
 *   | "min-learnts-lim"              | min_learnts_lim                     | int    | Minimum learnt-clause limit.                                 |
 *   | "restart-first"                | restart_first                       | int    | Initial restart limit (default 100).                         |
 *   | "restart-inc"                  | restart_inc                         | double | Restart limit multiplier per restart (÷1 000, default 1.5).  |
 *   | "learntsize-factor"            | learntsize_factor                   | double | Initial learnt-clause limit as a factor of original clauses (÷1 000, default 1/3). |
 *   | "learntsize-inc"               | learntsize_inc                      | double | Learnt-clause limit multiplier per restart (÷1 000, default 1.1). |
 *   | "learntsize-adjust-start-confl"| learntsize_adjust_start_confl       | int    | Conflict count before first learnt-size adjustment.          |
 *   | "learntsize-adjust-inc"        | learntsize_adjust_inc               | double | Learnt-size adjustment increment (÷1 000).                   |
 *
 * @param value Integer-encoded value.  For double options, the actual solver
 *              value is @p value / 1000.  For bool options, 0 is false and
 *              any non-zero value is true.
 *
 * @note Unrecognised keys are logged as a warning via LOGWARN and silently
 *       ignored.
 */
  void setOption(const std::string& key, int value) override;

  /// Pointer to a Minisat solver.
  Minisat::SimpSolver* solver;

  /// Database used to import units.
  boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>>
    m_unitsToImport;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  /// The same vector is used to add clauses into minisat to reduce the number
  /// of allocations
  std::unique_ptr<Minisat::vec<Minisat::Lit, int>> mcls;

  /// Callback to export/import clauses.
  friend void minisatExportClause(void*, Minisat::vec<Minisat::Lit, int>&);
  friend Minisat::Lit minisatImportUnit(void*);
  friend bool minisatImportClause(void*, Minisat::vec<Minisat::Lit, int>&);
};
