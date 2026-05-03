/**
 * @defgroup painlessapi Painless API
 * @brief The primary interface for interacting with the Painless framework,
 * including both synchronous and asynchronous solving capabilities. The
 interface follows
 * partially the IPASIR standard with additional extensions for parallel and
 distributed solving.
 *
 * @see https://github.com/biotomas/ipasir (IPASIR specification)
 * @{
 * @file solver.hpp
 * @brief Main API interface for the Painless SAT solver framework
 *
 * @ingroup painlessapi

 */

#ifndef __painless_hpp_INCLUDED
#define __painless_hpp_INCLUDED

#include "types.hpp"

// ==================================
// 		  DLL Export/Import Macros
// ==================================

#if defined(IPASIR_SHARED_LIB)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(BUILDING_IPASIR_SHARED_LIB)
#if defined(__GNUC__)
/**
 * @brief DLL export attribute for GCC on Windows
 */
#define PAINLESS_API __attribute__((dllexport))
#elif defined(_MSC_VER)
/**
 * @brief DLL export attribute for MSVC on Windows
 */
#define PAINLESS_API __declspec(dllexport)
#endif
#else
#if defined(__GNUC__)
/**
 * @brief DLL import attribute for GCC on Windows
 */
#define PAINLESS_API __attribute__((dllimport))
#elif defined(_MSC_VER)
/**
 * @brief DLL import attribute for MSVC on Windows
 */
#define PAINLESS_API __declspec(dllimport)
#endif
#endif
#elif defined(__GNUC__)
/**
 * @brief Visibility attribute for GCC on Unix-like systems
 */
#define PAINLESS_API __attribute__((visibility("default")))
#endif

#if !defined(PAINLESS_API)
#if !defined(IPASIR_SUPPRESS_WARNINGS)
#warning                                                                       \
  "Unknown compiler. Not adding visibility information to IPASIR symbols."
#warning "Define IPASIR_SUPPRESS_WARNINGS to suppress this warning."
#endif
/**
 * @brief Fallback when compiler is unknown
 */
#define PAINLESS_API
#endif
#else
/**
 * @brief Empty macro when not building as shared library
 */
#define PAINLESS_API
#endif

// ==================================
// 		  Painless Interface Class
// ==================================

/**
 * @class Painless
 * @brief Abstract interface for the Painless SAT solver framework
 *
 * This class defines the main API for interacting with Painless. It provides
 * both synchronous and asynchronous solving capabilities, along with utilities
 * for parsing CNF files.
 *
 * The interface is designed to support:
 * - Standard IPASIR (partially) operations (addLiteral, assume, solve)
 * - Asynchronous solving with callbacks
 * - Event-driven programming patterns
 *
 * @note This is an abstract base class. Use create_painless() to obtain an
 * instance.
 *
 * @see create_painless()
 * @see destroy_painless()
 */
class PAINLESS_API Painless
{
public:
  /**
   * @brief Virtual destructor
   *
   * Ensures proper cleanup of derived classes.
   */
  virtual ~Painless() = default;

  /**
   * @brief Get the solver signature
   *
   * Returns a string identifying the solver implementation, version,
   * and configuration. This is useful for logging and reproducibility.
   *
   * @return A string describing the solver configuration
   *
   * @note The format typically includes solver name, version, and strategy
   *       Example: "Painless 1.0.0 - PortfolioSimple with Kissat+CaDiCaL"
   */
  virtual std::string signature() = 0;

  // ==================================
  // 		  IPASIR Interface
  // ==================================

  /**
   * @brief Add a literal to the current clause
   *
   * Adds a literal to the clause being constructed. Call with lit=0 to finalize
   * the clause and add it to the formula. Literals are added incrementally, and
   * the clause is only complete when terminated with 0.
   *
   * @param lit The literal to add (0 to terminate the clause)
   *
   * @note Standard IPASIR behavior:
   *       - Positive integers represent positive literals
   *       - Negative integers represent negated literals
   *       - Zero terminates the current clause
   *
   * @par Example:
   * @code
   * solver->addLiteral(1);    // Add literal x1
   * solver->addLiteral(-2);   // Add literal ¬x2
   * solver->addLiteral(3);    // Add literal x3
   * solver->addLiteral(0);    // Finalize clause: (x1 ∨ ¬x2 ∨ x3)
   * @endcode
   */
  virtual void addLiteral(lit_t lit) = 0;

  /**
   * @brief Add an assumption literal for the next solve call
   *
   * Assumptions are literals that are temporarily assumed to be true for the
   * next solve() call. They are not permanently added to the formula and are
   * cleared after each solve attempt.
   *
   * @param lit The literal to assume as true
   *
   * @note Assumptions are useful for:
   *       - Incremental solving
   *       - Testing satisfiability under different conditions
   *
   * @par Example:
   * @code
   * solver->assume(5);        // Assume x5 is true
   * solver->assume(-7);       // Assume x7 is false
   * result_t result = solver->solve();  // Solve with these assumptions
   * @endcode
   */
  virtual void assume(lit_t lit) = 0;

