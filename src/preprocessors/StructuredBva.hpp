#pragma once

#include <map>
#include <random>

#include <Eigen/SparseCore>

#include "preprocessors/PreprocessorInterface.hpp"
#include "utils/Parsers.hpp"

//===============litQueue.h====================

/// @brief queuePair struct used for the priorityQueue in solve
typedef struct
{
	int lit;
	unsigned int occurencesCount;
} queuePair;

/* Notes from cppreference:
Note that the Compare parameter is defined such that it returns true if its first argument
comes before its second argument in a weak ordering. But because the priority queue outputs
largest elements first, the elements that "come before" are actually output last.
That is, the front of the queue contains the "last" element according to the weak ordering
imposed by Compare.
*/
bool
decreasingOrder(const queuePair& lhs, const queuePair& rhs);

bool
increasingOrder(const queuePair& lhs, const queuePair& rhs);

bool
randomOrder(const queuePair& lhs, const queuePair& rhs);

/// Functor for the priority queue used in solve()
struct PairCompare
{
	typedef bool (*compareFunc)(const queuePair&, const queuePair&);

	PairCompare(compareFunc func)
		: func(func)
	{
	}

	PairCompare()
		: func(decreasingOrder)
	{
	}

	bool operator()(const queuePair& lhs, const queuePair& rhs) const { return func(lhs, rhs); }

	compareFunc func;
};

//=============================================

// Occurence lists
#define REAL_LIT_COUNT(LIT)                                                                                            \
	((unsigned int)this->litToClause[LIT_IDX(LIT)].size() + this->litCountAdjustement[LIT_IDX(LIT)])

// Adjacency Matrix
#define MATRIX_VAR_TO_IDX(LIT) (LIT - 1)
#define MATRIX_LIT_TO_IDX(LIT) (LIT > 0 ? LIT - 1 : -LIT - 1)
#define MATRIX_IDX_TO_VAR(IDX) (IDX + 1)

/// Tie-Breaking Heuristics
enum class SBVATieBreak
{
	NONE = 1,
	THREEHOPS = 2,
	MOSTOCCUR = 3,
	LEASTOCCUR = 4,
	RANDOM = 5,
};

/* Do the same as isClauseDeleted ?*/
struct ProofClause
{
	std::vector<int> lits;
	bool isAddition;
};

/**
 * @brief Structured Binary Variable Addition (SBVA) preprocessing algorithm.
 *
 * This class implements the Structured BVA preprocessing technique for SAT solving.
 * It is based on the original implementation from https://github.com/hgarrereyn/SBVA,
 * reorganized and adapted for this solver framework.
 *
 * @ingroup preproc_solving
 */
class StructuredBVA : public PreprocessorInterface
{
  public:
	/// Constructor.
	StructuredBVA(int _id, unsigned long maxReplacements, bool shuffleTies);

	/// Destructor.
	~StructuredBVA();

	unsigned int getVariablesCount() override;

	void setSolverInterrupt() override;

	void unsetSolverInterrupt() override;

	SatResult solve(const std::vector<int>& cube = {}) override;

