#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseUtils.hpp"

#include "utils/Threading.hpp"

#include <fstream>
#include <unordered_map>

#include "SolverCDCLInterface.hpp"

#define CADICAL_

// Cadical includes
#include "cadical/src/cadical.hpp"

/*---------------------Main Class----------------------*/

/**
 * @brief Implements the different interfaces to interact with the Cadical CDCL
 * solver
 * @ingroup solving_cdcl
 */
class Cadical
  : public SolverCDCLInterface
  , public CaDiCaL::Learner
  , public CaDiCaL::Terminator
{
public:
  /// Constructor.
  Cadical(int id,
          const std::shared_ptr<ClauseDatabase>& clauseDB,
          FullClauseReader fullReader);

  /// Destructor.
  virtual ~Cadical();

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

  /* Statistics */
  std::string statisticsToString() override;

protected:
  void setOption(const std::string& key, int value) override;

  /// @brief Initializes the map @ref cadicalOptions with the default
  /// configuration.
  void initCadicalOptions();

  /// Pointer to a Cadical solver.
  std::unique_ptr<CaDiCaL::Solver> solver;

  /// Used to stop or continue the resolution.
  std::atomic<bool> stopSolver;

  /// @brief Database used to import clauses. Can be common with other solvers
  std::shared_ptr<ClauseDatabase> m_clausesToImport;

  /// @brief Database used to add irredundant clauses only. Can be common with
  /// other solvers
  // std::shared_ptr<ClauseDatabase> m_clausesToAdd;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  /*----------------------Learner------------------------*/
  /// @details It is important to note that the methods are not multi-thread
  /// safe
public:
  /* Export */
  /// @brief  Tells if CaDiCaL should export a clause
  /// @param size size of the clause to export
  /// @param glue lbd value of the clause to export
  /// @return true if the clause is to be exported, false otherwise
  bool learning(int size, int glue) override;

  /**
   * @brief Used by the base solver to export a clause
   * @param lit a literal of the clause to export (non zero integer), 0 if it is
   * the end
   */
  void learn(int lit) override;

  /* Import */
  /**
   * @brief Tells if CaDiCaL has a clause to import
   * @return true if there is one, false otherwise
   */
  bool hasClauseToImport() override;

  /**
   * @brief Loads the clause to import data in the parameters
   * @param clause holds the clause literals
   * @param glue holds the lbd value of the clause
   */
  void getClauseToImport(std::vector<int>& clause, int& glue) override;

private:
  /// A vector to store the clause to export
  clause_t m_tempClause;

  /// Stores the lbd value of the clause to export (loaded in learning)
  int m_tempLbd;

  /// A pointer pointing to the next clause to be exported (loaded in
  /// hasClauseToImport)
  ClauseExchangePtr m_tempClauseToImport;

  /*-----------------------Terminator----------------------*/
  /**
   * @brief Callback for the base solver to check if it should terminate or not
   * @return true if the base solver should terminate, false otherwise
   */
  bool terminate() { return this->stopSolver; }
};