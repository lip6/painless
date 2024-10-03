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

#include "utils/SatUtils.h"

#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <cassert>

#include "BloomFilter.h"
#include <unordered_set>
#include "Logger.h"
#include "Parameters.h"
#include "System.h"
#include "painless.h"

bool parseParameters(const char *filename, unsigned *clauseCount, unsigned *varCount)
{
	FILE *f = fopen(filename, "r");

	if (f == NULL)
		return false;

	int c = 0;
	bool neg = false;

	simpleClause cls;

	while (c != EOF)
	{
		c = fgetc(f);

		// comment line
		if (c == 'c')
		{
			// skip this line
			while (c != '\n')
			{
				c = fgetc(f);
			}
			continue;
		}

		// Problem definition
		if (c == 'p')
		{
			SKIP_NONDIGIT(c, f);

			if (nullptr != varCount)
			{
				*varCount = 0;

				GET_NUMBER(c, f, *varCount);
			}

			SKIP_NONDIGIT(c, f);

			if (nullptr != clauseCount)
			{
				*clauseCount = 0;

				GET_NUMBER(c, f, *clauseCount);
			}

			return true;
		}
	}

	return false;
}

/* TODO: a better parser, maybe simplify kissat_parser or minisat, need a version for Duplicate Detection */
bool parseFormula(const char *filename, std::vector<simpleClause> &clauses, unsigned *varCount)
{
	if (globalEnding)
	{
		LOGWARN("Ending before parsing, returning false");
		return false;
	}
	unsigned parsedClauseCount;
	unsigned parsedVarCount;
	FILE *f = fopen(filename, "r");

	if (f == NULL)
		return false;

	int c = 0;
	bool neg = false;

	simpleClause cls;

	while (c != EOF)
	{
		c = fgetc(f);

		// comment line
		if (c == 'c')
		{
			// skip this line
			while (c != '\n')
			{
				c = fgetc(f);
			}
			continue;
		}

		// Problem definition
		if (c == 'p')
		{
			SKIP_NONDIGIT(c, f);

			parsedVarCount = 0;

			GET_NUMBER(c, f, parsedVarCount);

			SKIP_NONDIGIT(c, f);

			parsedClauseCount = 0;

			GET_NUMBER(c, f, parsedClauseCount);

			clauses.reserve(parsedClauseCount);

			continue;
		}

		// whitespace
		if (isspace(c))
			continue;

		// negative
		if (c == '-')
		{
			neg = true;
			continue;
		}

		// number
		if (isdigit(c))
		{
			int num = c - '0';

			c = fgetc(f);

			while (isdigit(c))
			{
				num = num * 10 + (c - '0');
				c = fgetc(f);
			}

			if (neg)
			{
				num *= -1;
			}

			neg = false;

			if (num != 0)
			{
				cls.push_back(num);
			}
			else
			{
				LOGCLAUSE(&cls[0], cls.size(), "Parsed the clause:");
				clauses.push_back(std::move(cls));
				cls.clear();
			}
		}
	}

	fclose(f);

	if (globalEnding)
	{
		LOGWARN("Ending after parsing, returning false");
		return false;
	}

	assert(parsedClauseCount == clauses.size());
	*varCount = parsedVarCount;
	LOG("Successefully parsed the %u clauses with %u variables in %s.", parsedClauseCount, parsedVarCount, filename);

	return true;
}

bool parseFormulaForSBVA(const char *filename, std::vector<simpleClause> &clauses, std::vector<std::vector<unsigned>> &listsOfOccurence, std::vector<bool> &isClauseDeleted, unsigned *varCount)
{
	if (globalEnding)
	{
		LOGWARN("Ending before parsing, returning false");
		return false;
	}
	FILE *f = fopen(filename, "r");

	unsigned parsedClauseCount;
	unsigned parsedVarCount;

	unsigned actualIdx = 0;

	unsigned duplicatesCount = 0;

	/* The default std::vector equality check assumes the vectors to be sorted */
	std::unordered_set<simpleClause, clauseHash> clausesCache;

	if (f == NULL)
		return false;

	int c = 0;
	bool neg = false;

	simpleClause cls;

	while (c != EOF)
	{
		c = fgetc(f);

		// comment line
		if (c == 'c')
		{
			// skip this line
			while (c != '\n')
			{
				c = fgetc(f);
			}
			continue;
		}

		// Problem definition
		if (c == 'p')
		{
			SKIP_NONDIGIT(c, f);

			parsedVarCount = 0;

			GET_NUMBER(c, f, parsedVarCount);

			/* SBVA */
			listsOfOccurence.resize(parsedVarCount * 2);

			SKIP_NONDIGIT(c, f);

			parsedClauseCount = 0;

			GET_NUMBER(c, f, parsedClauseCount);

			if (parsedClauseCount > Parameters::getIntParam("sbva-max-clause", MILLION * 10))
			{
				LOGWARN("The number of clauses is too important to use SBVA, returning");
				return false;
			}

			clauses.reserve(parsedClauseCount);

			/* SBVA */
			isClauseDeleted.reserve(parsedClauseCount);

			continue;
		}

		// whitespace
		if (isspace(c))
			continue;

		// negative
		if (c == '-')
		{
			neg = true;
			continue;
		}

		// number
		if (isdigit(c))
		{
			int num = c - '0';

			c = fgetc(f);

			while (isdigit(c))
			{
				num = num * 10 + (c - '0');
				c = fgetc(f);
			}

			if (neg)
			{
				num *= -1;
			}

			neg = false;

			if (num != 0)
			{
				cls.push_back(num);
			}
			else
			{
				/* SBVA insert */
				std::sort(cls.begin(), cls.end());

				auto insertTest = clausesCache.insert(cls); /* Makes a copy */

				/* Better worst case complexity by sorting first before potential equality check O(NlogN + N) vs O(N^2) */
				if (!insertTest.second)
				{
					LOGCLAUSE(&cls[0], cls.size(), "This is a duplicate: ");
					duplicatesCount++;
					cls.clear();
					continue;
				}

				LOGCLAUSE(&cls[0], cls.size(), "Parsed for SBVA the clause: ");

				isClauseDeleted.push_back(0);
				for (int lit : cls)
				{
					listsOfOccurence[lit > 0 ? lit * 2 - 2 : -lit * 2 - 1].push_back(actualIdx);
				}

				/* To not move or copy all the set's clauses afterwards */
				clauses.emplace_back(std::move(cls)); /* owns it */

				cls.clear();

				if (globalEnding)
				{
					LOGWARN("Ending during parsing, returning false");
					return false;
				}

				actualIdx++;
			}
		}
	}

	fclose(f);

	assert(parsedClauseCount == clauses.size() + duplicatesCount);
	*varCount = parsedVarCount;
	LOG("Successefully parsed the %u (%lu) clauses (duplicates: %u) with %u variables in %s.", parsedClauseCount, clauses.size(), duplicatesCount, parsedVarCount, filename);

	return true;
}
