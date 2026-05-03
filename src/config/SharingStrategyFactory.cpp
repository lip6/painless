#include "sharing/LocalStrategies/HordeSatSharing.hpp"
#include "sharing/LocalStrategies/SimpleSharing.hpp"

#include "sharing/GlobalStrategies/AllGatherSharing.hpp"
#include "sharing/GlobalStrategies/GenericGlobalSharing.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "config/ClauseDatabaseFactory.hpp"
#include "config/SharingStrategyFactory.hpp"

#include "utils/StringUtils.hpp"

SharingStrategyFactory::SharingStrategyFactory(const Parameters& parameters)
  : m_parameters(parameters)
{
}

std::shared_ptr<SharingStrategy>
SharingStrategyFactory::createStrategy(
  const std::string& originalName,
  std::shared_ptr<ClauseDatabase>& database)
{
  std::shared_ptr<SharingStrategy> strat;
  std::string name = pl::str::toLower(originalName);

  if (name == "hordesat")
    strat = std::make_shared<HordeSatSharing>(database);
  else if (name == "simple")
    strat = std::make_shared<SimpleSharing>(database);
  else
    PABORT(PERR_NOT_SUPPORTED, "Sharing Strategy %s is unknown", name.c_str());

  return strat;
}

void
SharingStrategyFactory::instantiateLocalStrategies(
  int strategyNumber,
  std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
  std::vector<std::shared_ptr<SharingEntity>>& allEntities)
{
  if (allEntities.size() < 2) {
    LOGWARN(
      "Not enough SharingEntities, SharingStrategy %d will not be instantiated",
      strategyNumber);
    return;
  }

  std::vector<std::shared_ptr<SharingEntity>> firstHalf(
    allEntities.begin(), allEntities.begin() + allEntities.size() / 2);
  std::vector<std::shared_ptr<SharingEntity>> secondHalf(
    allEntities.begin() + allEntities.size() / 2, allEntities.end());

  if (strategyNumber == 0) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distShr(
      1, LOCAL_SHARING_STRATEGY_COUNT);
    strategyNumber = distShr(rng);
  }

  // Initialize required fields of the Database Factory
  ClauseDatabaseFactory clauseDBFactory(m_parameters.maxClauseSize,
                                        100'000,
                                        2,
                                        1,
                                        m_parameters.defaultClauseBufferSize);

  unsigned currentSize = localStrategies.size();

  std::shared_ptr<ClauseDatabase> lsharedDB =
    clauseDBFactory.createDatabase(m_parameters.localSharingDB.at(0));
  std::shared_ptr<ClauseDatabase> lsharedDB2 =
    clauseDBFactory.createDatabase(m_parameters.localSharingDB.at(0));

  if (strategyNumber == 1) {
    LOG0("LSTRAT>> HordeSatSharing(1Grp)");
    auto strat = std::make_shared<HordeSatSharing>(
      allEntities.size(),
      m_parameters.sharedLiteralsPerProducer,
      m_parameters.hordeInitialLbdLimit,
      m_parameters.hordeInitRound,
      std::chrono::microseconds(m_parameters.sharingSleep),
      lsharedDB,
      allEntities);
    // Add the strategy as a client the entities
    std::stringstream producersList;
    for (auto entity : allEntities) {
      entity->addClient(strat);
      producersList << std::to_string(entity->getSharingId()) << ",";
    }
    strat->configure("producer-ids", producersList.str());
    strat->markConfigured();

    localStrategies.push_back(strat);
  } else if (strategyNumber == 2) {

    if (allEntities.size() <= 2) {
      LOGERROR("Please select another sharing strategy other than 2 if you "
               "want to have %d solvers.",
               allEntities.size());
      LOGERROR("If you used -dist option, the strategies may not work");
      exit(PWARN_LSTRAT_CPU_COUNT);
    }

    LOG0("LSTRAT>> HordeSatSharing (2Grp of producers, Common Per Size "
         "Database)");
    auto stratOne = std::make_shared<HordeSatSharing>(
      firstHalf.size(),
      m_parameters.sharedLiteralsPerProducer,
      m_parameters.hordeInitialLbdLimit,
      m_parameters.hordeInitRound,
      std::chrono::microseconds(m_parameters.sharingSleep),
      lsharedDB,
      allEntities);
    std::stringstream producersList;
    for (auto entity : firstHalf) {
      entity->addClient(stratOne);
      producersList << std::to_string(entity->getSharingId()) << ",";
    }
    stratOne->configure("producer-ids", producersList.str());
    stratOne->markConfigured();

    auto stratTwo = std::make_shared<HordeSatSharing>(
      secondHalf.size(),
      m_parameters.sharedLiteralsPerProducer,
      m_parameters.hordeInitialLbdLimit,
      m_parameters.hordeInitRound,
      std::chrono::microseconds(m_parameters.sharingSleep),
      lsharedDB2,
      allEntities);
    producersList.clear();
    for (auto entity : secondHalf) {
      entity->addClient(stratTwo);
      producersList << std::to_string(entity->getSharingId()) << ",";
    }
    stratTwo->configure("producer-ids", producersList.str());
    stratTwo->markConfigured();

  } else if (strategyNumber == 3) {
    LOG0("LSTRAT>> SimpleSharing (Common Per Size Database)");

    localStrategies.emplace_back(new SimpleSharing(
      m_parameters.simpleShareLimit,
      m_parameters.sharedLiteralsPerProducer * allEntities.size(),
      std::chrono::microseconds(m_parameters.sharingSleep),
      lsharedDB,
      allEntities));
    localStrategies.back()->markConfigured();
  } else {
    LOGERROR("The sharing strategy number chosen isn't correct. Sharing is "
             "disabled !");
  }
}

