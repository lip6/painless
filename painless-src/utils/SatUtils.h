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

#include "../solvers/SolverInterface.hpp"
#include "ClauseUtils.h"
#include <algorithm>

#define SKIP_NONDIGIT(c, f) \
    do                      \
    {                       \
        c = fgetc(f);       \
    } while (!isdigit(c))

#define GET_NUMBER(c, f, n)     \
    do                          \
    {                           \
        n = n * 10 + (c - '0'); \
        c = fgetc(f);           \
    } while (isdigit(c))

/// Load the cnf contains in the file to the solver.
bool parseFormula(const char *filename, std::vector<simpleClause> &clauses, unsigned *varCount);

bool parseParameters(const char *filename, unsigned *clauseCount, unsigned *varCount);

// To be generalized by taking a refenrece to a Preprocess instance
bool parseFormulaForSBVA(const char *filename, std::vector<simpleClause> &clauses, std::vector<std::vector<unsigned>> &listsOfOccurence, std::vector<bool> &isClauseDeleted, unsigned *varCount);
