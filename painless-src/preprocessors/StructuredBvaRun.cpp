#include "StructuredBva.hpp"
#include "painless.h"

#include <queue>
#include <unordered_set>
#include <set>

// Performs partial clause difference between clause1 and clause2, storing the result in diff.
// Only the first maxDiff literals are stored in diff.
// Requires that clause and other are sorted.
/* clause1 \ clause2 */
inline void orderedClauseSub(simpleClause &clause1, simpleClause &clause2, simpleClause &diff, int maxDiff)
{
    diff.clear();

    int idx1 = 0, idx2 = 0;
    int size1 = clause1.size(), size2 = clause2.size();

    while (idx1 < size1 && idx2 < size2 && diff.size() < maxDiff)
    {
        if (clause1[idx1] == clause2[idx2])
        {
            idx1++;
            idx2++;
        }
        else if (clause1[idx1] < clause2[idx2]) /* sure that not in clause2 if ordered*/
        {
            diff.push_back(clause1[idx1]);
            idx1++;
        }
        else /* lit in clause2 only: possible to have the two diff in the same call ? */
        {
            idx2++;
        }
    }

    while (idx1 < size1 && diff.size() < maxDiff)
    {
        diff.push_back(clause1[idx1]);
        idx1++;
    }
}

int static inline reduction(int lits, int clauses)
{
    return (lits * clauses) - (lits + clauses);
}

