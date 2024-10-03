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

#include "clauses/ClauseDatabaseVector.h"
#include "clauses/ClauseExchange.h"
#include "utils/Logger.h"

#include <numeric>
#include <string.h>



ClauseDatabaseVector::ClauseDatabaseVector()
{
}

ClauseDatabaseVector::ClauseDatabaseVector(int maxClauseSize) : ClauseDatabase(maxClauseSize)
{
}

ClauseDatabaseVector::~ClauseDatabaseVector()
{
}

bool ClauseDatabaseVector::addClause(std::shared_ptr<ClauseExchange> clause)
{
   int clsSize = clause->size;
   
   if (maxClauseSize > 0 && clsSize > this->maxClauseSize)
   {
      LOG3( "Clause of size %d, will not be added to ClauseDatabaseVector of maxClauseSize = %d", clsSize, this->maxClauseSize);
      return false;
   }

   while (clauses.size() < clsSize)
   {
      std::vector<std::shared_ptr<ClauseExchange>> newVector;
      clauses.push_back(newVector);
      totalSizes.push_back(0);
   }

   if ((clauses[clsSize - 1].size() + 1) * clsSize < 1000)
   {
      clauses[clsSize - 1].push_back(clause);
      totalSizes[clsSize - 1]++;
      return true;
   }
   else
   {
      return false;
   }
}

int ClauseDatabaseVector::giveSelection(std::vector<std::shared_ptr<ClauseExchange> > &selectedCls,
                                  unsigned totalSize, int *selectCount)
{
   int used = 0;
   *selectCount = 0;

   int clausesSize = clauses.size();
   for (unsigned i = 0; i < clausesSize; i++)
   {
      unsigned clsSize = i + 1;
      unsigned left = totalSize - used;

      if (left < clsSize) // No more place
         return used;

      if (left >= clsSize * clauses[i].size())
      {
         // If all the clauses of size clsSize (i+1) can be added
         used = used + clsSize * clauses[i].size();

         selectedCls.insert(selectedCls.end(), clauses[i].begin(),
                            clauses[i].end());

         *selectCount += clauses[i].size();

         clauses[i].clear();
      }
      else
      {
         // Else how many clauses can be added
         // Add them one by one
         unsigned nCls = left / clsSize;
         used = used + clsSize * nCls;

         for (unsigned k = 0; k < nCls; k++)
         {
            selectedCls.push_back(clauses[i].back());
            clauses[i].pop_back();

            *selectCount += 1;
         }
      }
   }

   return used;
}

int ClauseDatabaseVector::giveSelection(std::vector<std::shared_ptr<ClauseExchange> > &selectedCls,
                                  unsigned totalSize)
{
   int used = 0;

   int clausesSize = this->clauses.size();
   for (int i = 0; i < clausesSize; i++)
   {
      unsigned clsSize = i + 1;
      unsigned left = totalSize - used;

      if (left < clsSize) // No more place
         return used;

      if (left >= clsSize * clauses[i].size())
      {
         // If all the clauses of size clsSize (i+1) can be added
         used = used + clsSize * clauses[i].size();

         selectedCls.insert(selectedCls.end(), clauses[i].begin(),
                            clauses[i].end());

         clauses[i].clear();
      }
      else
      {
         // Else how many clauses can be added
         // Add them one by one
         unsigned nCls = left / clsSize;
         used = used + clsSize * nCls;

         for (unsigned k = 0; k < nCls; k++)
         {
            selectedCls.push_back(clauses[i].back());
            clauses[i].pop_back();
         }
      }
   }

   return used;
}

bool ClauseDatabaseVector::giveOneClause(std::shared_ptr<ClauseExchange> &cls)
{
   for (auto &clauseVector : clauses)
   {
      if (clauseVector.size() > 0)
      {
         cls = clauseVector.back();
         clauseVector.pop_back();
         return true;
      }
   }
   return false;
}

void ClauseDatabaseVector::getClauses(std::vector<std::shared_ptr<ClauseExchange> > &v_cls)
{
   for (auto &clauseVector : clauses)
   {
      v_cls.insert(v_cls.end(), clauseVector.begin(), clauseVector.end());
      clauseVector.clear();
   }
}

void ClauseDatabaseVector::getSizes(std::vector<int> &nbClsPerSize)
{
   for (auto &clauseVector : clauses)
   {
      // index i stores number of clauses of size i+1
      nbClsPerSize.push_back(clauseVector.size());
   }
}

unsigned ClauseDatabaseVector::getSize()
{
   unsigned size = 0;
   for (auto &clauseVector : clauses)
   {
      // index i stores number of clauses of size i+1
      size += (clauseVector.size());
   }
   return size;
}

void ClauseDatabaseVector::deleteClauses(int clsSize)
{
   if (clsSize <= 0)
   {
      LOGERROR( "Deletion cancelled, received illegal clauseSize : %d", clsSize);
      return;
   }
   unsigned dbSize = clauses.size();
   for (int i = clsSize - 1; i < dbSize; i++)
   {
      clauses[i].clear();
   }
}
