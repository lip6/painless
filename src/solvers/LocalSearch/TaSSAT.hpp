#pragma once

#define TASSAT_

#include "solvers/LocalSearch/LocalSearchInterface.hpp"

/* Includes not having any ifdef macro */
extern "C"
{
#include "tassat/yals.h"
}

typedef double clsweight_t;

/*
 * TaSSAT is a dynamic local search implemented on YalSAT
 * (https://doi.org/10.1007/978-3-031-57246-3_3). It is a more simplified
 * version of the LiWet Algorithm from the same authors
 * (https://doi.org/10.1007/978-3-031-33170-1_27). The backend is a modified
 * version of https://doi.org/10.5281/zenodo.10042124, with more externalized
 * functions for more control from this C++ wrapper. The function
 * "yals_liwet_compute_uwrvs_top_n(Yals *, int *, int)" was added to get the n
 * best variables, using a lower bound search for ordered insert
 * "notyals_insert_var_in_sorted_array(Yals *, int *, int )".
 *
 * In this C++ Wrapper there is the random multiple pick mode (50% chance to
 * flip each pick), which can be used to accelerate the initial descent. We
 * imply that given a random initial assignment, there exist many paths to reach
 * local minimae, thus we have good chances to pick one of them by flipping
 * multiple variables at once. This mode may be useful when we have thousands of
 * unsatisfied clauses.
 *
 * @ingroup localSearch
 *
 * @warning When the multiple picks mode is enabled, the flips count doesn't
 * give a good description of the number of iterations. Please refer to
 * m_descentsCount instead for the number of times the algorithm reduced the
 * weight of unsats.
 */
class TaSSAT
  : public LocalSearchInterface
{
public:
  TaSSAT(int _id, FullClauseReader fullReader);
  TaSSAT(int _id,
         unsigned long flipsLimit,
         unsigned long maxNoise,
         FullClauseReader fullReader);

  ~TaSSAT();

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

  /// @brief Returns a hashmap relatting each clause to its TaSSAT weight
  /// @return an unordered_map of std::vector<int> to clsweight_t
  /// @warning The clause vector must be ordered in order for the equality
  /// operator to work correctly
  // std::unordered_map<ClikeClause, clsweight_t, ClauseUtils::ClauseHash>
  // getClauseWeights();

private:
  void setOption(const std::string& key, int value) override;
  void setOption(const std::string& key, double value) override;

  /// @brief Compute the weight to transfer given a victim weight. If the victim
  /// weight is equal to the initial weight we transfer initcpt * initialWeight;
  /// otherwise, we transfer currcpt * victimweight + basecpt * initialweight
  /// @param victimWeight
  /// @return the weight to substract from the victim satisfied clause and add
  /// to the unsat clause
  float tassatWeightToTransfer(const float victimWeight);

  /// @brief The inner loop of the local search, where
  /// @return SAT if solved, otherwise UNKNOWN if flips limit is reached
  SatAnswer simpleInnerLoop();

private:
  /// @brief FullReader callback that has access to all the clauses
  FullClauseReader mcbk_fullReader;

  /// @brief From which clause the solver will start reading
  uint m_fullReaderIndex;

  Yals* solver;

  /// @brief State attribute to test if the solver should terminate or not
  std::atomic<bool> terminateSolver;

  /// @brief The maximum number of flips the search can reach
  unsigned long m_flipsLimit;

  /// @brief The number of iteration where at least one variable was flipped
  unsigned long m_descentsCount;

  /// @brief The maximum noise in randomization
  unsigned long m_maxNoise;

  /// @brief Enable the multiple pick mode
  bool m_enableMultiplePicks;

  /// @brief The threshold on unsat clauses to reach in order for to have
  /// multiple picks when it is enabled
  unsigned int m_multiplePicksUnsatThreshold;

  /// @brief Ratio of a clause's initial weight to transfer for the first time
  float m_initpct = 1.f;
  /// @brief Ratio of initial weight to transfer (inflation)
  float m_basepct = 0.175f;
  /// @brief Ratio of current clause's weight to transfer
  float m_currpct = 0.075f;
  /// @brief Initial clause weight
  clsweight_t m_initialweight = 100.f;
  /// @brief Probability to pick a random satisfied clause instead of a
  /// satisfied neighbor clause for weight transfer
  float m_randomPick = 0.1f;

  // Inner Working

  /// @brief Random engine
  std::mt19937 rng_engine;

  /// @brief Stores the indices of the picked variables in the inner array of
  /// TaSSAT
  std::vector<int> picksIndices;
};