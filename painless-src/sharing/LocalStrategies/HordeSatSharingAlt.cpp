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
#include "HordeSatSharingAlt.h"

#include "solvers/SolverFactory.h"
#include "utils/System.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "painless.h"

HordeSatSharingAlt::HordeSatSharingAlt(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers) : LocalSharingStrategy(producers, consumers)
{
   // shr-lit * solver_nbr
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500) * this->producers.size();
   this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);
   this->roundBeforeIncrease = Parameters::getIntParam("shr-horde-init-round", 1);
   this->round = 0;
   this->initPhase = true;
   unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

   for (auto &producer : this->producers)
   {
      producer->setLbdLimit(lbdLimit);
   }

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }

   LOGSTAT("[HordeAlt] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d, literalsPerRound: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease, literalPerRound);
}

HordeSatSharingAlt::HordeSatSharingAlt(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers) : LocalSharingStrategy(producers, consumers)
{
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500) * this->producers.size();
   this->round = 0;
   this->initPhase = true;
   unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

   for (auto &producer : this->producers)
   {
      producer->setLbdLimit(lbdLimit);
   }

   this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);

   this->roundBeforeIncrease = Parameters::getIntParam("shr-horde-init-round", 1);

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }

   LOGSTAT("[HordeAlt] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d, literalsPerRound: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease, literalPerRound);
}

HordeSatSharingAlt::~HordeSatSharingAlt()
{
   // delete database;
   if (Parameters::getBoolParam("dup"))
   {
      delete filter;
   }
}

bool HordeSatSharingAlt::doSharing()
{
   round++;

   if (globalEnding)
      return true;

   if (this->mustAddEntities)
   {
      LOGDEBUG1("[HordeAlt] Adding new entities");
      this->addLock.lock();
      this->addSharingEntities(this->addProducers, this->producers);
      this->addSharingEntities(this->addConsumers, this->consumers);
      this->addLock.unlock();
      this->mustAddEntities = false;

      /* Update literalsPerRound */
      this->literalPerRound = Parameters::getIntParam("shr-lit", 1500) * this->producers.size();
      LOGSTAT("[HordeAlt] [Update] Producers: %d, Consumers: %d, literalsPerRound: %d", producers.size(), consumers.size(), literalPerRound);

      /* TODO : optimize by introducing addUnits in sharingEntity */
      for (int lit : this->units)
      {
         std::shared_ptr<ClauseExchange> tmpCls = std::make_shared<ClauseExchange>(std::vector<int>{lit}, 0, -1);
         LOGDEBUG2("Import unit clause [size: %d, lit: %d]", tmpCls->size, lit);
         for (auto consumer : this->addConsumers)
         {
            consumer->importClause(tmpCls);
         }
      }

      /* Temp solution: this can make so that some new solvers don't receive the units */
      this->units.clear();
   }

   if (this->mustRemoveEntities)
   {
      LOGDEBUG1("[HordeAlt] Removing old entities");
      this->removeLock.lock();
      this->removeSharingEntities(this->removeProducers, this->producers);
      this->removeSharingEntities(this->removeConsumers, this->consumers);
      this->removeLock.unlock();
      this->mustRemoveEntities = false;

      /* Update literalsPerRound */
      this->literalPerRound = Parameters::getIntParam("shr-lit", 1500) * this->producers.size();
      LOGSTAT("[HordeAlt] [Update] Producers: %d, Consumers: %d, literalsPerRound: %d", producers.size(), consumers.size(), literalPerRound);
   }

   // 1- Fill the database using all the producers
   if (Parameters::getBoolParam("dup"))
   {
      for (auto producer : producers)
      {
         unfiltered.clear();
         filtered.clear();

         producer->exportClauses(unfiltered);
         // ref = 1
         for (std::shared_ptr<ClauseExchange> c : unfiltered)
         {
            if (!filter->contains_or_insert(c->lits))
            {
               filtered.push_back(c);
            }
         }
         stats.receivedClauses += unfiltered.size();
         stats.receivedDuplicas += (unfiltered.size() - filtered.size());

         // if solver checks the usage percentage else nothing fancy to do
         producer->accept(this);
      }
   }
   else
   {
      for (auto producer : producers)
      {
         producer->exportClauses(filtered);
         stats.receivedClauses += filtered.size();
         producer->accept(this);
      }
   }

   std::vector<std::shared_ptr<ClauseExchange>> selectedClauses;

   selectedClauses.insert(selectedClauses.end(), std::make_move_iterator(filtered.begin()), std::make_move_iterator(filtered.end()));

   unfiltered.clear();
   filtered.clear();

   std::sort(selectedClauses.begin(), selectedClauses.end(), [](std::shared_ptr<ClauseExchange> &a, std::shared_ptr<ClauseExchange> &b)
             { return a->size < b->size || (a->size == b->size && a->lbd < b->lbd); });

   int sharedLiterals = 0;
   unsigned size = selectedClauses.size();
   int i = 0;

   // 3a- share all units
   while (i < size && 1 == selectedClauses[i]->size)
   {
      this->units.insert(selectedClauses[i]->lits[0]); /* for added consumers */
      LOGDEBUG2("Unit : %d", selectedClauses[i]->lits[0]);

      for (auto consumer : consumers)
      {
         consumer->importClause(selectedClauses[i]);
      }
      // do not count on sharedLiterals
      stats.sharedClauses++;
      i++;
   }

   // 3b- share the other clauses with a limit of literalsPerRound
   while (i < size)
   {
      sharedLiterals += selectedClauses[i]->size;
      if (sharedLiterals > this->literalPerRound)
         break;
      for (auto consumer : consumers)
      {
         consumer->importClause(selectedClauses[i]);
      }
      stats.sharedClauses++;
      i++;
   }

   LOG2("[HordeAlt] received cls %ld, shared cls %ld", stats.receivedClauses, stats.sharedClauses);

   if (globalEnding)
      return true;
   return false;
}

void HordeSatSharingAlt::visit(SolverCdclInterface *solver)
{
   LOG4("[HordeAlt] Visiting the solver %d", solver->id);

   // Here I check the number of produced clauses before selecting them (maybe will not cause solvers to export potentially useless clauses)
   int usedPercent;
   usedPercent = (100 * SharingStrategy::getLiteralsCount(filtered)) / literalPerRound;
   if (usedPercent < 75 && !this->initPhase)
   {
      solver->increaseClauseProduction();
      LOG3("[HordeAlt] production increase for solver %d.", solver->id);
   }
   else if (usedPercent > 98)
   {
      solver->decreaseClauseProduction();
      LOG3("[HordeAlt] production decrease for solver %d.", solver->id);
   }
   if (round >= this->roundBeforeIncrease)
   {
      this->initPhase = false;
   }
}

void HordeSatSharingAlt::visit(SharingEntity *sh_entity)
{
   LOG4("[HordeAlt] Visiting the sh_entity %d", sh_entity->id);
}

#ifndef NDIST
void HordeSatSharingAlt::visit(GlobalDatabase *g_base)
{
   LOG4("[HordeAlt] Visiting the global database %d", g_base->id);
}
#endif