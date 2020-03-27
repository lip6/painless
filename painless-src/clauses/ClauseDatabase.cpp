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

#include "../clauses/ClauseDatabase.h"
#include "../clauses/ClauseManager.h"
#include "../clauses/ClauseExchange.h"
#include "../utils/Logger.h"

#include <string.h>
#include <stdio.h>

using namespace std;

ClauseDatabase::ClauseDatabase()
{
}

ClauseDatabase::~ClauseDatabase()
{
}

void
ClauseDatabase::addClause(ClauseExchange * clause)
{
	int clsSize = clause->size;

	while (clauses.size() < clsSize) {
      vector<ClauseExchange *> newVector;
		clauses.push_back(newVector);
	}

   if ((clauses[clsSize-1].size() + 1) * clsSize < 1000) {
      clauses[clsSize-1].push_back(clause);
   } else {
      ClauseManager::releaseClause(clause);
   }
}

int
ClauseDatabase::giveSelection(vector<ClauseExchange *> & selectedCls,
                              unsigned totalSize, int * selectCount)
{
   int used     = 0;
   *selectCount = 0;

   for (unsigned i = 0; i < clauses.size(); i++) {
      unsigned clsSize = i + 1;
      unsigned left    = totalSize - used;

      if (left < clsSize) // No more place
         return used;
      
      if (left >= clsSize * clauses[i].size()) {
         // If all the clauses of size clsSize (i+1) can be added
         used = used + clsSize * clauses[i].size();

         selectedCls.insert(selectedCls.end(), clauses[i].begin(),
                            clauses[i].end());

         *selectCount += clauses[i].size();

         clauses[i].clear();
      } else {
         // Else how many clauses can be added
         // Add them one by one
         unsigned nCls = left / clsSize;
         used              = used + clsSize * nCls;

         for (unsigned k = 0; k < nCls; k++) {
            selectedCls.push_back(clauses[i].back());
            clauses[i].pop_back();

            *selectCount += 1;
         }
      }
   }

   return used;
}
