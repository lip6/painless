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
#include "SimpleSharing.h"

#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "painless.h"

SimpleSharing::SimpleSharing(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers) : LocalSharingStrategy(producers, consumers)
{
    this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);

    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    /*TODO: make it a class attribute used in a callback*/
    unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

    for (auto &producer : this->producers)
    {
        producer->setLbdLimit(lbdLimit);
    }

    this->database = new ClauseDatabaseVector();

    if (Parameters::getBoolParam("dup"))
    {
        this->filter = new BloomFilter();
    }

    LOGSTAT("[Simple] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime);
}

SimpleSharing::SimpleSharing(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers) : LocalSharingStrategy(producers, consumers)
{
    this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);

    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    /*TODO: make it a class attribute used in a callback*/
    unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

    for (auto &producer : this->producers)
    {
        producer->setLbdLimit(lbdLimit);
    }

    this->database = new ClauseDatabaseVector();

    if (Parameters::getBoolParam("dup"))
    {
        this->filter = new BloomFilter();
    }

    LOGSTAT("[Simple] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime);
}

SimpleSharing::~SimpleSharing()
{
    delete database;
    if (Parameters::getBoolParam("dup"))
    {
        delete filter;
    }
}

bool SimpleSharing::doSharing()
{
    if (globalEnding)
        return true;

    if (this->mustAddEntities)
   {
      LOGDEBUG1("[Hordesat] Adding new entities");
      this->addLock.lock();
      this->addSharingEntities(this->addProducers, this->producers);
      this->addSharingEntities(this->addConsumers, this->consumers);
      this->addLock.unlock();
      this->mustAddEntities = false;
   }

   if (this->mustRemoveEntities)
   {
      LOGDEBUG1("[Hordesat] Removing old entities");
      this->removeLock.lock();
      this->removeSharingEntities(this->removeProducers, this->producers);
      this->removeSharingEntities(this->removeConsumers, this->consumers);
      this->removeLock.unlock();
      this->mustRemoveEntities = false;
   }
    
    // 1- Fill the database using all the producers
    for (auto producer : producers)
    {
        unfiltered.clear();
        filtered.clear();

        // TODO: a separate inline function for dup and no dup used to set a function pointer that will be used here
        if (Parameters::getBoolParam("dup"))
        {
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
        }
        else
        {
            producer->exportClauses(filtered);
            stats.receivedClauses += filtered.size();
        }

        // if solver checks the usage percentage else nothing fancy to do
        // producer->accept(this);

        for (auto cls : filtered)
        {
            this->database->addClause(cls); // if not added it is released : ref = 0
        }
    }

    unfiltered.clear();
    filtered.clear();
    // 2- Get selection
    // consumer receives the same amount as in the original
    this->database->giveSelection(filtered, literalPerRound * producers.size());

    stats.sharedClauses += filtered.size();

    // 3-Send the best clauses (all producers included) to all consumers
    for (auto consumer : consumers)
    {
        for (auto cls : filtered)
        {
            if (cls->from != consumer->id)
            {
                consumer->importClause(cls);
            }
        }
    }

    LOG1("[SimpleShr] received cls %ld, shared cls %ld", stats.receivedClauses, stats.sharedClauses);

    if (globalEnding)
        return true;
    return false;
}

void SimpleSharing::visit(SolverCdclInterface *solver)
{
    LOG4("[SimpleShr] Visiting the solver %d", solver->id);
}

void SimpleSharing::visit(SharingEntity *sh_entity)
{
    LOG4("[SimpleShr] Visiting the sh_entity %d", sh_entity->id);
}

#ifndef NDIST
void SimpleSharing::visit(GlobalDatabase *g_base)
{
    LOG4("[SimpleShr] Visiting the global database %d", g_base->id);

    LOG2("[SimpleShr] Added %d clauses imported from another process", filtered.size());
}
#endif