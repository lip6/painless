/**
 * @file Parsers.hpp
 * @brief Defines classes and functions for parsing CNF formulas and processing clauses.
 */

#pragma once

#include "../solvers/SolverInterface.hpp"
#include "containers/ClauseUtils.hpp"
#include "containers/Formula.hpp"
#include <algorithm>
#include <functional>

/**
 * @ingroup utils
 * @brief A set of helper functions for CNF file parsing
 */
namespace Parsers {

/**
 * @brief Abstract base class for clause processors.
 *
 * ClauseProcessor defines an interface for objects that process clauses
 * during parsing. Derived classes can implement specific processing logic.
 */
class ClauseProcessor
{
  public:
	ClauseProcessor() = default;

	/**
	 * @brief Initialize the processor with problem parameters.
	 * @param varCount The number of variables in the formula.
	 * @param clauseCount The number of clauses in the formula.
	 * @return true if initialization was successful, false otherwise.
	 */
	virtual bool initMembers(unsigned int varCount, unsigned int clauseCount) = 0;

	/**
	 * @brief Process a clause.
	 * @param clause The clause to process.
	 * @return true if the clause should be kept, false if it should be filtered out.
	 */
	virtual bool operator()(simpleClause& clause) = 0;

	virtual ~ClauseProcessor() = default;
};

/**
 * @brief Filter for removing redundant clauses.
 *
 * RedundancyFilter checks for and removes duplicate clauses during parsing.
 */
class RedundancyFilter : public ClauseProcessor
{
  public:
	/**
	 * @brief Initialize the redundancy filter.
	 * @param varCount The number of variables in the problem.
	 * @param clauseCount The number of clauses in the problem.
	 * @return Always returns true.
	 */
	bool initMembers(unsigned int varCount, unsigned int clauseCount) override;

	/**
	 * @brief Check if a clause is redundant.
	 *
	 * This method sorts the clause, removes duplicates, and checks if it's
	 * already in the clauseCache. If it's new, it's added to the cache.
	 *
	 * @param clause The clause to check.
	 * @return true if the clause is not redundant, false if it is.
	 */
	bool operator()(simpleClause& clause) override;

  private:
	mutable std::unordered_set<simpleClause, ClauseUtils::ClauseHash> clauseCache;
};

/**
 * @brief Filter for removing tautological clauses. A tautological clause or simple a tautology is a clause that
 * contains a literal and its complement, thus always satisfied
 *
 * TautologyFilter checks for and removes tautological clauses during parsing.
 */
class TautologyFilter : public ClauseProcessor
{
  public:
	/**
	 * @brief Initialize the tautology filter.
	 * @param varCount The number of variables in the problem.
	 * @param clauseCount The number of clauses in the problem.
	 * @return Always returns true.
	 */
	bool initMembers(unsigned int varCount, unsigned int clauseCount) override;

	/**
	 * @brief Check if a clause is a tautology.
	 *
	 * This method checks if the clause contains both a literal and its negation.
	 *
	 * @param clause The clause to check.
	 * @return true if the clause is not a tautology, false if it is.
	 */
	bool operator()(simpleClause& clause) override;
};

/**
 * @brief Parse a CNF formula from a file into a vector of clauses.
 *
 * @param filename The path to the file to parse.
 * @param clauses Vector to store the parsed clauses.
 * @param varCount Pointer to store the number of variables.
 * @param processors Vector of clause processors to apply during parsing.
 * @return true if parsing was successful, false otherwise.
 */
bool
parseCNF(const char* filename,
		 std::vector<simpleClause>& clauses,
		 unsigned int* varCount,
		 const std::vector<std::unique_ptr<ClauseProcessor>>& processors = {});

/**
 * @brief Parse a CNF formula from a file into a Formula object.
 *
 * @param filename The path to the file to parse.
 * @param formula Formula object to store the parsed formula.
 * @param processors Vector of clause processors to apply during parsing.
 * @return true if parsing was successful, false otherwise.
 */
bool
parseCNF(const char* filename, Formula& formula, const std::vector<std::unique_ptr<ClauseProcessor>>& processors = {});

/**
 * @brief Parse the CNF parameters (variable count and clause count) from a file.
 *
 * @param f File pointer to parse from.
 * @param varCount Reference to store the number of variables.
 * @param clauseCount Reference to store the number of clauses.
 * @return true if parsing was successful, false otherwise.
 */
bool
parseCNFParameters(FILE* f, unsigned int& varCount, unsigned int& clauseCount);

} // namespace Parsers