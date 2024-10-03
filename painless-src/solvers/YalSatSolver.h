#pragma once

#define YALSAT_

#include "LocalSearchInterface.hpp"

/* Includes not having any ifdef macro */
extern "C"
{
#include <yalsat/yals.h>
}

class YalSat : public LocalSearchSolver
{
public:
    YalSat(int _id);

    ~YalSat();

    int getVariablesCount();

    int getDivisionVariable();

    void setSolverInterrupt();

    void unsetSolverInterrupt();

    void setPhase(const unsigned var, const bool phase);

    SatResult solve(const std::vector<int> &cube);

    void addClause(std::shared_ptr<ClauseExchange> clause);

    void addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses);

    void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars);

    void loadFormula(const char *filename);

    std::vector<int> getModel();

    void printStatistics();

    void printParameters();

    void diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist);

    friend int yalsat_terminate(void *p_YalSat);

private:
    Yals *solver;

    /// @brief State attribute to test if the solver should terminate or not
    std::atomic<bool> terminateSolver;

    unsigned long clausesCount;
};