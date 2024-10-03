#pragma once 

#include "./LocalStrategies/HordeSatSharing.h"
#include "./LocalStrategies/HordeSatSharingAlt.h"
#include "./LocalStrategies/HordeStrSharing.h"
#include "./LocalStrategies/SimpleSharing.h"
#include "./Sharer.h"

#ifndef NDIST
#include "sharing/GlobalStrategies/AllGatherSharing.h"
#include "sharing/GlobalStrategies/MallobSharing.h"
#include "sharing/GlobalStrategies/RingSharing.h"
#endif

#include "./utils/ErrorCodes.h"
#include "./utils/Parameters.h"
#include "./utils/MpiUtils.h"

#include <vector>

#define LOCAL_SHARING_STRATEGY_COUNT 5

struct SharingStrategyFactory
{
    static int selectedLocal;
    static int selectedGlobal;

    static void instantiateLocalStrategies(int strategyNumber, std::vector<std::shared_ptr<LocalSharingStrategy>> &localStrategies, const std::vector<std::shared_ptr<GlobalDatabase>> &globalDatabases, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers);

#ifndef NDIST
    static void instantiateGlobalStrategies(int strategyNumber, const std::vector<std::shared_ptr<GlobalDatabase>> &globalDatabases, std::vector<std::shared_ptr<GlobalSharingStrategy>> &globalStrategies);
#endif

    static void launchSharers(std::vector<std::shared_ptr<SharingStrategy>> &sharingStrategies, std::vector<std::unique_ptr<Sharer>> &sharers);

    static void addEntitiesToLocal(std::vector<std::shared_ptr<LocalSharingStrategy>> &localStrategies, std::vector<std::shared_ptr<SolverCdclInterface>> &newSolvers);
};