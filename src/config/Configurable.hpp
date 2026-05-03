#pragma once

#include "utils/Logger.hpp"
#include <atomic>
#include <string>

/**
 * @brief Parent for objects whose parameters are set once at construction
 * time, then frozen.
 * @ingroup topology
 *
 * Lifecycle: `configure(key, value)` ... `configure(key, value)` ->
 * `markConfigured()` -> object is read-only thereafter.
 *
 * `configure` is the public entry point and dispatches to the typed
 * protected ::setOption overloads (int / double / string). Derived classes
 * override only the overloads they support; the default implementations
 * abort with PERR_ARGS. Backend-specific implementations (MiniSat, Glucose,
 * MapleCOMSPS, HordeSat, Simple, the local searchers, ...) likewise abort
 * with PERR_ARGS in their `else` branch on an unknown key, so typos in
 * `params` are surfaced loudly rather than silently dropped.
 *
 * After `markConfigured()` any further `configure` call aborts with
 * PERR_BAD_BEHAVIOR. Long-lived consumers can guard their hot path with
 * `requireConfigured()` to fail fast on misuse. The configured state is
 * stored in an `std::atomic<bool>` so the freeze is visible across
 * threads.
 *
 * Subclasses can hook ::onConfigured to perform any work that depends on
 * having all parameters in hand, typically late initialization of internal
 * data structures (HordeSatSharing parses its `producer-ids` list there
 * to size its per-producer buffers, for example). Returning `false`
 * aborts the freeze with PERR_ARGS.
 *
 * This is the contract the topology builder relies on (see @ref topologies):
 * each freshly constructed solver / database / sharing / working strategy
 * has its `params` applied via `configure`, then is frozen with
 * `markConfigured()` before being wired into the graph.
 */
class Configurable
{
public:
  virtual ~Configurable() = default;

  template<typename T>
  void configure(const std::string& key, T value)
  {
    PABORTIF(m_configured,
             PERR_BAD_BEHAVIOR,
             "Reconfiguration after initialization is not allowed");
    setOption(key, std::move(value));
  }

  void markConfigured()
  {
    if (!onConfigured())
      PABORT(PERR_ARGS, "Configuration validation failed");
    m_configured.store(true);
  }

  bool isConfigured() const { return m_configured.load(); }

  void requireConfigured() const
  {
    PABORTIF(
      !isConfigured(), PERR_ARGS, "Object must be configured before use");
  }

protected:
  Configurable() = default;

  virtual void setOption(const std::string& key, int value)
  {
    PABORT(PERR_ARGS, "Int options not supported");
  }

  virtual void setOption(const std::string& key, double value)
  {
    PABORT(PERR_ARGS, "Double options not supported");
  }

  virtual void setOption(const std::string& key, const std::string& value)
  {
    PABORT(PERR_ARGS, "String options not supported");
  }

  virtual bool onConfigured() { return true; }

private:
  std::atomic<bool> m_configured{ false };
};