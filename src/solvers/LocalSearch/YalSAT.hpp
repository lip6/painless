#pragma once

#define YALSAT_

#include "LocalSearchInterface.hpp"

/* Includes not having any ifdef macro */
extern "C"
{
#include "yalsat/yals.h"
}

/**
 * @brief Implements LocalSearchInterface to interact with the YalSAT local
 * search solver.
 * @ingroup localSearch
 */
class YalSAT
  : public LocalSearchInterface
{
public:
  YalSAT(int _id, FullClauseReader fullReader);
  YalSAT(int _id,
         unsigned long flipsLimit,
         unsigned long maxNoise,
         FullClauseReader fullReader);

  ~YalSAT();

  uint getVariableCount();

  var_t getDivisionVariable();

  void setSolverInterrupt();

  void unsetSolverInterrupt();

  void setPhase(const var_t var, const bool phase);

  SatAnswer solve(cube_view_t cube);

  bool addClause(clause_view_t clause);

  /**
   * @brief Load clauses into CaDiCaL using the FullReaderCallback function
   * @return The number of clauses loaded during this call
   */
  uint loadClauses() override;

  void loadFormula(const char* filename);

  model_t getModel();

  std::string statisticsToString();

  void diversify(const SeedGenerator& getSeed);

  friend int YalSAT_terminate(void* p_yalsat);

private:
  void setOption(const std::string& key, int value) override;
  void setOption(const std::string& key, double value) override;

  Yals* solver;

  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  /// @brief State attribute to test if the solver should terminate or not
  std::atomic<bool> terminateSolver;

  /// @brief Stores the number of clauses in the formula fed to YalSAT
  unsigned long clausesCount;

  /// @brief The maximum number of flips the search can reach
  unsigned long m_flipsLimit;

  /// @brief The maximum noise in randomization
  unsigned long m_maxNoise;
};