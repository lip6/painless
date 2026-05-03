#include "config/TopologyConfigurator.hpp"

#include "config/ClauseDatabaseFactory.hpp"
#include "config/Configurable.hpp"
#include "config/SharingStrategyFactory.hpp"
#include "config/SolverFactory.hpp"
#include "config/WorkingStrategyRegistry.hpp"
#include "sharing/Sharer.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

#include <boost/json.hpp>
#include <sstream>
#include <unordered_map>

namespace topology {

static void
setParams(std::shared_ptr<Configurable> configurable,
          const boost::json::object& params)
{
  for (auto& keyVal : params) {
    const auto& v = keyVal.value();
    if (v.is_int64()) {
      configurable->configure(keyVal.key(), static_cast<int>(v.as_int64()));
    } else if (v.is_bool()) {
      // Configurable has no bool overload for now, so we go through int
      configurable->configure(keyVal.key(), v.as_bool() ? 1 : 0);
    } else if (v.is_double()) {
      configurable->configure(keyVal.key(), v.as_double());
    } else if (v.is_string()) {
      configurable->configure(keyVal.key(),
                              boost::json::value_to<std::string>(v));
    } else {
      LOGWARN("Ignoring topology param '%s': unsupported JSON value kind",
              keyVal.key().data());
    }
  }
}

void
buildPermanentWorkers(const TopologyDesc& topology, PainlessImpl& painless)
{
  std::vector<std::shared_ptr<SolverCDCLInterface>> cdcls;
  std::vector<std::shared_ptr<LocalSearchInterface>> localSearchers;

  FullClauseReader fullReader = [&painless](ClauseReader clauseReader,
                                            uint startIdx) -> uint {
    return painless.readClauses(clauseReader, startIdx);
  };

  // Iterate descriptors in declared order so solverId is the same as in the
  // file.
  for (size_t i = 0; i < topology.cdclSolvers.size(); i++) {
    auto& cdclDesc = topology.cdclSolvers[i];

    /* Instantiate the Database using the right template */
    const DatabaseDesc& templateDB = topology.databaseTemplates.at(
      topology.databaseTemplateIndex.at(cdclDesc.importDBId));
    std::shared_ptr<ClauseDatabase> solverDB =
      ClauseDatabaseFactory::createDatabase(templateDB.name);
    setParams(solverDB, templateDB.params);
    /* after this, setParams won't work */
    solverDB->markConfigured();

    /* Instantiate the solver*/
    std::shared_ptr<SolverCDCLInterface> solver =
      SolverFactory::createCDCLSolver(i, cdclDesc.name, solverDB, fullReader);
    setParams(solver, cdclDesc.params);
    solver->markConfigured();

    cdcls.push_back(solver);
  }
  assert(cdcls.size() == topology.cdclSolvers.size());

  for (size_t i = 0; i < topology.localSearchers.size(); i++) {
    auto& lsDesc = topology.localSearchers[i];

    std::shared_ptr<LocalSearchInterface> solver =
      SolverFactory::createLocalSearcher(i, lsDesc.name, fullReader);
    setParams(solver, lsDesc.params);
    solver->markConfigured();

    localSearchers.push_back(solver);
  }
  assert(localSearchers.size() == topology.localSearchers.size());

  std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;
  for (const auto& shrStratDesc : topology.sharingStrategies) {
    /* Create database from template first */
    const DatabaseDesc& templateDB = topology.databaseTemplates.at(
      topology.databaseTemplateIndex.at(shrStratDesc.dbId));

    /* Then sharing strategy */
    std::shared_ptr<ClauseDatabase> stratDB =
      ClauseDatabaseFactory::createDatabase(templateDB.name);
    setParams(stratDB, templateDB.params);
    stratDB->markConfigured();

    std::string name = shrStratDesc.name;
    auto strat = SharingStrategyFactory::createStrategy(name, stratDB);
    setParams(strat, shrStratDesc.params);

    sharingStrategies.push_back(strat);
  }
  assert(sharingStrategies.size() == topology.sharingStrategies.size());

  // Resolve producers/clients in a second pass so sharing strategies can
  // reference each other.
  for (const auto& shrStratDesc : topology.sharingStrategies) {
    std::shared_ptr<SharingStrategy> strat =
      sharingStrategies[topology.sharingStrategyIndex.at(shrStratDesc.id)];

    // Sharing is subscription-based: an entity calls addClient(strat) on each
    // of its producers, and the producer pushes clauses to its subscribers.

    // This producers list is an implementation requirement for HordeSat to work
    std::stringstream producersList;

    for (const std::string& producer : shrStratDesc.producerIds) {
      std::shared_ptr<SharingEntity> producerEntity = nullptr;
      /* Get the correct sharing entity using its ID (we hypothesize that there
       * are no duplicates)*/
      if (topology.cdclSolverIndex.contains(producer))
        producerEntity = cdcls[topology.cdclSolverIndex.at(producer)];
      else if (topology.sharingStrategyIndex.contains(producer))
        producerEntity =
          sharingStrategies[topology.sharingStrategyIndex.at(producer)];
      else
        PABORT(PERR_TOPOLOGY,
               "Entity %s was not instantiated",
               producer.c_str());
      producersList << std::to_string(producerEntity->getSharingId()) << ",";
      // The sharing strategy subscribes to its producers
      producerEntity->addClient(strat);
    }

    // HordeSat's filter is production-rate based, so the strategy needs the
    // ids of its producers up-front to size its per-producer data structures.
    // The subscription model only flows producer->client at runtime, hence
    // this configure() call is the path used to inject that information.
    if (pl::str::toLower(shrStratDesc.name) == "hordesat") {
      strat->configure("producer-ids", producersList.str());
    }

    for (const std::string& client : shrStratDesc.clientIds) {
      std::shared_ptr<SharingEntity> clientEntity = nullptr;
      /* Fetch the sharing entity which can either be a solver or a
       * strategy */
      if (topology.cdclSolverIndex.contains(client))
        clientEntity = cdcls[topology.cdclSolverIndex.at(client)];
      else if (topology.sharingStrategyIndex.contains(client))
        clientEntity =
          sharingStrategies[topology.sharingStrategyIndex.at(client)];
      else
        PABORT(PERR_TOPOLOGY,
               "Entity %s was not instantiated",
               client.c_str());
      // Clients subscribe to the sharing strategy
      strat->addClient(clientEntity);
    }

    strat->markConfigured();
  }

  /* Working strategy uses ids of solvers only, in Upcoming update we will
   * separate them by type */
  std::vector<std::shared_ptr<SolverInterface>> solvers;
  for (const std::string& solver : topology.workingStrategy.solverIds) {
    std::shared_ptr<SolverInterface> solverEntity = nullptr;

    /* Fetch solver that is either a cdcl or a local searcher */
    if (topology.cdclSolverIndex.contains(solver)) {
      auto cdcl = cdcls.at(topology.cdclSolverIndex.at(solver));
      /* We add it to the painless solver for the interface calls (addLiteral,
      assume, ...) to work (need to rethink it maybe) */
      painless.addSolver(cdcl);
      solverEntity = cdcl;
    } else if (topology.localSearcherIndex.contains(solver)) {
      auto localSearcher =
        localSearchers.at(topology.localSearcherIndex.at(solver));
      painless.addSolver(localSearcher);
      solverEntity = localSearcher;
    } else {
      PABORT(
        PERR_TOPOLOGY, "Entity %s was not instantiated", solver.c_str());
    }
    solvers.push_back(solverEntity);
  }

  std::shared_ptr<WorkingStrategy> mainStrategy =
    WorkingStrategyRegistry::create(
      topology.workingStrategy.name, painless, solvers);
  setParams(mainStrategy, topology.workingStrategy.params);
  mainStrategy->markConfigured();

  // The solver now knows what to execute for solving
  painless.setWorkingStrategy(mainStrategy);

  /* Finally Sharers */
  uint sharerId = 0;
  for (const auto& sharerDesc : topology.sharers) {
    std::vector<std::shared_ptr<SharingStrategy>> strategies;
    for (const auto& stratId : sharerDesc.strategyIds)
      strategies.push_back(
        sharingStrategies[topology.sharingStrategyIndex.at(stratId)]);
    // Add it to painless in order to control its startup and termination
    // Need to rethink the painless reference in sharer and workers
    painless.addSharer(
      std::make_shared<Sharer>(painless, sharerId, strategies));
    sharerId++;
  }

  LOG1("Instantiated the main working strategy with the created %zu solvers",
       solvers.size());
}

} // namespace topology
