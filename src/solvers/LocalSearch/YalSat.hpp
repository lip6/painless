#pragma once

#define YALSAT_

#include "LocalSearchInterface.hpp"

/* Includes not having any ifdef macro */
extern "C"
{
#include "yalsat/yals.h"
}

/**
 * @brief Implements LocalSearchInterface to interact with the YalSAT local search solver.
 * @ingroup localSearch
 */
class YalSat : public LocalSearchInterface
{
  public:
	YalSat(int _id, unsigned long flipsLimit, unsigned long maxNoise);

	~YalSat();

	unsigned int getVariablesCount();

	int getDivisionVariable();

	void setSolverInterrupt();

	void unsetSolverInterrupt();

	void setPhase(const unsigned int var, const bool phase);

	SatResult solve(const std::vector<int>& cube);

	void addClause(ClauseExchangePtr clause);

	void addClauses(const std::vector<ClauseExchangePtr>& clauses);

	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars) override;

	void addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars) override;

	void loadFormula(const char* filename);

	std::vector<int> getModel();

	void printStatistics();

	void printParameters();

	void diversify(const SeedGenerator& getSeed);

	friend int yalsat_terminate(void* p_YalSat);

  private:
	Yals* solver;

	/// @brief State attribute to test if the solver should terminate or not
	std::atomic<bool> terminateSolver;

	/// @brief Stores the number of clauses in the formula fed to YalSAT
	unsigned long clausesCount;

	/// @brief The maximum number of flips the search can reach
	unsigned long m_flipsLimit;

	/// @brief The maximum noise in randomization
	unsigned long m_maxNoise;
};