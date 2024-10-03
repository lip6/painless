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

#include "clauses/ClauseDatabase.h"
#include "clauses/ClauseExchange.h"
#include "utils/Logger.h"

#include <numeric>


ClauseDatabase::ClauseDatabase()
{
   this->maxClauseSize = 0;
}

ClauseDatabase::ClauseDatabase(int maxClauseSize) : maxClauseSize(maxClauseSize)
{
}

ClauseDatabase::~ClauseDatabase()
{
}

void ClauseDatabase::getTotalSizes(std::stringstream &strStream)
{
   unsigned size = totalSizes.size();
   for (int i = 0; i < size; i++)
      strStream<< "[" << i + 1 << "]:" << totalSizes[i] << ", ";
   strStream << " TOTAL: " << std::accumulate(totalSizes.begin(), totalSizes.end(), 0) << std::endl;
}
