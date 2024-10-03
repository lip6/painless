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
#include "HordeSatSharing.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/BloomFilter.h"
#include "painless.h"

HordeSatSharing::HordeSatSharing(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers) : LocalSharingStrategy(producers, consumers)
{
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);

   unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

   for (auto &producer : this->producers)
   {
      producer->setLbdLimit(lbdLimit);
   }
   // number of round corresponding to 5% of the 5000s timeout
   this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);
   this->roundBeforeIncrease = Parameters::getIntParam("shr-horde-init-round", 1);

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
   LOGSTAT("[HordeSat] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease);
}

HordeSatSharing::HordeSatSharing(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers) : LocalSharingStrategy(producers, consumers)
{
   this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);

   unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

   for (auto &producer : this->producers)
   {
      producer->setLbdLimit(lbdLimit);
   }
   // number of round corresponding to 5% of the 5000s timeout
   this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);
   this->roundBeforeIncrease = Parameters::getIntParam("shr-horde-init-round", 1);

   if (Parameters::getBoolParam("dup"))
   {
      this->filter = new BloomFilter();
   }
   LOGSTAT("[HordeSat] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease);
}

HordeSatSharing::~HordeSatSharing()
{
   for (auto pair : this->databases)
   {
      delete pair.second;
   }
   if (Parameters::getBoolParam("dup"))
   {
      delete filter;
   }
}

bool HordeSatSharing::doSharing()
{
   if (globalEnding)
      return true;

   if (this->mustAddEntities)
   {
      LOGDEBUG1("[Hordesat] Adding new entities");
      /* TODO send units saved previously */
      this->addLock.lock();
      this->addSharingEntities(this->addProducers, this->producers);
      this->addSharingEntities(this->addConsumers, this->consumers);
      this->addLock.unlock();
      this->mustAddEntities = false;
      LOGSTAT("[HordeAlt] [Update] Producers: %d, Consumers: %d, literalsPerRound: %d", producers.size(), consumers.size(), literalPerRound * producers.size());
   }

   if (this->mustRemoveEntities)
   {
      LOGDEBUG1("[Hordesat] Removing old entities");
      this->removeLock.lock();
      this->removeSharingEntities(this->removeProducers, this->producers);
      this->removeSharingEntities(this->removeConsumers, this->consumers);
      this->removeLock.unlock();
      this->mustRemoveEntities = false;
      LOGSTAT("[HordeAlt] [Update] Producers: %d, Consumers: %d, literalsPerRound: %d", producers.size(), consumers.size(), literalPerRound * producers.size());
   }

   LOGDEBUG1("[Hordesat] start sharing");

   for (auto producer : producers)
   {
      if (!this->databases.count(producer->id))
      {
         this->databases[producer->id] = new ClauseDatabaseVector();
      }
      unfiltered.clear();
      filtered.clear();

      if (Parameters::getBoolParam("dup"))
      {
         producer->exportClauses(unfiltered);
         // ref = 1
         for (std::shared_ptr<ClauseExchange> c : unfiltered)
         {
            uint8_t count = filter->test_and_insert(c->checksum, 12);
            if (count == 1 || (count == 6 && c->lbd > 6) || (count == 11 && c->lbd > 2))
            {
               filtered.push_back(c);
            }
            if (count == 6 && c->lbd > 6)
            { // to promote (tiers2) and share
               c->lbd = 6;
               ++stats.promotionTiers2;
               ++stats.receivedDuplicas;
            }
            else if (count == 6)
            {
               ++stats.alreadyTiers2;
            }
            else if (count == 11 && c->lbd > 2)
            { // to promote (core) and share
               c->lbd = 2;
               ++stats.promotionCore;
               ++stats.receivedDuplicas;
            }
            else if (count == 11)
            {
               ++stats.alreadyCore;
            }
         }
         stats.receivedClauses += unfiltered.size();
         stats.receivedDuplicas += (unfiltered.size() - filtered.size());
      }
      else
      {
         producer->exportClauses(filtered);
         stats.receivedClauses += filtered.size();
      }
      for (auto cls : filtered)
      {
         this->databases[producer->id]->addClause(cls);
         /* TODO separate units to a static attribute */
      }
      unfiltered.clear();
      filtered.clear();

      // get selection and checks the used percentage if producer is a solver
      producer->accept(this);

      stats.sharedClauses += filtered.size();

      for (auto consumer : consumers)
      {
         if (producer->id != consumer->id)
         {
            consumer->importClauses(filtered); // if imported ref++
         }
      }
   }

   round++;

   LOG2("[HordeSat] received cls %ld, shared cls %ld", stats.receivedClauses, stats.sharedClauses);
   if (globalEnding)
      return true;
   return false;
}

void HordeSatSharing::visit(SolverCdclInterface *solver)
{
   LOG4("[HordeSat] Visiting the solver %d", solver->id);
   int used, usedPercent, selectCount;

   used = this->databases[solver->id]->giveSelection(filtered, literalPerRound, &selectCount);
   usedPercent = (100 * used) / literalPerRound;

   if (usedPercent < 75)
   {
      solver->increaseClauseProduction();
      LOG4("[HordeSat] production increase for solver %d.", solver->id);
   }
   else if (usedPercent > 98)
   {
      solver->decreaseClauseProduction();
      LOG4("[HordeSat] production decrease for solver %d.", solver->id);
   }

   if (selectCount > 0)
   {
      LOG4("[HordeSat] filled %d%% of its buffer %.2f", usedPercent, used / (float)selectCount);
   }
   else
   {
      LOG4("[HordeSat] didn't produce any clause");
   }
}

void HordeSatSharing::visit(SharingEntity *sh_entity)
{
   LOG4("[HordeSat] Visiting the sharing entity %d", sh_entity->id);

   this->databases[sh_entity->id]->giveSelection(filtered, literalPerRound);
}

#ifndef NDIST

void HordeSatSharing::visit(GlobalDatabase *g_base)
{
   LOG4("[HordeSat] Visiting the global database %d", g_base->id);

   this->databases[g_base->id]->giveSelection(filtered, literalPerRound);

   LOG2("[HordeSat] Added %d clauses imported from another process", filtered.size());
}
#endif