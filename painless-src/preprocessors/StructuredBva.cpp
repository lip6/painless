#include "StructuredBva.hpp"
#include "utils/Parameters.h"
#include <random>
#include <unordered_set>
#include "utils/ErrorCodes.h"
#include "utils/System.h"

bool decreasingOrder(const queuePair &lhs, const queuePair &rhs)
{
    return lhs.occurencesCount < rhs.occurencesCount;
}

bool increasingOrder(const queuePair &lhs, const queuePair &rhs)
{
    return lhs.occurencesCount > rhs.occurencesCount;
}

bool randomOrder(const queuePair &lhs, const queuePair &rhs)
{
    std::srand(lhs.lit);
    return std::rand() % 2 == 0;
}

StructuredBVA::StructuredBVA(int _id) : Entity(_id)
{
    /* Status */
    this->initialized = false;
    /* Options */
    this->maxReplacements = Parameters::getIntParam("sbva-max-add", 0); /*no limit*/
    this->generateProof = false;
    this->preserveModelCount = false;
    this->tieBreakHeuristic = SBVATieBreak::THREEHOPS;
    this->breakTie = std::bind(&StructuredBVA::threeHopTieBreak, this, std::placeholders::_1, std::placeholders::_2);
    this->shuffleTies = false;
    this->stopPreprocessing = false;

    /* Stats */
    this->varCount = 0;
    this->adjacencyDeleted = 0;
    this->replacementsCount = 0;
}

StructuredBVA::~StructuredBVA()
{
    LOGDEBUG1("SBVA %d deleted!", this->getId());
}

unsigned StructuredBVA::getVariablesCount()
{
    return this->varCount;
}

unsigned StructuredBVA::getDivisionVariable()
{
    return (rand() % getVariablesCount()) + 1;
}

void StructuredBVA::setInterrupt()
{
    if (!this->stopPreprocessing)
    {
        LOG1("Asked SBVA %d to terminate", this->id);
        this->stopPreprocessing = true;
    }
}

void StructuredBVA::unsetInterrupt()
{
    this->stopPreprocessing = false;
}

void StructuredBVA::updateAdjacencyMatrix(int var)
{
    assert(var > 0);
    if (this->adjacencyMatrix[MATRIX_VAR_TO_IDX(var)].nonZeros() > 0)
    {
        // already done (it is cleared when needed, a var is added)
        return;
    }

    Eigen::SparseVector<int> vec(this->adjacencyMatrixWidth);

    /* For each clause in occurence list, update the number of times, lit is in the same clause with other literals*/
    for (unsigned clauseIdx : this->litToClause[PLIT_IDX(var)])
    {
        if (this->isClauseDeleted[clauseIdx])
        {
            continue;
        }

        for (int lit : this->clauses[clauseIdx])
        {
            vec.coeffRef(MATRIX_LIT_TO_IDX(lit))++;
        }
    }

    /* Same for negative lit*/
    for (unsigned clauseIdx : this->litToClause[NLIT_IDX(-var)])
    {
        if (this->isClauseDeleted[clauseIdx])
        {
            continue;
        }
        for (int lit : this->clauses[clauseIdx])
        {
            vec.coeffRef(MATRIX_LIT_TO_IDX(lit))++;
        }
    }

    this->adjacencyMatrix[MATRIX_VAR_TO_IDX(var)] = std::move(vec);
}

