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

#include "../sharing/HordeSatSharing.h"
#include "../utils/Logger.h"
#include "../utils/Parameters.h"
#include "../clauses/ClauseManager.h"
#include "../solvers/SolverFactory.h"

HordeSatSharing::HordeSatSharing()
{
   literalPerRound = Parameters::getIntParam("shr-lit", 1500);
}

HordeSatSharing::~HordeSatSharing()
{
}

void
HordeSatSharing::doSharing(int idSharer, const vector<SolverInterface *> & from,
                           const vector<SolverInterface *> & to)
{
   for (size_t i = 0; i < from.size(); i++) {
      int used, usedPercent, selectCount;
      
      tmp.clear();

      from[i]->getLearnedClauses(tmp);

      stats.receivedClauses += tmp.size();

      for (size_t k = 0; k < tmp.size(); k++) {
         database.addClause(tmp[k]);
      }

      tmp.clear();

      used        = database.giveSelection(tmp, literalPerRound, &selectCount);
      usedPercent = (100 * used) / literalPerRound;

      stats.sharedClauses += tmp.size();

      if (usedPercent < 80) {
         from[i]->increaseClauseProduction();
         log(1, "Sharer %d production increase for solver %d.\n", idSharer,
             from[i]->id);
      }

      if (selectCount > 0) {
         log(1, "Sharer %d filled %d%% of its buffer %.2f\n", idSharer,
             usedPercent, used/(float)selectCount);
      }


      for (size_t j = 0; j < to.size(); j++) {
         if (from[i]->id != to[j]->id) {
            for (size_t k = 0; k < tmp.size(); k++) {
               ClauseManager::increaseClause(tmp[k], 1);
            }
            to[j]->addLearnedClauses(tmp);
         }
      }

      for (size_t k = 0; k < tmp.size(); k++) {
         ClauseManager::releaseClause(tmp[k]);
      }
   }
}

SharingStatistics
HordeSatSharing::getStatistics()
{
   return stats;
}
