#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Threading.hpp"
#include <fstream>
#include <unordered_map>

#include "SolverCDCLInterface.hpp"

#define KISSAT_

// Kissat includes
extern "C"
{
#include <kissat/src/kissat.h>
}

/// Instance of a Kissat solver
/// @ingroup solving_cdcl
class Kissat : public SolverCDCLInterface
{
public:
  Kissat(int id,
         const std::shared_ptr<ClauseDatabase>& clauseDB,
         FullClauseReader fullReader);
  ~Kissat();

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

protected:
  void setOption(const std::string& key, int value) override;

  /// @brief Code for the Family of a CDCL solver at diversification
  enum class Family
  {
    SAT_STABLE = 0,
    MIXED_SWITCH,
    UNSAT_FOCUSED,
    COUNT
  };

  /// @brief Initializes the map @ref kissatOptions with the default
  /// configuration.
  void initKissatOptions();

  /**
   * @brief Send an imported clause in @ref m_clausesToImport to the backend
   * solver
   * @return True if the clause was accepted by the backend, false otherwise
   */
  bool backendImportClause();

  bool backendHasClauseToImport();

  /**
   * @brief Export a learned clause by the backend solver using @ref
   * exportClause from @ref SharingEntity
   * @return True if the clause was exported, false otherwise
   */
  bool backendExplortClause();

protected:
  /// Pointer to the backend Kissat solver.
  kissat* m_solver;

  /// Used to stop or continue the resolution.
  std::atomic<bool> m_stopSolver;

  /// Number of variables before solving
  unsigned int m_originalVars;

  /// @brief Database used to import clauses. Can be common with other solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  std::unordered_map<std::string, int> m_kissatParameters;

  std::vector<lit_t> m_assumptions;

  std::vector<lit_t> m_clauseToExport;

  /// A pointer pointing to the next clause to be exported
  ClauseExchangePtr m_clauseToImport;

  // Kissat Backend Callbacks
  // ========================

  /**
   * @brief C Callback termination check
   * @param vpkissat pointer to the painless Kissat object
   * @return 0 if to continue, !0 if to terminate
   */
  friend int kissatTerminate(void* vpkissat);

  friend unsigned char kissatHasClauseToImport(void* vpkissat);

  /**
   * @brief C Callback to get a clause to import into Kissat Backend
   * @param vpkissat pointer to the painless Kissat object
   * @return 0 if the clause was not imported, !0 otherwise
   */
  friend unsigned char kissatImportClause(void* vpkissat);

  /**
   * @brief C Callback to export a clause from Kissat Backend
   * @param vpkissat pointer to the painless Kissat object
   * @return 0 if the clause was not exported, !0 otherwise
   */
  friend char kissatExportClause(void* vpkissat);
};