unsigned StructuredBVA::getThreeHopHeuristic(int lit1, int lit2)
{
    unsigned var1 = std::abs(lit1);
    unsigned var2 = std::abs(lit2);

    /* If lit2 has its total_count already computed*/
    if (this->tempTieHeuristicCache.find(MATRIX_VAR_TO_IDX(var2)) != this->tempTieHeuristicCache.end())
    {
        return this->tempTieHeuristicCache[MATRIX_VAR_TO_IDX(var2)];
    }
    /* Update_adjacency matrix here because since there may be added variables */

    this->updateAdjacencyMatrix(var1); /* update in case needed */
    this->updateAdjacencyMatrix(var2); /* update in case needed */

    Eigen::SparseVector<int> &vec1 = this->adjacencyMatrix[MATRIX_VAR_TO_IDX(var1)];
    Eigen::SparseVector<int> &vec2 = this->adjacencyMatrix[MATRIX_VAR_TO_IDX(var2)];

    unsigned totalCount = 0;
    unsigned var;
    unsigned count;

    /* For each neighbor of var2 */
    for (int *idxPtr = vec2.innerIndexPtr(); idxPtr < vec2.innerIndexPtr() + vec2.nonZeros(); idxPtr++)
    {
        // *idxPtr is a matrix index
        var = MATRIX_IDX_TO_VAR(*idxPtr);
        count = vec2.coeffRef(*idxPtr);

        this->updateAdjacencyMatrix(var); /* update if needed: source of bug */
        Eigen::SparseVector<int> *vec3 = &this->adjacencyMatrix[*idxPtr];
        /* dot : returns the sum of products of the adjacencies of neighbors var and var2 have in commone
                The sum is then multiplied by the adjency of var2 with var */
        /* the more var and var1 have the same neighbors, the greater the weight. And the more var2 is connected to var, the greater the weight*/
        totalCount += count * vec3->dot(vec1);
    }

    this->tempTieHeuristicCache[MATRIX_VAR_TO_IDX(var2)] = totalCount;
    return totalCount;
}

int StructuredBVA::leastFrequentLiteral(simpleClause &clause, int elit)
{
    int leastOccuringLit = 0;
    int occurenceCount = INT32_MAX;
    int tempCount;

    for (int clit : clause)
    {
        if (clit == elit)
            continue;

        tempCount = REAL_LIT_COUNT(clit);

        if (tempCount < occurenceCount)
        {
            occurenceCount = tempCount;
            leastOccuringLit = clit;
        }
    }
    return leastOccuringLit;
}

void StructuredBVA::addInitialClauses(const std::vector<simpleClause> &initClauses, unsigned nbVariables)
{
    // if (initClauses.size() > Parameters::getIntParam("sbva-max-clause", MILLION * 10))
    // {
    //     LOGWARN("The number of clauses is too important to use SBVA, returning");
    //     this->initialized = false;
    //     return;
    // }
    std::unordered_set<simpleClause, clauseHash> clausesCache;
    unsigned duplicatesCount = 0;
    unsigned nbClauses = initClauses.size();
    simpleClause tmpClause;
    unsigned actualIdx = 0; /* since duplicates may exists */

    this->clauses.reserve(nbClauses);
    this->isClauseDeleted.reserve(nbClauses);
    this->litToClause.resize(2 * nbVariables);
    this->litCountAdjustement.resize(2 * nbVariables);

    for (unsigned i = 0; i < nbClauses && !this->stopPreprocessing; i++)
    {
        tmpClause = initClauses[i];
        std::sort(tmpClause.begin(), tmpClause.end());
        auto insertTest = clausesCache.insert(tmpClause);
        if (!insertTest.second)
        {
            duplicatesCount++;
            continue;
        }
        else
        {
            for (int lit : tmpClause)
            {
                this->litToClause[LIT_IDX(lit)].push_back(actualIdx);
            }
            this->clauses.push_back(std::move(tmpClause));
            this->isClauseDeleted.push_back(0);
            actualIdx++;
        }
    }

    if (this->stopPreprocessing)
    {
        LOGDEBUG1("[SBVA %d] stopped at addInitialClauses", this->id);
        return;
    }

    this->adjacencyMatrixWidth = nbVariables * 4;

    if (this->tieBreakHeuristic == SBVATieBreak::THREEHOPS)
    {
        adjacencyMatrix.resize(nbVariables);
        for (unsigned i = 1; i < this->varCount; i++)
        {
            this->updateAdjacencyMatrix(i);
        }
    }

    if (this->stopPreprocessing)
    {
        LOGDEBUG1("[SBVA %d] stopped at addInitialClauses after updateMatrices", this->id);
        return;
    }

    this->varCount = nbVariables;
    this->initialized = true;
    LOG1("Loaded all clauses in SBVA %d, duplicates detected %d", id, duplicatesCount);
}

void StructuredBVA::loadFormula(const char *filename)
{
    if (!parseFormulaForSBVA(filename, this->clauses, this->litToClause, this->isClauseDeleted, &(this->varCount)))
    {
        this->clearAll();
        LOGERROR("Error at parsing!");
        this->initialized = false;
        return;
    }

    this->adjacencyMatrixWidth = this->varCount * 4;
    this->litCountAdjustement.resize(2 * this->varCount);

    adjacencyMatrix.resize(this->varCount);

    this->initialized = true;
}

