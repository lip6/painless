#ifndef NDIST

#include "sharing/GlobalDatabase.h"
#include "utils/Parameters.h"
#include "painless.h"
#include <numeric>
#include <sstream>

GlobalDatabase::GlobalDatabase(int entity_id, std::shared_ptr<ClauseDatabase> firstDB, std::shared_ptr<ClauseDatabase> secondDB) : SharingEntity(entity_id), clausesToSend(firstDB), clausesReceived(secondDB)
{
}

GlobalDatabase::~GlobalDatabase()
{
}

void GlobalDatabase::printStats()
{
    std::stringstream toSendStats, receivedStats;
    this->clausesToSend->getTotalSizes(toSendStats);
    this->clausesReceived->getTotalSizes(receivedStats);
    LOGSTAT("Global Database Statistics: Final ToSend Database Size: %d Final Received Database Size: %d\
        \n\t* Sizes of clausesToSend : %s\
        \n\t* Sizes of clausesReceived : %s",
            this->clausesToSend->getSize(), this->clausesReceived->getSize(), toSendStats.str().c_str(), receivedStats.str().c_str());
}

//---------------------------------------------------------
//             Local Import/Export Interface
//---------------------------------------------------------

bool GlobalDatabase::importClause(std::shared_ptr<ClauseExchange> cls)
{
    /* TODO: flag the clauses at export and manage in strategy ? */
    for (int lit : cls->lits)
    {
        if (this->maxVar > 0 && std::abs(lit) > maxVar)
        {
            LOGVECTOR(&cls->lits[0], cls->size, "The clause contains a new variable, thus will not be added to globalDB: ");
            return false;
        }
    }
    return this->clausesToSend->addClause(cls);
    // bloom filter is left for sharingStrategy when serializing if wanted
}

void GlobalDatabase::importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &v_cls)
{
    for (auto cls : v_cls)
    {
        importClause(cls);
    }
}

void GlobalDatabase::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls)
{
    clausesReceived->getClauses(v_cls);
}

void GlobalDatabase::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls, unsigned totalSize)
{
    clausesReceived->giveSelection(v_cls, totalSize);
}

//---------------------------------------------------------
//           Useful functions for globalStrategy
//---------------------------------------------------------

void GlobalDatabase::addReceivedClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls)
{
    for (auto cls : v_cls)
    {
        addReceivedClause(cls);
    }
}

void GlobalDatabase::getClausesToSend(std::vector<std::shared_ptr<ClauseExchange>> &v_cls)
{
    clausesToSend->getClauses(v_cls);
}

void GlobalDatabase::getClausesToSend(std::vector<std::shared_ptr<ClauseExchange>> &v_cls, unsigned literals_count)
{
    clausesToSend->giveSelection(v_cls, literals_count);
}
#endif
