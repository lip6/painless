#include "config/TopologyConfigurator.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"

#include <boost/json.hpp>
#include <fstream>
#include <unordered_set>

namespace topology {

static std::vector<std::string>
jsonIdArrayToVector(std::unordered_map<std::string, uint>& idRefs,
                    boost::json::array& array,
                    const std::string& listContext)
{
  std::vector<std::string> vector;
  std::unordered_set<std::string> seen;
  for (auto& jstr : array) {
    std::string str = boost::json::value_to<std::string>(jstr);
    PABORTIF(!seen.insert(str).second,
             PERR_TOPOLOGY,
             "Duplicate id '%s' in %s",
             str.c_str(),
             listContext.c_str());
    vector.push_back(str);
    idRefs.at(str)++;
  }
  return vector;
}

TopologyDesc
parseJsonTopology(const std::string& jsonPath)
{
  std::ifstream topologyFile(jsonPath);
  PABORTIF(!topologyFile, PERR_ARGS, "Couldn't open file %s", jsonPath.c_str());

  /* Read whole json file */
  std::string line;
  std::string json;
  while (std::getline(topologyFile, line))
    json.append(line);
  topologyFile.close();

  LOGD2("Read from file %s: %s", jsonPath.c_str(), json.c_str());

  TopologyDesc topology;
  boost::json::value jsonValue;
  boost::json::object jsonObject;

  /* Parse the whole json object at once thanks to boost::json */
  try {
    jsonValue = boost::json::parse(json);
    LOGD2("JsonValue is %s an object", ((jsonValue.is_object()) ? "" : "not"));
    jsonObject = jsonValue.as_object();
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during topology parsing %s",
           e.what());
  }