  /**
   * @brief Solve the current SAT problem
   *
   * Attempts to find a satisfying assignment for the formula with any current
   * assumptions. This is a blocking call that returns when solving is complete.
   *
   * @return A result_t structure containing:
   *         - answer: SAT, UNSAT, TIMEOUT, or UNKNOWN
   *         - model: The satisfying assignment (if SAT)
   *
   * @note The model vector contains literals representing the assignment:
   *       - Positive literal: variable is true
   *       - Negative literal: variable is false
   *
   * @par Example:
   * @code
   * result_t result = solver->solve();
   * if (result.answer == SatAnswer::SAT) {
   *     std::cout << "Satisfiable!" << std::endl;
   *     for (lit_t lit : result.model) {
   *         std::cout << lit << " ";
   *     }
   * }
   * @endcode
   */
  virtual result_t solve() = 0;

  // Commented IPASIR methods (not yet implemented):
  // virtual lit_t valueOf(lit_t lit) = 0;
  // virtual bool failedDueTo(lit_t lit) = 0;
  // virtual void setTerminateCallback(void* data, int (*terminate)(void* data))
  // = 0; virtual void setLearner(void* data, int max_length, void
  // (*learn)(void* data, lit_t* clause)) = 0;

  // ==================================
  //    Asynchronous API Extensions
  // ==================================

  /**
   * @brief Start asynchronous solving
   *
   * Initiates the solving process in a non-blocking manner. The solver runs
   * in the background, and results can be retrieved using popResult() or
   * by waiting with waitOnResult().
   *
   * @note After calling this method:
   *       - Use waitOnResult() to check when a result is available
   * 		   - Use waitOnEnd() to check when the solver ends according to
   * the interruption callback
   *       - Use popResult() to retrieve results
   *       - Use asyncEnd() to request termination to ask for interruption
   * asynchronously
   */
  virtual void asyncSolve() = 0;

  /**
   * @brief Request solver to end asynchronously
   *
   * Signals the whole solver to terminate gracefully. The solver may not stop
   * immediately but will terminate as soon as it reaches a safe checkpoint.
   *
   * @note This is a non-blocking request. Use waitOnEnd() to ensure
   *       the solver has actually stopped.
   */
  virtual void asyncEnd() = 0;

  /**
   * @brief Wait indefinitely for asynchronous solving to complete
   *
   * Blocks the calling thread until the asynchronous solving process
   * has finished.
   *
   * @note This does not retrieve the result; use popResult() for that.
   */
  virtual void waitOnEnd() = 0;

  /**
   * @brief Wait for asynchronous solving to complete with timeout
   *
   * Blocks the calling thread for at most the specified duration or until
   * the solving process completes, whichever comes first.
   *
   * @param microseconds Maximum time to wait in microseconds
   * @return true if the solver ended, false otherwise (timeout reached)
   */
  virtual bool waitOnEndFor(uint64_t microseconds) = 0;

  /**
   * @brief Retrieve a result from the asynchronous solver
   *
   * Attempts to pop a result from the internal result queue. If multiple
   * results are available (e.g., from solution enumeration), they can be
   * retrieved sequentially.
   *
   * @param[out] result The result structure to populate
   * @return true if a result was retrieved, false if the queue is empty
   *
   * @note This is a non-blocking operation. The result parameter is only
   *       modified if the function returns true.
   *
   * @par Example:
   * @code
   * solver->asyncSolve();
   * solver->waitOnResult();
   * result_t result;
   * if (solver->popResult(result)) {
   *     // Process result
   * }
   * @endcode
   */
  virtual bool popResult(result_t& result) = 0;

  /**
   * @brief Wait indefinitely for a result to become available
   *
   * Blocks until at least one result is ready to be retrieved via popResult().
   *
   * @return true if a result is available, false if solving ended with no
   * results
   *
   * @note This does not retrieve the result; it only waits for availability.
   *       Call popResult() after this returns true.
   */
  virtual bool waitOnResult() = 0;

  /**
   * @brief Wait for a result with timeout
   *
   * Blocks for at most the specified duration or until a result becomes
   * available, whichever comes first.
   *
   * @param microseconds Maximum time to wait in microseconds
   * @return true if a result is available, false if timeout occurred
   *
   * @note After timeout, you can:
   *       - Try waiting again
   *       - Call asyncEnd() to terminate
   *       - Continue with other operations
   */
  virtual bool waitOnResultFor(uint64_t microseconds) = 0;

  // ==================================
  // 		  Utility Methods
  // ==================================

