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
#include "HordeStrSharing.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/BloomFilter.h"
#include "painless.h"

HordeStrSharing::HordeStrSharing(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers, std::shared_ptr<SolverCdclInterface> _reducer) : LocalSharingStrategy(producers, consumers), reducer(_reducer)
{
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;

    unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

    for (auto &producer : this->producers)
    {
        producer->setLbdLimit(lbdLimit);
    }
    // number of round corresponding to 5% of the 5000s timeout
    this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);
    if (sleepTime)
        this->roundBeforeIncrease = 250000000 / sleepTime;
    else
    {
        LOGWARN("sleepTime of sharing strategy is set to: %d", sleepTime);
        this->roundBeforeIncrease = 250000000 / 500000;
    }
    LOGSTAT("[HordeStreng] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease);
}

HordeStrSharing::HordeStrSharing(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers, std::shared_ptr<SolverCdclInterface> _reducer) : LocalSharingStrategy(producers, consumers), reducer(_reducer)
{
    this->literalPerRound = Parameters::getIntParam("shr-lit", 1500);
    this->initPhase = true;

    unsigned lbdLimit = Parameters::getIntParam("shr-initial-lbd", 2);

    for (auto &producer : this->producers)
    {
        producer->setLbdLimit(lbdLimit);
    }
    // number of round corresponding to 5% of the 5000s timeout
    this->sleepTime = Parameters::getIntParam("shr-sleep", 500000);
    if (sleepTime)
        this->roundBeforeIncrease = 250000000 / sleepTime;
    else
    {
        LOGWARN("sleepTime of sharing strategy is set to: %d", sleepTime);
        this->roundBeforeIncrease = 250000000 / 500000;
    }
    LOGSTAT("[HordeStreng] Producers: %d, Consumers: %d, Initial Lbd limit: %u, sleep time: %d, round before increase: %d", producers.size(), consumers.size(), lbdLimit, sleepTime, roundBeforeIncrease);
}

HordeStrSharing::~HordeStrSharing()
{
    for (auto pair : this->databases)
    {
        delete pair.second;
    }
}

bool HordeStrSharing::doSharing()
{
    if (globalEnding)
        return true;

    // Producers to reducer
    for (auto producer : producers)
    {
        if (!this->databases.count(producer->id))
        {
            this->databases[producer->id] = new ClauseDatabaseVector();
        }
        tmp.clear();

        producer->exportClauses(tmp);
        stats.receivedClauses += tmp.size();

        for (auto cls : tmp)
        {
            this->databases[producer->id]->addClause(cls); // if not added it is released : ref = 0
        }
        tmp.clear();

        // get selection and checks the used percentage if producer is a solver
        producer->accept(this);

        stats.sharedClauses += tmp.size();

        reducer->importClauses(tmp);
    }

    // Reducer to consumers
    if (!this->databases.count(reducer->id))
    {
        this->databases[reducer->id] = new ClauseDatabaseVector();
    }
    tmp.clear();

    reducer->exportClauses(tmp);
    stats.receivedClauses += tmp.size();

    for (auto cls : tmp)
    {
        this->databases[reducer->id]->addClause(cls); // if not added it is released : ref = 0
    }
    tmp.clear();

    reducer->accept(this);

    stats.sharedClauses += tmp.size();

    for (auto consumer : consumers)
    {
        consumer->importClauses(tmp);
    }
    round++;

    LOG1("[HordeStr] received cls %ld, shared cls %ld", stats.receivedClauses, stats.sharedClauses);
    if (globalEnding)
        return true;

    return false;
}

void HordeStrSharing::visit(SolverCdclInterface *solver)
{
    LOG4("[HordeStr] Visiting the solver %d", solver->id);
    int used, usedPercent, selectCount;

    used = this->databases[solver->id]->giveSelection(tmp, literalPerRound, &selectCount);
    usedPercent = (100 * used) / literalPerRound;

    stats.sharedClauses += tmp.size();

    if (usedPercent < 75)
    {
        solver->increaseClauseProduction();
        LOG4("[HordeStr] production increase for solver %d.", solver->id);
    }
    else if (usedPercent > 98)
    {
        solver->decreaseClauseProduction();
        LOG4("[HordeStr] production decrease for solver %d.", solver->id);
    }

    if (selectCount > 0)
    {
        LOG4("[HordeStr] filled %d%% of its buffer %.2f", usedPercent, used / (float)selectCount);
        this->initPhase = false;
    }
}

void HordeStrSharing::visit(SharingEntity *sh_entity)
{
    LOG4("[HordeStr] Visiting the sharing entity %d", sh_entity->id);

    this->databases[sh_entity->id]->giveSelection(tmp, literalPerRound);
}

#ifndef NDIST

void HordeStrSharing::visit(GlobalDatabase *g_base)
{
    LOG4("[HordeStr] Visiting the global database %d", g_base->id);

    this->databases[g_base->id]->giveSelection(tmp, literalPerRound);

    LOG2("[HordeStr] Added %d clauses imported from another process", tmp.size());
}
#endif