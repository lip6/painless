#include "PainlessConfigurator.hpp"

#include "config/Parameters.hpp"
#include "config/SolverFactory.hpp"
#include "config/SharingStrategyFactory.hpp"
#include "config/TopologyConfigurator.hpp"
#include "config/WorkingStrategyRegistry.hpp"

namespace PainlessConfigurator {

static void
initializeParametersFromJson(const std::string& jsonPath,
                             Parameters& parameters)
{
  if (!jsonPath.empty()) {
    LOGD1("Initializing painless from the json %s", jsonPath.c_str());
    parameters.initFromJSON(jsonPath);
  } else {
    LOGD1("Parameters will keep their default values");
  }

  LOG0("Configuring Painless with parameters: ");
  LOG1("\n%s", parameters.toString().c_str());
}

static void
initializeParametersFromCLI(const int argc,
                            char const* const* const argv,
                            Parameters& parameters)
{
  parameters.initFromCLI(argc, argv);

  LOG0("Configuring Painless with parameters: ");
  LOG1("\n%s", parameters.toString().c_str());
}

static bool
createPermanentWorkersFromParameters(PainlessImpl& painless)
{
  FullClauseReader fullReader = [&painless](ClauseReader clauseReader,
                                            uint startIdx) -> uint {
    return painless.readClauses(clauseReader, startIdx);
  };

  const Parameters& parameters = painless.parameters();
  std::vector<std::shared_ptr<SolverInterface>> solvers;

  SolverFactory solverFactory(parameters);
  SharingStrategyFactory sharingFactory(parameters);

  solverFactory.createSolvers(
    parameters.cpus, 's', parameters.solver, fullReader, solvers);
  LOG1("Created all the required solvers");

  std::shared_ptr<WorkingStrategy> mainStrategy =
    WorkingStrategyRegistry::create(
      parameters.parallelStrategy, painless, solvers);

  LOG1("Instantiated the main working strategy with the created %u solvers",
       solvers.size());

  /* Sharing */
  std::vector<std::shared_ptr<SharingEntity>> sharingEntities;
  for (auto solver : solvers) {
    if (solver->getAlgoType() == SolverInterface::Type::CDCL) {
      sharingEntities.push_back(
        dynamic_pointer_cast<SolverCDCLInterface>(solver));
    }
  }
  std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;
  sharingFactory.instantiateLocalStrategies(
    parameters.sharingStrategy, sharingStrategies, sharingEntities);

  std::vector<std::shared_ptr<Sharer>> sharers;
  sharingFactory.createSharers(painless, sharingStrategies, sharers);

  LOGSTAT("Instantiated %u sharers", sharers.size());

  /* Painless Initialization */
  for (auto solver : solvers)
    painless.addSolver(solver);

  painless.setWorkingStrategy(mainStrategy);

  for (auto sharer : sharers)
    painless.addSharer(sharer);

  return true;
}

static void
buildFromParametersOrTopology(PainlessImpl& painless)
{
  const Parameters& parameters = painless.parameters();
  if (parameters.topology.empty()) {
    createPermanentWorkersFromParameters(painless);
  } else {
    topology::buildPermanentWorkers(
      topology::parseJsonTopology(parameters.topology), painless);
  }
}

bool
configurePainlessFromJson(const std::string& json, PainlessImpl& painless)
{
  Parameters parameters;
  initializeParametersFromJson(json, parameters);

  painless.setParameters(parameters);

  buildFromParametersOrTopology(painless);

  painless.setInitialized();

  return true;
}

bool
configurePainlessFromCLI(const int argc,
                         char const* const* const argv,
                         PainlessImpl& painless)
{
  Parameters parameters;
  initializeParametersFromCLI(argc, argv, parameters);

  painless.setParameters(parameters);

  buildFromParametersOrTopology(painless);

  painless.setInitialized();

  return true;
}

} // namespace PainlessConfigurator
