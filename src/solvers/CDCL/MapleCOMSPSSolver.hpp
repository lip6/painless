#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "utils/Threading.hpp"

#include "SolverCDCLInterface.hpp"

#include <memory>

#define MAPLECOMSPS_

#define ID_SYM 0
#define ID_XOR 1

struct parameter
{
  int tier1;
  int chrono;
  int stable;
  int walkinitially;
  int target;
  int phase;
  int heuristic;
  int margin;
  int ccanr;
  int targetinc;
};

// Some forward declatarations for MapleCOMSPS
namespace MapleCOMSPS {
class SimpSolver;
class Lit;
template<class T>
class vec;
}

/// @ingroup solving_cdcl
/// Instance of a MapleCOMSPS solver
class MapleCOMSPSSolver : public SolverCDCLInterface
{
public:
  /// Constructor.
  MapleCOMSPSSolver(int id,
                    const std::shared_ptr<ClauseDatabase>& clauseDB,
                    FullClauseReader fullReader);

  /// Destructor.
  virtual ~MapleCOMSPSSolver();

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

  void setStrengthening(bool b);

  void setParameter(parameter p) {};

  void initshuffle(int id) {};

protected:
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
  
  /// Pointer to a MapleCOMSPS solver.
  MapleCOMSPS::SimpSolver* solver;

  /// Buffer used to import units.
  boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>>
    m_unitsToImport;

  /// @brief Database used to import clauses. Can be common with other solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /// The same vector is used to add clauses into minisat to reduce the number
  /// of allocations
  std::unique_ptr<MapleCOMSPS::vec<MapleCOMSPS::Lit>> mcls;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  /// Used to stop or continue the resolution.
  std::atomic<bool> stopSolver;

  /// Callback to export/import clauses.
  friend MapleCOMSPS::Lit cbkMapleCOMSPSImportUnit(void*);
  friend bool cbkMapleCOMSPSImportClause(void*,
                                         unsigned int*,
                                         MapleCOMSPS::vec<MapleCOMSPS::Lit>&);
  friend void cbkMapleCOMSPSExportClause(void*,
                                         unsigned int,
                                         MapleCOMSPS::vec<MapleCOMSPS::Lit>&);

public:
  ;
};