	void addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVariables) override;

	void addInitialClauses(const lit_t *literals, unsigned int nbClauses, unsigned int nbVariables) override;

	void loadFormula(const char* filename) override;

	void printStatistics() override;

	std::vector<simpleClause> getSimplifiedFormula() override;

	PreprocessorStats getPreprocessorStatistics()
	{
		return { (uint)this->clauses.size() - this->adjacencyDeleted, this->adjacencyDeleted, 0, this->replacementsCount, 0 };
	}

	int getDivisionVariable() { return 0; }

	void addClause(ClauseExchangePtr clause) { return; }

	void addClauses(const std::vector<ClauseExchangePtr>& clauses) { return; }

	/**
	 * @brief Diversify the preprocessor's behavior.
	 * @param getSeed Function to get a random seed (default uses solver ID).
	 */
	void diversify(const SeedGenerator& getSeed = [](SolverInterface* s) { return s->getSolverId(); });

	void updateAdjacencyMatrix(int var);

	unsigned int getThreeHopHeuristic(int lit1, int lit2);

	/* returns the least occuring literal in a clause c\var */
	int leastFrequentLiteral(simpleClause& clause, int lit);

	void setTieBreakHeuristic(SBVATieBreak tieBreak);

	void releaseMemory()
	{
		this->tempTieHeuristicCache.clear();
		this->litCountAdjustement.clear();
		this->adjacencyMatrix.clear();
		this->isClauseDeleted.clear();
		this->litToClause.clear();
		this->clauses.clear();
		this->proof.clear();

		this->litCountAdjustement.shrink_to_fit();
		this->adjacencyMatrix.shrink_to_fit();
		this->isClauseDeleted.shrink_to_fit();
		this->litToClause.shrink_to_fit();
		this->clauses.shrink_to_fit();
		this->proof.shrink_to_fit();
	}

	/**
	 * @brief Restore the model by removing added clauses.
	 * @param model The model to be restored.
	 */
	void restoreModel(std::vector<int>& model) override { model.resize(this->varCount - this->replacementsCount); }

	/* TODO factorize using a macro */
	inline int threeHopTieBreak(const std::vector<int>& ties, const int currentLit)
	{
		int lmax = 0;
		/* How much currentLit.lit is connected to ties[i] */
		unsigned int maxHeuristicVal = 0;
		for (int tie : ties) {
			unsigned int temp = this->getThreeHopHeuristic(currentLit, tie);
			if (temp > maxHeuristicVal) {
				maxHeuristicVal = temp;
				lmax = tie;
			}
		}
		return lmax;
	}

	inline int mostOccurTieBreak(const std::vector<int>& ties, const int currentLit)
	{
		int lmax = 0;
		/* How much currentLit.lit is connected to ties[i] */
		unsigned int maxHeuristicVal = 0;
		for (int tie : ties) {
			unsigned int temp = REAL_LIT_COUNT(tie);
			if (temp > maxHeuristicVal) {
				maxHeuristicVal = temp;
				lmax = tie;
			}
		}
		return lmax;
	}

	inline int leastOccurTieBreak(const std::vector<int>& ties, const int currentLit)
	{
		int lmax = 0;
		/* How much currentLit.lit is connected to ties[i] */
		unsigned int maxHeuristicVal = UINT32_MAX;
		for (int tie : ties) {
			unsigned int temp = REAL_LIT_COUNT(tie);
			if (temp < maxHeuristicVal) {
				maxHeuristicVal = temp;
				lmax = tie;
			}
		}
		return lmax;
	}

	inline int randomTieBreak(const std::vector<int>& ties, const int currentLit)
	{
		std::srand(currentLit);
		return ties[std::rand() % ties.size()];
	}

	std::vector<int> getModel() override
	{
		LOGWARN("BVA cannot solve a formula");
		return {};
	}

  private:
	void printParameters();

  private:
	std::atomic<bool> stopPreprocessing;

	/// @brief Vector of clauses
	std::vector<simpleClause> clauses;

	/// @brief Instead of using another struct for clauses
	std::vector<bool> isClauseDeleted;

	/// @brief Occurence list using the literal as an index for a list of clause indexes in this->clauses
	std::vector<std::vector<unsigned int>> litToClause;

	/// @brief To keep track of the real number of occurences during the algorithm
	std::vector<int> litCountAdjustement;

	/// @brief For the eigenVector size
	unsigned int adjacencyMatrixWidth;

	/// @brief Adjacency matrix used to compute 3HOP tie breaking heuristic
	std::vector<Eigen::SparseVector<int>> adjacencyMatrix;

	/// @brief Cache used for not recomputing the heuristic each time
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

	unsigned int maxReplacements;

	std::function<int(const std::vector<int>&, const int)> breakTie;

	// Stats
	//------
	unsigned int varCount;

	unsigned int adjacencyDeleted;

	unsigned int replacementsCount;

	unsigned int originalClauseCount;

	// Helpers
	//--------
};

namespace Parsers {

/**
 * @brief A filter for Structured BVA (Binary Variable Addition)
 *
 * This class implements a clause filter for Structured BVA operations.
 * It inherits from Parsers::ClauseProcessor.
 */
class SBVAInit : public ClauseProcessor
{
  public:
	SBVAInit(std::vector<std::vector<unsigned int>>& litToClause, std::vector<bool>& isClauseDeleted)
		: m_litToClause(litToClause)
		, m_isClauseDeleted(isClauseDeleted)
	{
	}

	bool initMembers(unsigned int varCount, unsigned int clauseCount);
	bool operator()(simpleClause& clause) override;

  private:
	std::vector<std::vector<unsigned int>>& m_litToClause;
	std::vector<bool>& m_isClauseDeleted;
};

}