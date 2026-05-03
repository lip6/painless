#pragma once

#include "containers/ClauseDatabase.hpp"

#include <memory>
#include <string>
#include <boost/json.hpp>

/**
 * @brief Factory class for creating clause database instances.
 *
 * Two construction paths coexist:
 *  - The **topology path** (preferred): the static
 *    ::createDatabase(const std::string&) overload looks up the backend by
 *    name. Construction-time `params` (e.g. `max-clause-size`) are applied
 *    by the topology builder via Configurable::setOption.
 *  - The **legacy parameters path**: the instance method
 *    ::createDatabase(char) selects a backend by single-character code and
 *    forwards constructor arguments captured by the factory itself.
 *
 * @note ::initialize is declared but not implemented. The factory is
 * configured exclusively through the constructor for now.
 */
class ClauseDatabaseFactory
{
public:
  ClauseDatabaseFactory(unsigned int maxClauseSize,
                        size_t maxCapacity,
                        int mallobMaxPartitioningLbd,
                        int mallobMaxFreeSize,
                        unsigned int bufferSize);


  /**
   * @brief [legacy] Create a database from a single-character type code.
   *
   * @param dbTypeChar Database type code:
   *        `'s'` - SingleBuffer, `'d'` - PerSize, `'e'` - BufferPerEntity,
   *        `'m'` - Mallob. Any other value logs a warning and returns a
   *        PerSize database.
   *
   * Constructor arguments (`max-clause-size`, capacity, etc.) come from the
   * factory's stored parameters; for per-instance overrides use the
   * topology path with `params` instead.
   */
  std::shared_ptr<ClauseDatabase> createDatabase(char dbTypeChar);

  /**
   * @brief Create a database from a backend name (topology path).
   * @ingroup topology
   *
   * Accepted names (case-insensitive): `singleBuffer`, `perSize`,
   * `bufferPerEntity`, `mallob`. Aborts with PERR_UNKNOWN_DATABASE on an
   * unknown name.
   *
   * The returned database is constructed with default parameters; the
   * topology builder applies the per-template `params` via
   * Configurable::setOption immediately afterwards.
   */
  static std::shared_ptr<ClauseDatabase> createDatabase(const std::string& name);

public:
  // Static configuration parameters
  unsigned int m_maxClauseSize;
  size_t m_maxCapacity;
  int m_mallobMaxPartitioningLbd;
  int m_mallobMaxFreeSize;
  unsigned int m_clauseBuffer;
};