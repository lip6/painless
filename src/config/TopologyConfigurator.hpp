#pragma once

#include "core/painless.hpp"
#include <boost/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @defgroup topology Topology Configuration
 * @brief JSON-driven description of the solver/sharing graph instantiated at
 * startup.
 *
 * A topology file declares the components Painless should spawn (CDCL solvers,
 * local searchers, clause databases, sharing strategies, sharers) and the
 * subscription edges between them. Parsing happens in two phases:
 *  1. ::topology::parseJsonTopology turns the JSON into a ::topology::TopologyDesc
 *     whose cross-references are still raw string ids.
 *  2. ::topology::buildPermanentWorkers resolves those ids and wires the
 *     concrete objects into the running ::PainlessImpl.
 *
 * See the @ref topologies page for the full schema, the available `type`/`name`
 * values and a worked example.
 * @{
 */

namespace topology {

/**
 * @brief Descriptor for a single entry in the topology JSON.
 *
 * Each `*Desc` struct mirrors one JSON object 1:1. Cross-references (database
 * ids, producer/client lists, working-strategy solver lists) are kept as raw
 * strings; the builder resolves them in a second pass once every entity exists.
 *
 * The free-form `params` object is passed verbatim to
 * ::Configurable::setOption, so the accepted keys depend on the concrete
 * backend chosen by `type`/`name`. See @ref topologies for the per-type list.
 */

/**
 * @brief Template for an importDB / sharing DB (entry of `databaseTemplates`).
 *
 * `name` is the database type ("persize", "bufferperentity", "singlebuffer",
 * "mallob"). One concrete ::ClauseDatabase instance is created from the
 * template per consuming entity (each CDCL solver and each sharing strategy
 * that references it gets its own clone).
 */
struct DatabaseDesc
{
  std::string id;             ///< Unique JSON id, referenced by other entities.
  std::string name;           ///< Database type (case-insensitive). See @ref topologies.
  size_t capacity;            ///< Reserved for future use; currently parsed but ignored.
  boost::json::object params; ///< Forwarded blindly to ClauseDatabase::setOption.
};

/**
 * @brief A CDCL solver instance (entry of `cdclSolvers`).
 *
 * `name` selects the backend ("kissat", "cadical", "lingeling", "minisat",
 * "glucosesyrup", "maplecomsps") and is matched case-insensitively. The solver
 * is given its own ClauseDatabase built from the template referenced by
 * `importDBId`.
 */
struct CdclSolverDesc
{
  std::string id;
  std::string name;
  std::string importDBId;     ///< Id of a ::DatabaseDesc, resolved by the builder.
  boost::json::object params; ///< Forwarded to the backend solver via setOption.
};

/**
 * @brief A local-search solver instance (entry of `localSearchers`).
 *
 * `name` is "yalsat" or "tassat". Local searchers do not own a clause
 * database; they only consume clauses through the working strategy.
 */
struct LocalSearchDesc
{
  std::string id;
  std::string name;
  boost::json::object params;
};

/**
 * @brief A sharing strategy instance (entry of `sharingStrategies`).
 *
 * `name` selects the strategy implementation ("hordesat", "simple"). The
 * strategy owns a fresh ClauseDatabase built from `dbId`. Producer and client
 * ids may point at CDCL solvers *or* at other sharing strategies, allowing
 * hierarchical sharing topologies (e.g. local strategies feeding a global
 * one). Each sharing-strategy id may appear at most once across all
 * `producers`/`clients` lists in the file.
 */
struct SharingStrategyDesc
{
  std::string id;
  std::string name;
  std::vector<std::string> producerIds; ///< Entities subscribed *to* (resolved by builder).
  std::vector<std::string> clientIds;   ///< Entities subscribed *from* (resolved by builder).
  std::string dbId;                     ///< Id of a ::DatabaseDesc, resolved by the builder.
  boost::json::object params;
};

/**
 * @brief A sharer thread (entry of `sharers`).
 *
 * Each sharer drives a non-empty list of sharing strategies on a dedicated
 * thread. Multiple sharers can run in parallel; bundling several strategies
 * into one sharer trades parallelism for fewer OS threads.
 */
struct SharerDesc
{
  std::string id;
  std::vector<std::string> strategyIds; ///< Ids of ::SharingStrategyDesc to drive.
};

/**
 * @brief The single working strategy that orchestrates the solvers
 * (`workingStrategy` object in the JSON).
 *
 * `name` is "PortfolioSimple" or "DivideAndConquer" (case-insensitive). The
 * `solvers` list selects which CDCL/LocalSearch instances participate, in
 * declaration order.
 */
struct WorkingStrategyDesc
{
  std::string name;
  std::vector<std::string> solverIds; ///< Ids of solver descriptors, resolved by builder.
  boost::json::object params;
};

/**
 * @brief Fully parsed topology, ready to be handed to ::buildPermanentWorkers.
 *
 * Each kind of entity is stored in a vector that preserves JSON declaration
 * order, plus a `name -> index` map for O(1) lookups during the resolve pass.
 * The declared order matters: it determines the solver ids assigned by the
 * builder, which makes runs reproducible for a given JSON input.
 */
struct TopologyDesc
{
  WorkingStrategyDesc workingStrategy;

  /**
   * @brief Reference counter for every declared id.
   *
   * Incremented every time an id is mentioned (in `producers`, `clients`,
   * `importDB`, `db`, `solvers`, `strategies`). Used by the parser to fail
   * early on dangling ids and to forbid using the same sharing strategy from
   * more than one place.
   */
  std::unordered_map<std::string, uint> idRefs;

  std::vector<DatabaseDesc> databaseTemplates;
  std::unordered_map<std::string, size_t> databaseTemplateIndex;

  std::vector<CdclSolverDesc> cdclSolvers;
  std::unordered_map<std::string, size_t> cdclSolverIndex;

  std::vector<LocalSearchDesc> localSearchers;
  std::unordered_map<std::string, size_t> localSearcherIndex;

  std::vector<SharingStrategyDesc> sharingStrategies;
  std::unordered_map<std::string, size_t> sharingStrategyIndex;

  std::vector<SharerDesc> sharers;
  std::unordered_map<std::string, size_t> sharerIndex;
};

/**
 * @brief Parse a topology JSON file from disk into a TopologyDesc.
 *
 * Validation performed up-front:
 *  - duplicate ids (across all entity kinds) abort with PERR_TOPOLOGY;
 *  - unknown ids referenced by `importDB`, `db`, `producers`, `clients`,
 *    `solvers` or `strategies` are detected during the dangling-id sweep at
 *    the end and abort with PERR_TOPOLOGY;
 *  - any declared id never referenced by another entity is rejected with
 *    PERR_TOPOLOGY;
 *  - a sharing strategy referenced more than once aborts as well.
 *
 * Concrete `type`/`name` values are *not* validated here: an invalid
 * "persize2" type will only be caught by the corresponding factory in
 * ::buildPermanentWorkers.
 */
TopologyDesc
parseJsonTopology(const std::string& jsonPath);

/**
 * @brief Instantiate every entity declared by @p topology and wire them into
 * @p painless (solvers, sharing strategies, sharers, working strategy).
 *
 * The build proceeds in dependency order: databases (one clone per consumer)
 * -> CDCL solvers -> local searchers -> sharing strategies -> producer/client
 * edges (a second pass, so strategies can subscribe to each other) -> working
 * strategy -> sharers. For each entity, `Configurable::configure` is called
 * for every key in `params`, then `markConfigured()` freezes it.
 *
 * Iteration is insertion-ordered, so solver ids assigned here are
 * deterministic across runs for a given JSON file.
 */
void
buildPermanentWorkers(const TopologyDesc& topology, PainlessImpl& painless);

/**
 * @brief Emit a human-readable summary of the parsed topology to the log
 * (entity counts, per-type breakdowns, fan-out averages). Called automatically
 * at the end of ::parseJsonTopology.
 */
void
logTopologySummary(const TopologyDesc& desc);

} // namespace topology

/** @} */ // end of topology group
