#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include <algorithm>
#include <random>

/**
 * @class ClauseDatabaseSingleBuffer
 * @brief This is just a ClauseDatabase wrapper around a ClauseBuufer
 *
 * This class provides a simple implementation of the ClauseDatabase interface
 * using a single ClauseBuffer for clause storage and management.
 *
 * I wanted ClauseBuffer to not implement ClauseDatabase to make it easier to change(independent).
 *
 */
class ClauseDatabaseSingleBuffer : public ClauseDatabase
{
  private:
	/// @brief The single buffer of this database, it stores all the clauses
	ClauseBuffer buffer;

  public:
	/**
	 * @brief Default Constructor deleted to enforce parameterized constructor.
	 */
	ClauseDatabaseSingleBuffer() = delete;

	ClauseDatabaseSingleBuffer(size_t bufferSize)
		: buffer(bufferSize)
	{
	}

	/**
	 * @brief Virtual destructor.
	 */
	~ClauseDatabaseSingleBuffer() override = default;

	/**
	 * @brief Adds a clause to the database.
	 * @param clause The clause to be added.
	 * @return True if the clause was successfully added, false otherwise.
	 */
	bool addClause(ClauseExchangePtr clause) override { return buffer.addClause(std::move(clause)); }

	/**
	 * @brief Selects clauses up to the specified total size.
	 * @param selectedCls Vector to store the selected clauses.
	 * @param literalCountLimit The maximum number of literals to select.
	 * @return The number of literals in the selected clauses.
	 *
	 * @note This method fills the buffer without exceeding  by getting one clause at a time.
	 * @warning This method may not select exactly  literals if the last clause added exceeds the limit.
	 */
	size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit) override
	{
		size_t selectedLiterals = 0;
		ClauseExchangePtr clause;

		while (buffer.getClause(clause)) {
			if (selectedLiterals + clause->size <= literalCountLimit) {
				selectedCls.push_back(clause);
				selectedLiterals += clause->size;
			} else {
				// If this clause would exceed the limit, put it back and stop
				buffer.addClause(clause);
				break;
			}
		}

		return selectedLiterals;
	}

	/**
	 * @brief Retrieves all clauses from the database.
	 * @param v_cls Vector to store the retrieved clauses.
	 */
	void getClauses(std::vector<ClauseExchangePtr>& v_cls) override { buffer.getClauses(v_cls); }

	/**
	 * @brief Retrieves a single clause from the database.
	 * @param cls Shared pointer to store the retrieved clause.
	 * @return True if a clause was retrieved, false if the database is empty.
	 */
	bool getOneClause(ClauseExchangePtr& cls) override { return buffer.getClause(cls); }

	/**
	 * @brief Returns the current number of clauses in the database.
	 * @return The number of clauses in the database.
	 */
	size_t getSize() const override { return buffer.size(); }

	/**
	 * @brief Shrinks the database by clearing all clauses.
	 * @return The number max size_t number
	 *
	 * @warning This method clears the entire database. All clauses will be removed.
	 */
	/// Does nothing
	/// @return maximum size_t number
	size_t shrinkDatabase() override
	{
		LOGDEBUG3("This does nothing!");
		return (size_t)-1;
	};

	/**
	 * @brief Clears all clauses from the database.
	 */
	void clearDatabase() override { buffer.clear(); }
};