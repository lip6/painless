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

#include "clauses/ClauseDatabaseLockFree.h"
#include "clauses/ClauseExchange.h"
#include "utils/Logger.h"

#include <string.h>
#include <stdio.h>
#include <numeric>



ClauseDatabaseLockFree::ClauseDatabaseLockFree()
{
   this->maxClauseSize = 50;
   for (int i = 0; i < maxClauseSize; i++)
   {
      this->clauses.push_back(new ClauseBufferAlpha());
   }
   this->totalSizes.clear(); // just to be sure it is empty
   this->totalSizes.resize(maxClauseSize, 0);
}

ClauseDatabaseLockFree::ClauseDatabaseLockFree(int maxClauseSize) : ClauseDatabase(maxClauseSize)
{
   if(maxClauseSize <= 0){
      LOG( "The value %d for maxClauseSize is not supported by ClauseDatabaseLockFree, it will be set to 50", maxClauseSize);
   }
   this->maxClauseSize = 50;

   for (int i = 0; i < this->maxClauseSize; i++)
   {
      this->clauses.push_back(new ClauseBufferAlpha());
   }
   this->totalSizes.clear(); // just to be sure it is empty
   this->totalSizes.resize(this->maxClauseSize, 0);
}

ClauseDatabaseLockFree::~ClauseDatabaseLockFree()
{
   for (auto clauseBuffer : clauses)
   {
      delete clauseBuffer;
   }
}

bool ClauseDatabaseLockFree::addClause(std::shared_ptr<ClauseExchange> clause)
{
   int clsSize = clause->size;
   if (clsSize <= 0)
   {
      LOG( "Panic, want to add a clause of size 0, clause won't be added and will be released");
      return false;
   }
   if (clsSize <= this->maxClauseSize && (clauses[clsSize - 1]->size() + 1) * clsSize < 10000)
   {
      clauses[clsSize - 1]->addClause(clause);
      totalSizes[clsSize - 1]++;
      return true;
   }
   else
   {
      return false;
   }
}

int ClauseDatabaseLockFree::giveSelection(std::vector<std::shared_ptr<ClauseExchange>> &selectedCls, unsigned totalSize)
{
   int used = 0;

   for (unsigned i = 0; i < maxClauseSize; i++)
   {
      unsigned clsSize = i + 1;
      unsigned left = totalSize - used;

      if (left < clsSize) // No more place
         return used;

      if (left >= clsSize * clauses[i]->size())
      {
         // If all the clauses of size clsSize (i+1) can be added
         used = used + clsSize * clauses[i]->size();

         clauses[i]->getClauses(selectedCls); // the clauses are consumed
      }
      else
      {
         // Else how many clauses can be added
         // Add them one by one
         unsigned nCls = left / clsSize;
         std::shared_ptr<ClauseExchange> tmp_clause;
         used = used + clsSize * nCls;

         while (clauses[i]->getClause(tmp_clause) && nCls > 0) // stops if no more clause is available or the needed quota is attained
         {
            selectedCls.push_back(tmp_clause);
            nCls--;
         }
      }
   }

   return used;
}

bool ClauseDatabaseLockFree::giveOneClause(std::shared_ptr<ClauseExchange> &cls)
{
   for (auto &clauseBuffer : clauses)
   {
      if (clauseBuffer->size() > 0)
      {
         clauseBuffer->getClause(cls);
         return true;
      }
   }
   return false;
}

void ClauseDatabaseLockFree::getClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls)
{
   for (auto &clauseBuffer : clauses)
   {
      clauseBuffer->getClauses(v_cls);
   }
}

void ClauseDatabaseLockFree::getSizes(std::vector<int> &nbClsPerSize)
{
   int bufferCount = this->clauses.size();
   for (int i = 0; i < bufferCount; i++)
   {
      // index i stores number of clauses of size i+1
      nbClsPerSize.push_back(clauses[i]->size());
   }
}

unsigned ClauseDatabaseLockFree::getSize()
{
   unsigned size = 0;
   int bufferCount = this->clauses.size();
   for (int i = 0; i < bufferCount; i++)
   {
      // index i stores number of clauses of size i+1
      size += (clauses[i]->size());
   }
   return size;
}

void ClauseDatabaseLockFree::deleteClauses(int size)
{
   if (size <= 0)
   {
      LOGERROR( "Deletion cancelled, received illegal clauseSize : %d", size);
      return;
   }
   int bufferCount = this->clauses.size();
   for (int i = size - 1; i < bufferCount; i++)
   {
      clauses[i]->clear();
   }
}