  /* Start by parsing databases since they are independant */
  /* IDs must be unique */
  try {
    auto& databases = jsonObject.at("databaseTemplates").as_array();
    for (auto& database : databases) {
      auto& databaseObject = database.as_object();
      LOGD2("Parsing database: %s", boost::json::serialize(database).c_str());

      DatabaseDesc databaseDesc;
      databaseDesc.id = databaseObject.at("id").as_string();
      databaseDesc.name = databaseObject.at("type").as_string();
      /* capacity is not used for now, will come in an upcoming update */
      databaseDesc.capacity = databaseObject.at("capacity").as_int64();
      // The params are optional and are saved for building phase
      if (databaseObject.contains("params"))
        databaseDesc.params = databaseObject.at("params").as_object();

      PABORTIF(topology.idRefs.contains(databaseDesc.id),
               PERR_TOPOLOGY,
               "Duplicate database ID %s detected !",
               databaseDesc.id.c_str());
      /* Add the id to the reference counter */
      topology.idRefs.emplace(databaseDesc.id, 0);
      /* Index the new database template for easy fetch */
      topology.databaseTemplateIndex.emplace(databaseDesc.id,
                                             topology.databaseTemplates.size());
      topology.databaseTemplates.push_back(std::move(databaseDesc));
    }
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during database parsing %s",
           e.what());
  }

  /* Parse LocalSearch solvers that are independant for the other components */
  try {
    auto& localSearchers = jsonObject.at("localSearchers").as_array();
    for (auto& ls : localSearchers) {
      auto& lsObject = ls.as_object();
      LOGD2("Parsing local searcher: %s", boost::json::serialize(ls).c_str());

      LocalSearchDesc lsDesc;
      lsDesc.id = lsObject.at("id").as_string();
      lsDesc.name = lsObject.at("type").as_string();
      if (lsObject.contains("params"))
        lsDesc.params = lsObject.at("params").as_object();

      PABORTIF(topology.idRefs.contains(lsDesc.id),
               PERR_TOPOLOGY,
               "Duplicate LocalSearcher ID %s detected !",
               lsDesc.id.c_str());
      /* ref counter */
      topology.idRefs.emplace(lsDesc.id, 0);
      /* indexation */
      topology.localSearcherIndex.emplace(lsDesc.id,
                                          topology.localSearchers.size());
      topology.localSearchers.push_back(std::move(lsDesc));
    }
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during local searchers parsing %s",
           e.what());
  }

  /* Parse the solvers that are only dependant on the database templates */
  try {
    auto& cdclSolvers = jsonObject.at("cdclSolvers").as_array();
    for (auto& cdcl : cdclSolvers) {
      auto& cdclObject = cdcl.as_object();
      LOGD2("Parsing cdcl: %s", boost::json::serialize(cdcl).c_str());

      CdclSolverDesc cdclDesc;
      cdclDesc.id = cdclObject.at("id").as_string();
      cdclDesc.name = cdclObject.at("type").as_string();
      cdclDesc.importDBId = cdclObject.at("importDB").as_string();
      if (cdclObject.contains("params"))
        cdclDesc.params = cdclObject.at("params").as_object();

      PABORTIF(topology.idRefs.contains(cdclDesc.id),
               PERR_TOPOLOGY,
               "Duplicate CDCL ID %s detected !",
               cdclDesc.id.c_str());
      /* increment database template usage */
      topology.idRefs.at(cdclDesc.importDBId)++;
      /* ref counter */
      topology.idRefs.emplace(cdclDesc.id, 0);
      /* indexation */
      topology.cdclSolverIndex.emplace(cdclDesc.id,
                                       topology.cdclSolvers.size());
      topology.cdclSolvers.push_back(std::move(cdclDesc));
    }
  } catch (const std::runtime_error& e) {
    PABORT(
      PERR_UNHANDLED_EXCEPTION, "Exception during cdcl parsing %s", e.what());
  }

  /* Parse the working strategy, for now it only containes a list of solvers and
   * parameters. In the future we may make working strategies express the
   * parallelization organization */
  try {
    auto& wStratObject = jsonObject.at("workingStrategy").as_object();
    LOGD2("Parsing working strategy: %s",
          boost::json::serialize(jsonObject.at("workingStrategy")).c_str());

    topology.workingStrategy.name = wStratObject.at("name").as_string();
    /* Get the list of solver ids used as a vector and increment the ids usage
     */
    topology.workingStrategy.solverIds =
      jsonIdArrayToVector(topology.idRefs,
                          wStratObject.at("solvers").as_array(),
                          "workingStrategy.solvers");
    if (wStratObject.contains("params"))
      topology.workingStrategy.params = wStratObject.at("params").as_object();
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during working strat parsing %s",
           e.what());
  }

  /* Parse the sharing strategies that are dependant on CDCL solvers and
   * Databases */
  try {
    auto& sharingStrategies = jsonObject.at("sharingStrategies").as_array();
    for (auto& sharingStrat : sharingStrategies) {
      auto& sharingStratObject = sharingStrat.as_object();
      LOGD2("Parsing sharing strategy: %s",
            boost::json::serialize(sharingStrat).c_str());

      SharingStrategyDesc sharingStratDesc;
      sharingStratDesc.id = sharingStratObject.at("id").as_string();
      sharingStratDesc.name = sharingStratObject.at("name").as_string();
      sharingStratDesc.dbId = sharingStratObject.at("db").as_string();
      if (sharingStratObject.contains("params"))
        sharingStratDesc.params = sharingStratObject.at("params").as_object();

      PABORTIF(topology.idRefs.contains(sharingStratDesc.id),
               PERR_TOPOLOGY,
               "Duplicate SharingStrategy ID %s detected !",
               sharingStratDesc.id.c_str());
      /* increment database template usage */
      topology.idRefs.at(sharingStratDesc.dbId)++;
      /* ref counter */
      topology.idRefs.emplace(sharingStratDesc.id, 0);
      /* indexation */
      topology.sharingStrategyIndex.emplace(sharingStratDesc.id,
                                            topology.sharingStrategies.size());
      topology.sharingStrategies.push_back(std::move(sharingStratDesc));
    }

    /* Sharing strategies can be producers and consumers of each other,
    so we resolve the producer/client lists in a second pass. */
    for (auto& sharingStrat : sharingStrategies) {
      auto& sharingStratObject = sharingStrat.as_object();
      std::string id(sharingStratObject.at("id").as_string());
      SharingStrategyDesc& sharingStratDesc =
        topology.sharingStrategies.at(topology.sharingStrategyIndex.at(id));

      /* TODO verify that the ids in clients and producers are sharing entities
       * (cdcl or sharing strategies)*/
      sharingStratDesc.producerIds = jsonIdArrayToVector(
        topology.idRefs,
        sharingStratObject.at("producers").as_array(),
        "sharingStrategy '" + sharingStratDesc.id + "' producers");
      sharingStratDesc.clientIds = jsonIdArrayToVector(
        topology.idRefs,
        sharingStratObject.at("clients").as_array(),
        "sharingStrategy '" + sharingStratDesc.id + "' clients");
    }
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during sharing strategies parsing %s",
           e.what());
  }

  try {
    auto& sharers = jsonObject.at("sharers").as_array();
    for (auto& sharer : sharers) {
      auto& sharerObject = sharer.as_object();
      LOGD2("Parsing sharer: %s", boost::json::serialize(sharer).c_str());

      SharerDesc sharerDesc;
      sharerDesc.id = sharerObject.at("id").as_string();

      sharerDesc.strategyIds =
        jsonIdArrayToVector(topology.idRefs,
                            sharerObject.at("strategies").as_array(),
                            "sharer '" + sharerDesc.id + "' strategies");

      /* Indexation is useless for now*/
      topology.sharerIndex.emplace(sharerDesc.id, topology.sharers.size());
      topology.sharers.push_back(std::move(sharerDesc));
      /* Sharers are not added to idRefs (no entity references them). So No
       * duplicate check for now*/
    }
  } catch (const std::runtime_error& e) {
    PABORT(PERR_UNHANDLED_EXCEPTION,
           "Exception during sharers parsing %s",
           e.what());
  }

  for (auto& idRef : topology.idRefs) {
    PABORTIF(idRef.second == 0,
             PERR_TOPOLOGY,
             "The id %s was never used",
             idRef.first.c_str());
    PABORTIF(topology.sharingStrategyIndex.contains(idRef.first) &&
               idRef.second > 1,
             PERR_TOPOLOGY,
             "The sharing strategy %s was used more than once",
             idRef.first.c_str());
  }

  logTopologySummary(topology);

  return topology;
}

