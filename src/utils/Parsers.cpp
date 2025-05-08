#include "utils/Parsers.hpp"

#include <cassert>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include "ErrorCodes.hpp"
#include "Logger.hpp"
#include "NumericConstants.hpp"
#include "Parameters.hpp"
#include "painless.hpp"
#include <unordered_set>

namespace Parsers {
bool
RedundancyFilter::initMembers(unsigned int varCount, unsigned int clauseCount)
{
	return true;
}

bool
RedundancyFilter::operator()(simpleClause& clause)
{
	std::sort(clause.begin(), clause.end());
	auto newLast = std::unique(clause.begin(), clause.end());
	clause.erase(newLast, clause.end());
	return clauseCache.insert(clause).second;
}

bool
TautologyFilter::initMembers(unsigned int varCount, unsigned int clauseCount)
{
	return true;
}

bool
TautologyFilter::operator()(simpleClause& clause)
{
	std::unordered_set<int> literals;
	for (int lit : clause) {
		if (literals.count(-lit) != 0)
			return false;
		literals.insert(lit);
	}
	return true;
}

// Parser Helpers
// Utility functions
inline char
skipWhitespace(FILE* f)
{
	char c;
	while (isspace(c = fgetc(f)))
		;
	return c;
}

inline void
skipLine(FILE* f)
{
	char c;
	while ((c = fgetc(f)) != '\n' && c != EOF)
		;
}

inline int
parseNumber(FILE* f, int firstDigit)
{
	int num = firstDigit - '0';
	char c;
	while (isdigit(c = fgetc(f))) {
		num = num * 10 + (c - '0');
	}
	ungetc(c, f); // Put back the non-digit character
	return num;
}

// Parse the problem definition line
bool
parseCNFParameters(FILE* f, unsigned int& varCount, unsigned int& clauseCount)
{
	char c;
	while ((c = skipWhitespace(f)) != EOF) {
		if (c == 'c') {
			skipLine(f);
			continue;
		}
		if (c == 'p') {
			// Skip "\s*c"
			c = skipWhitespace(f);
			// Skip "nf"
			for (int i = 0; i < 2; i++) {
				if (fgetc(f) == EOF) {
					LOGERROR("EOF Detected to early");
					return false;
				}
			}

			c = skipWhitespace(f);
			if (!isdigit(c)) {
				LOGERROR("Unexpected character, %c", c);
				return false;
			}
			varCount = parseNumber(f, c);

			c = skipWhitespace(f);
			if (!isdigit(c)) {
				LOGERROR("Unexpected character, %c", c);
				return false;
			}
			clauseCount = parseNumber(f, c);

			return true;
		}
	}

	LOGERROR("p character not detected");
	return false;
}

// Parse a single clause
static bool
parseClause(FILE* f, simpleClause& cls)
{
	cls.clear();
	char c;
	bool neg = false;

	while ((c = skipWhitespace(f)) != EOF) {
		if (c == 'c') {
			skipLine(f);
			continue;
		}
		if (c == '-') {
			neg = true;
			continue;
		}
		if (isdigit(c)) {
			int num = parseNumber(f, c);
			if (neg) {
				num = -num;
				neg = false;
			}
			if (num == 0) {
				LOGCLAUSE2(&cls[0], cls.size(), "Parsed the clause:");
				return true; // End of clause
			}
			cls.push_back(num);
		} else {
			LOGERROR("Unexpected character, %c", c);
			return false;
		}
	}
	return false;
}

bool
parseCNF(const char* filename, Formula& parsedFormula, const std::vector<std::unique_ptr<ClauseProcessor>>& processors)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		LOGERROR("Couldn't open file: %s", filename);
		return false;
	}

	unsigned int parsedClauseCount = 0, parsedVarCount = 0, filteredOutCount = 0;
	if (!parseCNFParameters(f, parsedVarCount, parsedClauseCount)) {
		fclose(f);
		return false;
	}

	parsedFormula.setVarCount(parsedVarCount);

	for (auto& processor : processors) {
		if (!processor->initMembers(parsedVarCount, parsedClauseCount)) {
			LOGERROR("Error at member initialization of processor %s", typeid(*processor).name());
			exit(PERR_PARSING);
		}
	}

	simpleClause cls;
	while (parseClause(f, cls)) {
		if (!cls.empty()) {
			bool keepClause = true;
			for (auto& processor : processors) {
				if (!(keepClause = processor->operator()(cls))) {
					filteredOutCount++;
					break;
				}
			}
			if (keepClause && !parsedFormula.push_clause(std::move(cls))) {
				finalResult = SatResult::UNSAT;
				LOGDEBUG1("Parse stopping because of UNSAT");
				fclose(f);
				return true;
			}
		}
	}

	fclose(f);

	assert(parsedClauseCount - filteredOutCount == parsedFormula.getAllClauseCount());
	LOG0("Successfully parsed %u clauses (filtered out: %u) with %u variables in %s.",
		 parsedClauseCount,
		 filteredOutCount,
		 parsedVarCount,
		 filename);

	return true;
}