void StructuredBVA::run()
{
    if (!this->initialized)
    {
        LOGWARN("SBVA %d wasn't initialized correctly, cannot run, returning", this->getId());
        return;
    }
    /* A priority queue is instantiated to keep track of all the literals in pairs (num_clauses_occuring, lit) using a certain order */
    /* Functor to sort using the clause number only, thus order in decreasing number of clauses*/
    std::priority_queue<queuePair, std::vector<queuePair>, PairCompare> litQueue(this->pairCompare);

    /* Init queue with original literals, DIVERSIFICATION potentiel: change the order, do not add all literals ... */
    for (int i = 1; i <= this->varCount; i++)
    {
        litQueue.emplace(queuePair{i, REAL_LIT_COUNT(i)});
        litQueue.emplace(queuePair{-i, REAL_LIT_COUNT(-i)});

        LOGDEBUG2("Emplaced: (%d,%u), (%d,%u)", i, REAL_LIT_COUNT(i), -i, REAL_LIT_COUNT(-i));
    }

    // std::vector<int> matchedLiterals;
    std::unordered_set<int> matchedLiterals;
    /* Stores the index of the clauses in this->clauses vector */
    std::vector<int> matchedClauses;
    std::vector<int> matchedClausesSwap;
    /* Stores the index of the clauses in an occurence list == column in matchedEntries */
    std::vector<int> matchedClausesIdx; /* exists only for clause deletion */
    std::vector<int> matchedClausesIdxSwap;

    /* Stores the pair (global_clause_idx, in_occurence_list_idx) */
    std::vector<std::pair<int, int>> clausesToRemove;
    /* Init the diff vector used with this->clauseSub*/
    std::vector<int> diff;

    /* Keep track of the matrix of swaps to be performed, to be changed to a map of pairs, with lit as key ? */
    std::vector<std::tuple<int, int, int>> matchedEntries;

    /* Keep a list of the literals that are matched to sort and count them later */
    std::vector<int> matchedEntriesLits;

    /* Used for litQueue updates*/
    std::unordered_set<int> litsToUpdate;

    /* TODO Add a better reserve than the 10000, using the max_clause_size, nbVar, nbClauses ... */

    this->replacementsCount = 0;

    queuePair currentLit;

    while (litQueue.size() > 0)
    {
        /* Check if to stop */
        if (this->stopPreprocessing || globalEnding || (this->maxReplacements > 0 && this->replacementsCount >= this->maxReplacements))
        {
            LOG("SBVA %d is ending: stopPreprocessing %d, globalEnding %d, maxReplacement %u, replacementCount %u", this->getId(), this->stopPreprocessing.load(), globalEnding.load(), this->maxReplacements, this->replacementsCount);
            return;
        }

        /* Clear to not cancel the reserve */
        matchedLiterals.clear();
        matchedClauses.clear();
        matchedClausesIdx.clear();
        clausesToRemove.clear();
        this->tempTieHeuristicCache.clear();

        /* Get the least occuring literal to test */
        currentLit = litQueue.top();
        litQueue.pop();

        LOGDEBUG2("Trying literal %d (%u)", currentLit.lit, currentLit.occurencesCount);

        /* if the number of occurences is zero or are out of date, loop*/
        if (currentLit.occurencesCount == 0 || currentLit.occurencesCount != REAL_LIT_COUNT(currentLit.lit))
            continue;

        matchedLiterals.insert(currentLit.lit);

        /* Occurence list with the clauses indexes in this->clauses */
        std::vector<unsigned> &occurenceList = this->litToClause[LIT_IDX(currentLit.lit)];
        unsigned size = occurenceList.size();

        /* Matched clauses are init to all occurences of chosen literal */
        for (unsigned i = 0; i < size; i++)
        {
            if (!this->isClauseDeleted[occurenceList[i]])
            {
                matchedClauses.push_back(occurenceList[i]);
                matchedClausesIdx.push_back(i);
                clausesToRemove.emplace_back(occurenceList[i], i); /* TO OPTIMIZE: need to check at match if to remove or not!! */
            }
        }

        /* Search for potential matches with currentLit.lit */
        while (1)
        {
            matchedEntries.clear();
            matchedEntriesLits.clear();

            size = matchedClauses.size();

            // foreach C in matchedClauses check if there is a literal lmin in C having a clause D s.t D \ l2 == C \ l1 (lmin must be incommon)
            for (int i = 0; i < size; i++)
            {
                int clauseGlobalIdx = matchedClauses[i];
                int clauseMatrixIdx = matchedClausesIdx[i]; /* not used ! */

                int lmin = this->leastFrequentLiteral(this->clauses[clauseGlobalIdx], currentLit.lit);
                if (lmin == 0)
                    continue; /* unit clause, Unit clauses cannot be matched, store them individually ? */

                for (int otherGlobalIdx : litToClause[LIT_IDX(lmin)])
                {
                    /* if deleted or trivially unmatchable */
                    if (this->isClauseDeleted[otherGlobalIdx] || this->clauses[clauseGlobalIdx].size() != this->clauses[otherGlobalIdx].size())
                        continue;

                    /* If the difference C \ D is more than 1 literal, l1 and l2 cannot be factorized */
                    orderedClauseSub(this->clauses[clauseGlobalIdx], this->clauses[otherGlobalIdx], diff, 2);

                    /* To be factorized (matched): C \ {l1} \ D == D \ {l2} \ C
                     * C \ D must equal l1 and D \ C must equal l2
                     * (all the other literals are shared)
                     */
                    if (diff.size() == 1 && diff[0] == currentLit.lit)
                    {
                        orderedClauseSub(this->clauses[otherGlobalIdx], this->clauses[clauseGlobalIdx], diff, 2);

                        /*
                         * Since we checked if of the same size, the other diff is necessarely of size 1:
                         * C \ D = l1 => each l in C \ l1 is in D => they have C.size() - 1 literals in common
                         * C.size() == D.size() thus D has only one different literal l2
                         */

                        int lit = diff[0];

                        if (matchedLiterals.find(lit) == matchedLiterals.end()) /* different from original implementation */
                        {
                            /* Duplicated clauses must have been deleted at parsing or addition to not have more than once the same literal for a given i*/
                            LOGDEBUG2("matchedEntry(%d,%d,%d)", lit, otherGlobalIdx, i);
                            matchedEntries.emplace_back(lit, otherGlobalIdx, i); /* better to have a map and then get keys*/
                            matchedEntriesLits.push_back(lit);                   /* redundant information */
                        }
                    } // else diff = 0 (same clause) or diff >= 2 or diff == 1 && diff[0] != currentLit.lit
                }
            }
            /* Now we have a list of literals with matches with currentLit.lit, we need to take the one with the most matches */

            int lmax = 0;
            int lmaxMatches = 0;

            std::vector<int> ties;
            int count = 1;

            /* Better complexity than sorting the vector O(2N) vs O(N + NlogN)*/
            std::unordered_map<int, int> counts;

            for (int lit : matchedEntriesLits)
            {
                counts[lit]++;
            }
            LOGDEBUG2("MatchedLiterals: ");
            // Find the element with the maximum count
            for (const auto &pair : counts)
            {
                LOGDEBUG2("\t*(%d,%d)", pair.first, pair.second);
                if (pair.second > lmaxMatches)
                {
                    lmaxMatches = pair.second;
                    lmax = pair.first;
                    ties.clear();
                    ties.push_back(lmax);
                }
                else if (pair.second == lmaxMatches)
                {
                    ties.push_back(pair.first);
                }
            }

            if (lmax == 0)
            {
                LOGDEBUG2("Breaking since no matches");
                break; /* stop while(1) currentLit.lit cannot be matched */
            }

            int prevReduction = reduction(matchedLiterals.size(), matchedClauses.size());
            int newReduction = reduction(matchedLiterals.size() + 1, lmaxMatches);

            if (newReduction <= prevReduction)
            {
                LOGDEBUG2("Breaking since prevReduction is same or better");
                break; /* breaks while(1) : Not worth it */
            }

            /* DIVERSIFICATION shuffle the ties to select different lit if ties on heuristic value */
            if (this->shuffleTies)
            {
                std::random_shuffle(ties.begin(), ties.end());
            }

            /* DIVERSIFICATION : different tieBreakingHeuristics: take randomly a tie, take the least occuring / the most occuring,  */
            /* If several ties on lmaxMatches, select the most connected lmax to currentLit.lit */
            if (ties.size() > 1 && this->tieBreakHeuristic != SBVATieBreak::NONE)
            {
                this->breakTie(ties, currentLit.lit);
            }

            LOGDEBUG2("lmax: %d (ties:%lu), lmaxCount: %d, prevReduction: %d, newReduction: %d", lmax, ties.size(), lmaxMatches, prevReduction, newReduction);

            /* add best match according to tieBreakHeuristic or the first lmax*/
            matchedLiterals.insert(lmax);

            /* TODO optimize this using a map as matchedEntries, and do not use swap , Eigen can be used no ?*/
            /* What we want: update the matchedClauses to contain only the matches with lmax*/
            matchedClausesSwap.resize(lmaxMatches);
            matchedClausesIdxSwap.resize(lmaxMatches);

            int insertIdx = 0;
            for (auto &tuple : matchedEntries)
            {
                int lit = std::get<0>(tuple);
                if (lit != lmax)
                    continue;

                int clauseGlobalIdx = std::get<1>(tuple);
                int columnIdx = std::get<2>(tuple);

                matchedClausesSwap[insertIdx] = matchedClauses[columnIdx];
                matchedClausesIdxSwap[insertIdx] = matchedClausesIdx[columnIdx];
                insertIdx++;

                clausesToRemove.emplace_back(clauseGlobalIdx, matchedClausesIdx[columnIdx]); /* To optimize: requires a check later at deletion */
            }

            std::swap(matchedClauses, matchedClausesSwap);
            std::swap(matchedClausesIdx, matchedClausesIdxSwap);

            LOGDEBUG2("Matched clauses new size %d", matchedClauses.size());

        } // end of while(1)

        unsigned matchesClauseCount = matchedClauses.size();
        unsigned matchesCount = matchedLiterals.size();

        /* No match or not worth it: take another literal */
        if (matchesCount == 1 || (matchesCount <= 2 && matchesClauseCount <= 2))
            continue;

        /* Introduce new var and update clauses */
        int newVar = ++this->varCount;

        assert(newVar > 0);

        LOGDEBUG2("A new variable %d was added", newVar);

        /* Current clauses + (newVar, matche_i) clauses + (newVar, C \ matche_i) + preservingModelCountClause */
        this->clauses.reserve(this->clauses.size() + matchesCount + matchesClauseCount + this->preserveModelCount);
        this->litToClause.resize(this->varCount * 2);
        this->litCountAdjustement.resize(this->varCount * 2);

        /* Check if the adjency matrices have to be reconstructed */
        if (MATRIX_VAR_TO_IDX(newVar) >= this->adjacencyMatrixWidth)
        {
            LOGWARN("All the adjacency matrix is to be recomputed");
            this->adjacencyMatrixWidth = this->varCount * 2;
            this->adjacencyMatrix.clear();
        }
        this->adjacencyMatrix.resize(this->varCount);

        /* Adding (newVar, match_i) clauses */
        for (int lit : matchedLiterals)
        {
            // Add clause
            this->clauses.emplace_back(std::vector<int>{lit, newVar});
            this->isClauseDeleted.push_back(false);
            // Update occurence lists
            int newClauseGlobalIdx = this->clauses.size() - 1;
            this->litToClause[LIT_IDX(lit)].push_back(newClauseGlobalIdx);
            this->litToClause[PLIT_IDX(newVar)].push_back(newClauseGlobalIdx);

            if (this->generateProof)
            {
                // newVar must be first in proof clause
                this->proof.emplace_back(ProofClause{std::vector<int>{newVar, lit}, true});
            }
        }

        /* Adding (-newVar, ... ) clauses */
        for (unsigned globalClauseIdx : matchedClauses)
        {
            int newNLit = -1 * newVar;
            this->clauses.emplace_back(std::vector<int>{newNLit});
            this->isClauseDeleted.push_back(false);

            int newClauseGlobalIdx = this->clauses.size() - 1;
            this->litToClause[NLIT_IDX(newNLit)].push_back(newClauseGlobalIdx);

            for (int lit : this->clauses[globalClauseIdx])
            {
                if (lit != currentLit.lit)
                {
                    this->clauses.back().push_back(lit);
                    this->litToClause[LIT_IDX(lit)].push_back(newClauseGlobalIdx);
                }
            }

            if (this->generateProof)
            {
                this->proof.emplace_back(ProofClause{this->clauses.back(), true});
            }
        }

        /* From Original Implementation */
        // Preserving model count:
        //
        // The only case where we add a model is if both assignments for the auxiiliary variable satisfy the formula
        // for the same assignment of the original variables. This only happens if all(matched_lits) *AND*
        // all(matches_clauses) are satisfied.
        //
        // The easiest way to fix this is to add one clause that constrains all(matched_lits) => -f
        if (this->preserveModelCount)
        {
            int newNLit = -1 * newVar;
            this->clauses.emplace_back(std::vector<int>{newNLit});
            this->isClauseDeleted.push_back(false);

            int newClauseGlobalIdx = this->clauses.size() - 1;
            this->litToClause[NLIT_IDX(newNLit)].push_back(newClauseGlobalIdx);

            for (int lit : matchedLiterals)
            {
                this->clauses.back().push_back(-lit);
                this->litToClause[LIT_IDX(lit)].push_back(newClauseGlobalIdx);
            }

            if (this->generateProof)
            {
                this->proof.emplace_back(ProofClause{this->clauses.back(), true});
            }
            LOGDEBUG2("PreservedModel clauses generated");
        }

        /* Remove olds clauses */

        litsToUpdate.clear();

        /* TODO: a better clause deletion management */
        std::set<int> validClausesToDelete;

        for (int i = 0; i < matchesClauseCount; i++)
        {
            validClausesToDelete.insert(matchedClausesIdx[i]);
        }

        for (auto &pair : clausesToRemove)
        {
            int clauseGlobalIdx = pair.first;
            int clauseMatrixIdx = pair.second;

            if (validClausesToDelete.find(clauseMatrixIdx) == validClausesToDelete.end())
                continue;

            this->isClauseDeleted[clauseGlobalIdx] = true;
            this->adjacencyDeleted++;

            for (int lit : this->clauses[clauseGlobalIdx])
            {
                this->litCountAdjustement[LIT_IDX(lit)]--;
                litsToUpdate.insert(lit);
            }

            if (this->generateProof)
            {
                proof.emplace_back(ProofClause{this->clauses[clauseGlobalIdx], false});
            }
        }

        /* Requeue modified literals */

        for (int lit : litsToUpdate) /* currentLit.lit is always in litsToUpdate*/
        {
            litQueue.emplace(queuePair{lit, REAL_LIT_COUNT(lit)}); /* can be rematched since clauses were deleted */
            this->adjacencyMatrix[MATRIX_LIT_TO_IDX(lit)] = Eigen::SparseVector<int>(this->adjacencyMatrixWidth);
        }

        int realOccurences = this->litToClause[PLIT_IDX(newVar)].size() + this->litCountAdjustement[PLIT_IDX(newVar)];
        litQueue.emplace(queuePair{(int)this->varCount, (unsigned)realOccurences}); /* occurences >= 0*/

        realOccurences = this->litToClause[NLIT_IDX(-1 * newVar)].size() + this->litCountAdjustement[NLIT_IDX(-1 * newVar)];
        litQueue.emplace(queuePair{-1 * newVar, (unsigned)realOccurences});

        this->replacementsCount++;
    }
}