void
logTopologySummary(const TopologyDesc& desc)
{
  LOG0("=== Topology Summary ===");
  LOG0("Working Strategy  : %s", desc.workingStrategy.name.c_str());
  LOG0("Solver refs       : %zu", desc.workingStrategy.solverIds.size());
  LOG0("Database Templates: %zu", desc.databaseTemplates.size());
  LOG0("CDCL Solvers      : %zu", desc.cdclSolvers.size());
  LOG0("Local Searchers   : %zu", desc.localSearchers.size());
  LOG0("Sharing Strategies: %zu", desc.sharingStrategies.size());
  LOG0("Sharers           : %zu", desc.sharers.size());
  LOG0("ID refs           : %zu", desc.idRefs.size());

  // Solver breakdown by type
  std::unordered_map<std::string, size_t> solverTypeCounts;
  for (const auto& solver : desc.cdclSolvers) {
    solverTypeCounts[solver.name]++;
  }
  for (const auto& [type, count] : solverTypeCounts) {
    LOG1("\tCDCL solver type '%s': %zu", type.c_str(), count);
  }

  std::unordered_map<std::string, size_t> lsTypeCounts;
  for (const auto& ls : desc.localSearchers) {
    lsTypeCounts[ls.name]++;
  }
  for (const auto& [type, count] : lsTypeCounts) {
    LOG1("\tLocalSearch type '%s': %zu", type.c_str(), count);
  }

  // Sharing strategy details
  size_t totalProducers = 0, totalClients = 0;
  std::unordered_map<std::string, size_t> strategyNameCounts;
  for (const auto& ss : desc.sharingStrategies) {
    strategyNameCounts[ss.name]++;
    totalProducers += ss.producerIds.size();
    totalClients += ss.clientIds.size();
  }
  LOG0("Total producer links: %zu", totalProducers);
  LOG0("Total client links  : %zu", totalClients);
  for (const auto& [name, count] : strategyNameCounts) {
    LOG1("\tStrategy \"%s\": %zu instance(s)", name.c_str(), count);
  }

  // Sharer strategy fan-out
  size_t totalSharerStrategies = 0;
  size_t maxStratsPerSharer = 0;
  for (const auto& sharer : desc.sharers) {
    totalSharerStrategies += sharer.strategyIds.size();
    maxStratsPerSharer =
      std::max(maxStratsPerSharer, sharer.strategyIds.size());
  }
  if (!desc.sharers.empty()) {
    LOG0("Avg strategies/sharer: %.1f, max: %zu",
         static_cast<double>(totalSharerStrategies) / desc.sharers.size(),
         maxStratsPerSharer);
  }

  LOG0("========================");
}

} // namespace topology