#ifndef NDIST
void
SharingStrategyFactory::instantiateGlobalStrategies(
  int strategyNumber,
  const int mpiRank,
  const int mpiWorldSize,
  std::vector<std::shared_ptr<GlobalSharingStrategy>>& globalStrategies)
{
  int right_neighbor = (mpiRank - 1 + mpiWorldSize) % mpiWorldSize;
  int left_neighbor = (mpiRank + 1) % mpiWorldSize;
  std::vector<int> subscriptions;
  std::vector<int> subscribers;

  ClauseDatabaseFactory clauseDBFactory(m_parameters.maxClauseSize,
                                        m_parameters.globalSharedLiterals * 10,
                                        2,
                                        1,
                                        m_parameters.defaultClauseBufferSize);

  std::shared_ptr<ClauseDatabase> gsharedDB =
    clauseDBFactory.createDatabase(m_parameters.globalSharingDB.at(0));

  switch (strategyNumber) {
    case 0:
      LOGWARN("For now, gshr-strat at 0 is AllGatherSharing, a future default "
              "one is in dev");
    case 1:
      LOG0("GSTRAT>> AllGatherSharing");
      globalStrategies.emplace_back(new AllGatherSharing(
        m_parameters.globalSharedLiterals,
        mpiRank,
        mpiWorldSize,
        std::chrono::microseconds(m_parameters.globalSharingSleep),
        gsharedDB));
      break;
    case 2:
      LOG0("GSTRAT>> MallobSharing");
      globalStrategies.emplace_back(new MallobSharing(
        mpiRank,
        mpiWorldSize,
        std::chrono::microseconds(m_parameters.globalSharingSleep),
        m_parameters.globalSharedLiterals,
        m_parameters.mallobMaxBufferSize,
        m_parameters.mallobLBDLimit,
        m_parameters.mallobSizeLimit,
        m_parameters.mallobSharingsPerSecond,
        m_parameters.mallobMaxCompensation,
        m_parameters.mallobResharePeriod,
        gsharedDB));
      break;
    case 3:
      LOG0("GSTRAT>> GenericGlobalSharing As RingSharing");
      subscriptions.push_back(right_neighbor);
      subscriptions.push_back(left_neighbor);
      subscribers.push_back(right_neighbor);
      subscribers.push_back(left_neighbor);

      globalStrategies.emplace_back(new GenericGlobalSharing(
        m_parameters.globalSharedLiterals,
        mpiWorldSize,
        subscriptions,
        subscribers,
        std::chrono::microseconds(m_parameters.globalSharingSleep),
        gsharedDB));
      break;
    default:
      LOGERROR("Global Strategy %d is not defined", strategyNumber);
      std::abort();
      break;
  }
}
#endif

void
SharingStrategyFactory::createSharers(
  PainlessImpl& manager,
  std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies,
  std::vector<std::shared_ptr<Sharer>>& sharers)
{
  if (m_parameters.oneSharer) {
    sharers.emplace_back(new Sharer(manager, 0, sharingStrategies));
  } else {
    for (unsigned int i = 0; i < sharingStrategies.size(); i++) {
      sharers.emplace_back(new Sharer(manager, i, sharingStrategies[i]));
    }
  }
}

std::vector<std::shared_ptr<SharingStrategy>>
SharingStrategyFactory::configureSharing(
  const int mpiRank,
  const int mpiWorldSize,
  std::vector<std::shared_ptr<SharingEntity>>& sharingEntities)
{
  std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;
#ifndef NDIST
  std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;

  if (m_parameters.enableMallob && m_parameters.enableDistributed) {
    // only global strategy
    instantiateGlobalStrategies(2, mpiRank, mpiWorldSize, globalStrategies);
    for (auto& entity : sharingEntities) {
      entity->addClient(globalStrategies.back()); // entity -cls-> gstrat
      globalStrategies.back()->addClient(entity); // gstrat -cls-> entity
    }
  } else {

    if (m_parameters.enableDistributed) {
      instantiateGlobalStrategies(m_parameters.globalSharingStrategy,
                                  mpiRank,
                                  mpiWorldSize,
                                  globalStrategies);
    }
    // Global sharing entities are added to the list to be connected with
    // local sharing strategies
    sharingEntities.insert(
      sharingEntities.end(), globalStrategies.begin(), globalStrategies.end());
    instantiateLocalStrategies(
      m_parameters.sharingStrategy, sharingStrategies, sharingEntities);
  }
  sharingStrategies.insert(
    sharingStrategies.end(), globalStrategies.begin(), globalStrategies.end());
#else
  instantiateLocalStrategies(
    m_parameters.sharingStrategy, sharingStrategies, sharingEntities);
#endif

  return sharingStrategies;
}