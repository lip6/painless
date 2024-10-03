#include "painless.h"
#include "SharingStrategyFactory.h"

int SharingStrategyFactory::selectedLocal = 0;
int SharingStrategyFactory::selectedGlobal = 0;

void SharingStrategyFactory::instantiateLocalStrategies(int strategyNumber, std::vector<std::shared_ptr<LocalSharingStrategy>> &localStrategies, const std::vector<std::shared_ptr<GlobalDatabase>> &globalDatabases, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers)
{
    std::vector<std::shared_ptr<SharingEntity>> allEntities;
    allEntities.insert(allEntities.end(), cdclSolvers.begin(), cdclSolvers.end());
    allEntities.insert(allEntities.end(), globalDatabases.begin(), globalDatabases.end());

    if (strategyNumber == 0)
    {
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> distShr(1, LOCAL_SHARING_STRATEGY_COUNT);
        strategyNumber = distShr(rng);
    }

    // TODO: decide weither or not to make a unique_ptr inside Sharer
    switch (strategyNumber)
    {
    case 1:
        LOG("LSTRAT>> HordeSatSharing(1Grp)");
        localStrategies.emplace_back(new HordeSatSharing(allEntities, allEntities));
        break;
    case 2:
        if (cdclSolvers.size() <= 2)
        {
            LOGERROR("Please select another sharing strategy other than 2 or 3 if you want to have %d solvers.", cdclSolvers.size());
            LOGERROR("If you used -dist option, the strategies may not work");
            exit(PWARN_LSTRAT_CPU_COUNT);
        }
        LOGERROR("NOT IMPLEMENTED!! SOON");
        exit(0);
        break;
    case 3:
        if (cdclSolvers.size() <= 2)
        {
            LOGERROR("Please select another sharing strategy other than 2 or 3 if you want to have %d solvers.", cdclSolvers.size());
            LOGERROR("If you used -dist option, the strategies may not work");
            exit(PWARN_LSTRAT_CPU_COUNT);
        }

        LOG("LSTRAT>> HordeSatSharingAlt(2Grp of producers)");

        localStrategies.emplace_back(new HordeSatSharingAlt({allEntities.begin(), allEntities.begin() + allEntities.size() / 2}, {allEntities.begin(), allEntities.end()}));
        localStrategies.emplace_back(new HordeSatSharingAlt({allEntities.begin() + allEntities.size() / 2, allEntities.end()}, {allEntities.begin(), allEntities.end()}));
        break;
    case 4:
        LOG("LSTRAT>> HordeSatSharingAlt(1Grp)");

        localStrategies.emplace_back(new HordeSatSharingAlt(allEntities, allEntities));
        break;
    case 5:
        LOG("LSTRAT>> SimpleSharing");

        localStrategies.emplace_back(new SimpleSharing(allEntities, allEntities));
        break;
    default:
        LOGWARN("The sharing strategy number chosen isn't correct, use a value between 1 and %d", LOCAL_SHARING_STRATEGY_COUNT);
        break;
    }

    SharingStrategyFactory::selectedLocal = strategyNumber;
}

#ifndef NDIST
void SharingStrategyFactory::instantiateGlobalStrategies(int strategyNumber, const std::vector<std::shared_ptr<GlobalDatabase>> &globalDatabases, std::vector<std::shared_ptr<GlobalSharingStrategy>> &globalStrategies)
{
    switch (strategyNumber)
    {
    case 1:
        LOG("GSTRAT>> AllGatherSharing");
        globalStrategies.emplace_back(new AllGatherSharing(globalDatabases[0]));
        break;
    case 2:
        LOG("GSTRAT>> MallobSharing");
        globalStrategies.emplace_back(new MallobSharing(globalDatabases[0]));
        break;
    case 3:
        LOG("GSTRAT>> RingSharing");
        globalStrategies.emplace_back(new RingSharing(globalDatabases[0]));
        break;
    default:
        LOGWARN("GSTART>> Default GSTRAT selected: MallobSharing");
        break;
    }

    /*Since bootstraping, loop is not optimzed*/
    for (int i = 0; i < globalStrategies.size() && dist; i++)
    {
        if (!globalStrategies.at(i)->initMpiVariables())
        {
            LOGERROR("The global sharing strategy %d wasn't able to initalize its MPI variables", i);
            TESTRUNMPI(MPI_Finalize());
            dist = false;
            globalStrategies.erase(globalStrategies.begin()+i);
        }
    }

    SharingStrategyFactory::selectedGlobal = strategyNumber;
}
#endif

void SharingStrategyFactory::launchSharers(std::vector<std::shared_ptr<SharingStrategy>> &sharingStrategies, std::vector<std::unique_ptr<Sharer>> &sharers)
{
    if (Parameters::getBoolParam("one-sharer"))
    {
        sharers.emplace_back(new Sharer(0, sharingStrategies));
    }
    else
    {
        for (int i = 0; i < sharingStrategies.size(); i++)
        {
            sharers.emplace_back(new Sharer(i, sharingStrategies[i]));
        }
    }
}

void SharingStrategyFactory::addEntitiesToLocal(std::vector<std::shared_ptr<LocalSharingStrategy>> &localStrategies, std::vector<std::shared_ptr<SolverCdclInterface>> &newSolvers)
{
    switch (SharingStrategyFactory::selectedLocal)
    {
    case 1:
    case 4:
    case 5:
        LOG("UPDATE>> 1Grp");
        for (auto newSolver : newSolvers)
        {
            localStrategies[0]->addConsumer(newSolver);
            localStrategies[0]->addProducer(newSolver);
        }
        break;
    case 2:
    case 3:
        LOG("UPDATE>> 2Grp of producers");

        for (int i = 0; i < newSolvers.size() / 2; i++)
        {
            localStrategies[0]->addConsumer(newSolvers[i]);
            localStrategies[0]->addProducer(newSolvers[i]);

            localStrategies[1]->addConsumer(newSolvers[i]);
        }

        for (int i = newSolvers.size() / 2; i < newSolvers.size(); i++)
        {
            localStrategies[0]->addConsumer(newSolvers[i]);

            localStrategies[1]->addProducer(newSolvers[i]);
            localStrategies[1]->addConsumer(newSolvers[i]);
        }
        break;
    default:
        LOGWARN("The sharing strategy number chosen isn't correct, use a value between 1 and %d", LOCAL_SHARING_STRATEGY_COUNT);
        break;
    }
}
