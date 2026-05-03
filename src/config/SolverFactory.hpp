#pragma once

#include "config/ClauseDatabaseFactory.hpp"
#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include <atomic>
#include <vector>

#include "core/painless.hpp"

/**
 * @brief Factory class for creating and managing SAT solvers.
 * @ingroup solving
 *
 * Two construction paths coexist:
 *  - The **topology path** (preferred), driven by the static
 *    ::createCDCLSolver / ::createLocalSearcher overloads that take a
 *    string name. Used by the topology builder (see @ref topologies).
 *  - The **legacy parameters path** driven by ::createSolver(char, ...) /
 *    ::createSolvers(...), which use single-character type codes and a
 *    portfolio string from the CLI parameters.
 */
class SolverFactory
{
public:
  SolverFactory(const Parameters& parameters);

  /**
   * @brief [legacy] Creates a solver from a single-character type code.
   *
   * Accepted codes (subject to compile-time backend availability):
   *  - `'g'` GlucoseSyrup, `'l'` Lingeling, `'M'` MapleCOMSPS,
   *  - `'m'` MiniSat, `'k'` Kissat, `'I'` KissatINC, `'K'` KissatMAB,
   *  - `'c'` CaDiCaL, `'y'` YalSAT, `'t'` TaSSAT.
   *
   * Aborts with PERR_UNKNOWN_SOLVER on an unknown code. Returns `nullptr`
   * (without aborting) once the per-process CPU budget is reached.
   *
   * @param type Single-character solver type code.
   * @param importDB ClauseDatabase the solver will pull imported clauses from.
   * @param fullReader Callback used by the solver to load the formula.
   * @return Created solver, or `nullptr` if the CPU budget is exhausted.
   */
  std::shared_ptr<SolverInterface> createSolver(
    const char type,
    std::shared_ptr<ClauseDatabase> importDB,
    FullClauseReader& fullReader);

  /**
   * @brief Create a CDCL solver from a backend name (topology path).
   * @ingroup topology
   *
   * Accepted names (case-insensitive, subject to compile-time availability):
   * `kissat`, `cadical`, `lingeling`, `minisat`, `glucosesyrup`,
   * `maplecomsps`. Aborts with PERR_UNKNOWN_SOLVER on an unknown or empty
   * name.
   *
   * @param id Internal solver id assigned by the topology builder
   *           (insertion order in `cdclSolvers`).
   */
  static std::shared_ptr<SolverCDCLInterface> createCDCLSolver(
    uint id,
    const std::string& name,
    std::shared_ptr<ClauseDatabase> importDB,
    FullClauseReader& fullReader);

  /**
   * @brief Create a local-search solver from a backend name (topology path).
   * @ingroup topology
   *
   * Accepted names (case-insensitive, subject to compile-time availability):
   * `yalsat`, `tassat`. Aborts with PERR_UNKNOWN_SOLVER on an empty name;
   * an unrecognized name silently returns `nullptr`.
   */
  static std::shared_ptr<LocalSearchInterface> createLocalSearcher(
    uint id,
    const std::string& name,
    FullClauseReader& fullReader);

  /**
   * @brief [legacy] Creates multiple solvers based on a portfolio string.
   * Each solver gets its own freshly-instantiated importDB.
   *
   * @param count Maximum number of solvers to create.
   * @param importDBType Single-character database type code:
   *        `'s'` SingleBuffer, `'d'` PerSize, `'e'` PerEntity, `'m'` Mallob.
   * @param portfolio String of solver-type codes (see ::createSolver) cycled
   *        through round-robin to fill `count` slots.
   * @param fullReader Callback the solvers use to load the formula.
   * @param solvers Output vector receiving the created solvers.
   */
  void createSolvers(int count,
                     char importDBType,
                     const std::string& portfolio,
                     FullClauseReader& fullReader,
                     std::vector<std::shared_ptr<SolverInterface>>& solvers);

public:
  /**
   * @brief Atomic counter for assigning unique IDs to solvers.
   */
  std::atomic<int> m_currentIdSolver;
  ClauseDatabaseFactory m_clauseDBFactory;
  Parameters m_parameters;
};