bool
parseCNF(const char* filename,
		 std::vector<simpleClause>& clauses,
		 unsigned int* varCount,
		 const std::vector<std::unique_ptr<ClauseProcessor>>& processors)
{

	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		LOGERROR("Couldn't open file: %s", filename);
		return false;
	}

	unsigned int parsedClauseCount = 0, parsedVarCount = 0, filteredOutCount = 0;
	if (!parseCNFParameters(f, parsedVarCount, parsedClauseCount)) {
		fclose(f);
		return false;
	}

	*varCount = parsedVarCount;

	for (auto& processor : processors) {
		if (!processor->initMembers(parsedVarCount, parsedClauseCount)) {
			LOGERROR("Error at member initialization of processor %s", typeid(*processor).name());
			exit(PERR_PARSING);
		}
	}

	simpleClause cls;
	while (parseClause(f, cls)) {
		if (!cls.empty()) {
			bool keepClause = true;
			for (auto& processor : processors) {
				if (!(keepClause = processor->operator()(cls))) {
					filteredOutCount++;
					break;
				}
			}
			if (keepClause)
				clauses.push_back(std::move(cls));
		}

		cls.clear();
	}

	fclose(f);

	assert(parsedClauseCount - filteredOutCount == clauses.size());
	LOG0("Successfully parsed %u clauses (filtered out: %u) with %u variables in %s.",
		 parsedClauseCount,
		 filteredOutCount,
		 parsedVarCount,
		 filename);

	return true;
}

bool
parseCNF(const char* filename,
		 std::vector<lit_t>& literals,
		 unsigned int* varCount,
		 unsigned int* clsCount,
		 const std::vector<std::unique_ptr<ClauseProcessor>>& processors)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		LOGERROR("Couldn't open file: %s", filename);
		return false;
	}

	unsigned int parsedClauseCount = 0, parsedVarCount = 0, filteredOutCount = 0, clsCount_ = 0;
	size_t litCounts = 0;
	if (!parseCNFParameters(f, parsedVarCount, parsedClauseCount)) {
		fclose(f);
		return false;
	}

	*varCount = parsedVarCount;

	for (auto& processor : processors) {
		if (!processor->initMembers(parsedVarCount, parsedClauseCount)) {
			LOGERROR("Error at member initialization of processor %s", typeid(*processor).name());
			exit(PERR_PARSING);
		}
	}

	simpleClause cls;
	while (parseClause(f, cls)) {
		if (!cls.empty()) {
			bool keepClause = true;
			for (auto& processor : processors) {
				if (!(keepClause = processor->operator()(cls))) {
					filteredOutCount++;
					break;
				}
			}
			if (keepClause) {
				literals.insert(literals.end(), cls.begin(), cls.end());
				literals.push_back(0);
				litCounts+=cls.size();
				clsCount_++;
			}
		}

		cls.clear();
	}

	fclose(f);

	assert(parsedClauseCount - filteredOutCount == clsCount_);

	*clsCount = clsCount_;

	LOG0("Successfully parsed %u clauses (filtered out: %u) with %u variables in %s.",
		 parsedClauseCount,
		 filteredOutCount,
		 parsedVarCount,
		 filename);

	return true;
}

} // namespace Parsers