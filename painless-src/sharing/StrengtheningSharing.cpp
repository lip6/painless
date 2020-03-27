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

#include "../clauses/ClauseManager.h"
#include "../sharing/StrengtheningSharing.h"
#include "../solvers/SolverFactory.h"
#include "../utils/Logger.h"

#include <algorithm>

StrengtheningSharing::StrengtheningSharing()
{
   solver = SolverFactory::createMapleCOMSPSSolver();
}

StrengtheningSharing::~StrengtheningSharing()
{
   delete solver;
}

bool
StrengtheningSharing::strengthed(ClauseExchange * cls,
                               ClauseExchange ** outCls)
{
   assumps.clear();
   tmpNewClause.clear();
   
   for (int idLit = 0; idLit < cls->size; idLit++) {

      assumps.push_back(-cls->lits[idLit]);
      SatResult res = solver->solve(assumps);

      if (res == UNSAT) {
         vector<int> finalAnalysis = solver->getFinalAnalysis();

         tmpNewClause.clear();
         tmpNewClause.insert(tmpNewClause.end(), finalAnalysis.begin(),
                             finalAnalysis.end());
         break;
      }

      tmpNewClause.push_back(idLit);
   }

   if (cls->size == tmpNewClause.size())
      return false;

   *outCls = ClauseManager::allocClause(tmpNewClause.size());

   for (int idLit = 0; idLit < tmpNewClause.size(); idLit++) {
      (*outCls)->lits[idLit] = tmpNewClause[idLit];
   }

   (*outCls)->from = cls->from;
   (*outCls)->lbd  = cls->lbd;
   if ((*outCls)->size < (*outCls)->lbd) {
      (*outCls)->lbd = (*outCls)->size;
   }

   return true;
}
void
StrengtheningSharing::doSharing(int idSharer,
                              const vector<SolverInterface *> & from,
                              const vector<SolverInterface *> & to)
{
   for (int i = 0; i < from.size(); i++) {
      tmp.clear();

      from[i]->getLearnedClauses(tmp);

      stats.receivedClauses += tmp.size();
      stats.sharedClauses   += tmp.size();

      for (int idCls = 0; idCls < tmp.size(); idCls++) {
         ClauseExchange * strenghedClause;

         if (tmp[idCls]->size >= 8 && strengthed(tmp[idCls], &strenghedClause))
         {
            solver->addClause(strenghedClause);

            for (int j = 0; j < to.size(); j++) {
               ClauseManager::increaseClause(strenghedClause, to.size());
               to[j]->addLearnedClause(strenghedClause);
            }
         } else {
            for (int j = 0; j < to.size(); j++) {
               if (i != j) {
                  ClauseManager::increaseClause(tmp[idCls], to.size() - 1);
                  to[j]->addLearnedClause(tmp[idCls]);
               }
            }
         }
      }
            
      for (int idCls = 0; idCls < tmp.size(); idCls++) {
         ClauseManager::releaseClause(tmp[idCls]);
      }
   }
}

SharingStatistics
StrengtheningSharing::getStatistics()
{
   return stats;
}
