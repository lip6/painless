// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include <map>
#include <random>

#include <Eigen/SparseCore>

#include "utils/SatUtils.h"
#include "Entity.hpp"
#include "preprocessors/PreprocessInterface.h"

//===============litQueue.h====================

/// @brief queuePair struct used for the priorityQueue in solve
typedef struct
{
    int lit;
    unsigned occurencesCount;
} queuePair;

/* Notes from cppreference:
Note that the Compare parameter is defined such that it returns true if its first argument
comes before its second argument in a weak ordering. But because the priority queue outputs
largest elements first, the elements that "come before" are actually output last.
That is, the front of the queue contains the "last" element according to the weak ordering
imposed by Compare.
*/
bool decreasingOrder(const queuePair &lhs, const queuePair &rhs);

bool increasingOrder(const queuePair &lhs, const queuePair &rhs);

bool randomOrder(const queuePair &lhs, const queuePair &rhs);

/// Functor for the priority queue used in solve()
struct PairCompare
{
    typedef bool (*compareFunc)(const queuePair &, const queuePair &);

    PairCompare(compareFunc func) : func(func) {}

    PairCompare() : func(decreasingOrder) {}

    bool operator()(const queuePair &lhs, const queuePair &rhs) const
    {
        return func(lhs, rhs);
    }

    compareFunc func;
};

//=============================================

// Occurence lists
#define PLIT_IDX(LIT) (LIT * 2 - 2)
#define NLIT_IDX(LIT) (-LIT * 2 - 1)
#define LIT_IDX(LIT) (LIT > 0 ? PLIT_IDX(LIT) : NLIT_IDX(LIT))
#define REAL_LIT_COUNT(LIT) ((unsigned int)this->litToClause[LIT_IDX(LIT)].size() + this->litCountAdjustement[LIT_IDX(LIT)])

// Adjacency Matrix
#define MATRIX_VAR_TO_IDX(LIT) (LIT - 1)
#define MATRIX_LIT_TO_IDX(LIT) (LIT > 0 ? LIT - 1 : -LIT - 1)
#define MATRIX_IDX_TO_VAR(IDX) (IDX + 1)

/// Tie-Breaking Heuristics
enum SBVATieBreak
{
    NONE = 1,
    THREEHOPS = 2,
    MOSTOCCUR = 3,
    LEASTOCCUR = 4,
    RANDOM = 5,
};

/* Do the same as isClauseDeleted ? Instead of a 16B struct */
struct ProofClause
{
    std::vector<int> lits;
    bool isAddition;
};

/// \ingroup solving

/// @brief SBVA preprocessing, it is the original implementation of https://github.com/hgarrereyn/SBVA that was reorganized
class StructuredBVA : public PreprocessInterface, public Entity
{
public:
    /// Constructor.
    StructuredBVA(int _id);

    /// Destructor.
    ~StructuredBVA();

    unsigned getVariablesCount();

    unsigned getDivisionVariable();

    void setInterrupt();

    void unsetInterrupt();

    void run();

    void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVariables);

    void loadFormula(const char *filename);

    void printStatistics();

    std::vector<simpleClause> getClauses();

    std::size_t getClausesCount() { return this->clauses.size() - this->adjacencyDeleted ;}

    unsigned getNbClausesDeleted() { return this->adjacencyDeleted; }

    void printParameters();

    /// Native diversification.
    /// Generates diversified data by adding noise using a uniform distribution.
    /// @param rng_engine The random number generator engine to use for generating random numbers.
    /// @param uniform_dist The uniform distribution for generating noise uniformly.
    void diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist);

    void updateAdjacencyMatrix(int var);

    unsigned getThreeHopHeuristic(int lit1, int lit2);

    /* returns the least occuring literal in a clause c\var */
    int leastFrequentLiteral(simpleClause &clause, int lit);

    void setTieBreakHeuristic(SBVATieBreak tieBreak);

    void clearAll()
    {
        this->tempTieHeuristicCache.clear();
        this->litCountAdjustement.clear();
        this->adjacencyMatrix.clear();
        this->isClauseDeleted.clear();
        this->litToClause.clear();
        this->clauses.clear();
        this->proof.clear();
    }

    void restoreModel(std::vector<int> &model)
    {
        model.resize(this->varCount - this->replacementsCount);
    }


    /* TODO factorize using a macro */
    inline int threeHopTieBreak(const std::vector<int> &ties, const int currentLit)
    {
        int lmax = 0;
        /* How much currentLit.lit is connected to ties[i] */
        unsigned maxHeuristicVal = 0;
        for (int tie : ties)
        {
            unsigned temp = this->getThreeHopHeuristic(currentLit, tie);
            if (temp > maxHeuristicVal)
            {
                maxHeuristicVal = temp;
                lmax = tie;
            }
        }
        return lmax;
    }

    inline int mostOccurTieBreak(const std::vector<int> &ties, const int currentLit)
    {
        int lmax = 0;
        /* How much currentLit.lit is connected to ties[i] */
        unsigned maxHeuristicVal = 0;
        for (int tie : ties)
        {
            unsigned temp = REAL_LIT_COUNT(tie);
            if (temp > maxHeuristicVal)
            {
                maxHeuristicVal = temp;
                lmax = tie;
            }
        }
        return lmax;
    }

    inline int leastOccurTieBreak(const std::vector<int> &ties, const int currentLit)
    {
        int lmax = 0;
        /* How much currentLit.lit is connected to ties[i] */
        unsigned maxHeuristicVal = UINT32_MAX;
        for (int tie : ties)
        {
            unsigned temp = REAL_LIT_COUNT(tie);
            if (temp < maxHeuristicVal)
            {
                maxHeuristicVal = temp;
                lmax = tie;
            }
        }
        return lmax;
    }

    inline int randomTieBreak(const std::vector<int> &ties, const int currentLit)
    {
        std::srand(currentLit);
        return ties[std::rand() % ties.size()];
    }

private:
    std::atomic<bool> stopPreprocessing;

    /// @brief Vector of clauses
    std::vector<simpleClause> clauses;

    /// @brief Instead of using another struct for clauses
    std::vector<bool> isClauseDeleted;

    /// @brief Occurence list using the literal as an index for a list of clause indexes in this->clauses
    std::vector<std::vector<unsigned>> litToClause;

    /// @brief To keep track of the real number of occurences during the algorithm
    std::vector<int> litCountAdjustement;

    /// @brief For the eigenVector size
    unsigned adjacencyMatrixWidth;

    std::vector<Eigen::SparseVector<int>> adjacencyMatrix;

    std::map<int, int> tempTieHeuristicCache;

    /// @brief Stores the DRAT proof if enabled
    std::vector<ProofClause> proof;

    // Options
    //-------
    bool generateProof;

    bool preserveModelCount;

    bool shuffleTies;

    PairCompare pairCompare;

    SBVATieBreak tieBreakHeuristic;

    unsigned maxReplacements;

    std::function<int(const std::vector<int> &, const int)> breakTie;

    // Stats
    //------
    //  TODO make a stats structure if more stats are tracked

    unsigned varCount;

    unsigned adjacencyDeleted;

    unsigned replacementsCount;
};