std::vector<simpleClause> StructuredBVA::getClauses()
{
    if (!this->initialized)
        return {};
    std::vector<simpleClause> actualClauses;
    unsigned nbClauses = this->clauses.size();
    for (unsigned i = 0; i < nbClauses; i++)
    {
        if (!this->isClauseDeleted[i])
        {
            actualClauses.push_back(clauses[i]);
        }
    }
    return actualClauses;
}

void StructuredBVA::setTieBreakHeuristic(SBVATieBreak heuristic)
{
    this->tieBreakHeuristic = heuristic;
    switch (heuristic)
    {
    case SBVATieBreak::THREEHOPS:
        this->breakTie = std::bind(&StructuredBVA::threeHopTieBreak, this, std::placeholders::_1, std::placeholders::_2);
        break;
    case SBVATieBreak::MOSTOCCUR:
        this->breakTie = std::bind(&StructuredBVA::mostOccurTieBreak, this, std::placeholders::_1, std::placeholders::_2);
        break;
    case SBVATieBreak::LEASTOCCUR:
        this->breakTie = std::bind(&StructuredBVA::leastOccurTieBreak, this, std::placeholders::_1, std::placeholders::_2);
        break;
    case SBVATieBreak::RANDOM:
        this->breakTie = std::bind(&StructuredBVA::randomTieBreak, this, std::placeholders::_1, std::placeholders::_2);
        break;
    default:
        this->tieBreakHeuristic = SBVATieBreak::NONE;
    }
}

void StructuredBVA::printStatistics()
{
    LOG1("[SBVA %d] varCount: %u, realClauseCount: %lu, adjacencyDeleted: %u, replacementsCount: %u",
            this->id,
            this->varCount,
            this->clauses.size() - this->adjacencyDeleted,
            this->adjacencyDeleted,
            this->replacementsCount);
}

void StructuredBVA::printParameters()
{
    LOG1("[SBVA %d] generateProof: %s, preserveModelCount: %s, maxReplacements: %u, pairCompare: %s, tieBreakHeuristic: %s",
            this->id,
            this->generateProof ? "true" : "false",
            this->preserveModelCount ? "true" : "false",
            this->maxReplacements,
            (pairCompare.func == decreasingOrder) ? "decreasingOrder" : (pairCompare.func == increasingOrder) ? "increasingOrder"
                                                                    : (pairCompare.func == randomOrder)       ? "randomOrder"
                                                                                                              : "unknown",
            (tieBreakHeuristic == NONE) ? "NONE" : (tieBreakHeuristic == THREEHOPS) ? "THREEHOPS"
                                               : (tieBreakHeuristic == MOSTOCCUR)   ? "MOSTOCCUR"
                                               : (tieBreakHeuristic == LEASTOCCUR)  ? "LEASTOCCUR"
                                               : (tieBreakHeuristic == RANDOM)      ? "RANDOM"
                                                                                    : "unknown");
}

void StructuredBVA::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
    /* TODO deterministic div: ids are successive so modulo would suffice */
    // Default
    this->setTieBreakHeuristic(SBVATieBreak::THREEHOPS);
    this->pairCompare.func = decreasingOrder;

    unsigned sbvaCount = Parameters::getIntParam("sbva-count", 12);
    if(!sbvaCount) return;
    int tempId = this->id % sbvaCount;
    // 2 Configuration
    // if( 1 == tempId)
    // {
    //     this->setTieBreakHeuristic(SBVATieBreak::MOSTOCCUR);
    // this->pairCompare.func = decreasingOrder;
    // }

    // 12 configurations
    switch (tempId % 3)
    {
    case 1:
        this->pairCompare.func = randomOrder;
        break;
    case 2:
        this->pairCompare.func = increasingOrder;
        break;
        // default decreasing
    }

    if (tempId > 2 && tempId < 6) // [3,5]
        this->setTieBreakHeuristic(SBVATieBreak::MOSTOCCUR);
    else if (tempId > 5 && tempId < 9) // [6, 8]
        this->setTieBreakHeuristic(SBVATieBreak::LEASTOCCUR);
    else if (tempId > 8 && tempId < 12) // [9, 11]
        this->setTieBreakHeuristic(SBVATieBreak::RANDOM);

    if(!Parameters::getBoolParam("-no-sbva-shuffle"))
    this->shuffleTies = true;
}
