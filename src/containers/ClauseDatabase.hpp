#pragma once

#include "containers/ClauseExchange.hpp"

#include <atomic>
#include <memory>
#include <numeric>
#include <sstream>
#include <vector>

/**
 * @defgroup pl_containers_db Clause Databases
 * @brief Different ClauseDatabase implementations
 * @ingroup pl_containers
 * @{
*/

/**
 * @class ClauseDatabase
 * @brief Abstract base class defining the interface for clause storage and management.
 * 
 * This class provides a common interface for different implementations of clause databases.
 * It allows for adding, retrieving, and managing clauses using a specific logic.
 * 
 */
class ClauseDatabase
{
  public:
	/// @brief Default Constructor.
	ClauseDatabase() {}

	/// @brief Virtual destructor to ensure proper cleanup of derived classes.
	virtual ~ClauseDatabase() {}

	/**
	 * @brief Add a clause to the database.
	 * @param clause Shared pointer to the clause to be added.
	 * @return true if the clause was successfully added, false otherwise.
	 */
	virtual bool addClause(ClauseExchangePtr clause) = 0;

	/**
	 * @brief Fill the given buffer with a selection of clauses.
	 * @param selectedCls Vector to be filled with selected clauses.
	 * @param literalCountLimit The maximum number of literals to be selected.
	 * @return The number of literals in the selected clauses.
	 */
	virtual size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit ) = 0;

	/**
	 * @brief Retrieve all clauses from the database.
	 * @param v_cls Vector to be filled with all clauses in the database.
	 */
	virtual void getClauses(std::vector<ClauseExchangePtr>& v_cls) = 0;

	/**
	 * @brief Retrieve a single clause from the database.
	 * @param cls Reference to a shared pointer where the selected clause will be stored.
	 * @return true if a clause was retrieved, false if the database is empty.
	 */
	virtual bool getOneClause(ClauseExchangePtr& cls) = 0;

	/**
	 * @brief Get the current number of clauses in the database.
	 * @return The number of clauses currently stored in the database.
	 */
	virtual size_t getSize() const = 0;

	/**
	 * @brief Reduce the size of the database by removing some clauses.
	 * @return The number of literals removed from the database.
	 */
	virtual size_t shrinkDatabase() = 0;

	/**
	 * @brief Remove all clauses from the database.
	 */
	virtual void clearDatabase() = 0;
};

/**
 * @}
 */