  /**
   * @brief Parse a CNF file in DIMACS format
   *
   * Reads and parses a CNF file, and adds the literals into the solver.
   *
   * @param[in]  filename  Path to the CNF file to parse
   * @return true if parsing succeeded, false otherwise
   *
   * @note The the read file must follow DIMACS format:
   *       - Literals from each clause are added sequentially
   *       - Each clause is terminated with a 0
   *       - Example: [1, -2, 0, 3, 4, 0] represents (x1 ∨ ¬x2) ∧ (x3 ∨ x4)
   *
   * @par Example:
   * @code
   *
   * if (solver->parseCNF("formula.cnf")) {
   *     solver->solve();
   * }
   * @endcode
   */
  virtual bool loadDIMACS(const std::string& filename) = 0;

  virtual bool set(const std::string& key, int value) = 0;
  virtual bool set(const std::string& key, unsigned value) = 0;
  virtual bool set(const std::string& key, long value) = 0;
  virtual bool set(const std::string& key, unsigned long value) = 0;
  virtual bool set(const std::string& key, float value) = 0;
  virtual bool set(const std::string& key, double value) = 0;
  virtual bool set(const std::string& key, bool value) = 0;
  // virtual bool set(const std::string& key, const std::string& value) = 0;
  virtual bool set(const std::string& key, const char* value) = 0;
};

// ==================================
// 		  C API for Dynamic Loading
// ==================================

/**
 * @brief C interface for creating and destroying Painless instances
 *
 * These functions provide a C-compatible interface for dynamic library loading.
 * The names are not mangled, allowing them to be loaded via dlopen/dlsym on
 * Unix or LoadLibrary/GetProcAddress on Windows.
 */
extern "C"
{
  /**
   * @brief Factory function to create a virgin Painless solver instance
   *
   * Creates an unconfigured instance of the Painless solver
   * 
   * @return Pointer to a new Painless instance, or nullptr on failure
   *
   * @note The caller is responsible for destroying the instance using
   * destroy_painless()
   *
   * @see destroy_painless()
   */
  PAINLESS_API Painless* create_empty_painless();

  /**
   * @brief Factory function to create an initialized Painless solver instance configured
   * with a json file
   *
   * Creates a new instance of the Painless solver with the specified
   * configuration.
   *
   * @param jsonPath     Path to Configuration in JSON format (optional)
   *                 Can specify solver parameters, sharing strategies,
   * timeouts, etc.
   * @param isQuiet  Whether to suppress log output (1 = quiet (default), 0 =
   * verbose)
   * @return Pointer to a new Painless instance, or nullptr on failure
   *
   * @note The caller is responsible for destroying the instance using
   * destroy_painless()
   *
   * @see destroy_painless()
   */
  PAINLESS_API Painless* create_painless(const char* jsonPath, int isQuiet = 1);

  /**
   * @brief Destroy a Painless solver instance
   *
   * Properly cleans up and deallocates a Painless instance created with
   * create_painless(). This ensures all worker threads are stopped and
   * resources are freed.
   *
   * @param solver Pointer to the Painless instance to destroy
   *
   * @note After calling this function, the solver pointer becomes invalid
   *       and should not be used.
   *
   * @see create_painless()
   */
  PAINLESS_API void destroy_painless(Painless* solver);
}

/**
 * @brief Documentation for potential C wrapper functions
 *
 * These wrapper functions could be implemented to provide a more complete C
 * API:
 *
 * @code
 * // popModel C Wrapper:
 * // Retrieve model into a pre-allocated array
 * bool popModel_c(Painless* solver, lit_t* lits, size_t size) {
 *     model_t model;
 *     if (!solver->popModel(model)) return false;
 *
 *     if (model.size() > size) {
 *         // Error: buffer too small
 *         return false;
 *     }
 *
 *     for (size_t i = 0; i < model.size(); i++) {
 *         lits[i] = model[i];
 *     }
 *     return true;
 * }
 *
 * // parseCNF C Wrapper:
 * // Parse CNF and allocate memory for literals
 * bool parseCNF_c(Painless* solver, const char* filename,
 *                 lit_t** cliterals, size_t* lit_count,
 *                 var_t* varCount, csize_t* clsCount) {
 *     std::vector<lit_t> literals;
 *
 *     if (!solver->parseCNF(filename, literals, *varCount, *clsCount)) {
 *         return false;
 *     }
 *
 *     // Allocate memory for C caller
 *     *cliterals = new lit_t[literals.size()];
 *     *lit_count = literals.size();
 *     std::copy(literals.begin(), literals.end(), *cliterals);
 *
 *     return true;
 * }
 *
 * // Note: C callers must free the allocated memory:
 * // delete[] cliterals;
 * @endcode
 */
/**
 * @} // end of solving group
 */
#endif