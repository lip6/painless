#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

/**
 * @class ClauseDatabaseBufferPerEntity
 * @brief A clause database that maintain separate buffers for each entity, mimicing old painless default behavior
 *
 * This class extends ClauseDatabase to provide a mechanism for storing and managing clauses in separate buffers for
 * different entities thanks to the from attributed in ClauseExchange. It uses fine-grained locking to allow concurrent
 * access where possible.
 *
 * Since ClauseBuffer is lockfree, all consumption operations (read) can be done concurently. The only synchronization
 * required is on the map of buffers
 * 
 * The maximum size parameter is only used at selection, in the temporary ClauseDatabasePerSize instance.
 *
 * @ingroup pl_containers_db
 */
class ClauseDatabaseBufferPerEntity : public ClauseDatabase
{
  public:
	/**
	 * @brief Default constructor deleted to enforce use of parameterized constructor.
	 */
	ClauseDatabaseBufferPerEntity() = delete;

	/**
	 * @brief Constructor with a specified maximum clause size.
	 * @param maxClauseSize The maximum size of clauses to be stored.
	 */
	explicit ClauseDatabaseBufferPerEntity(int maxClauseSize);

	/**
	 * @brief Virtual destructor.
	 */
	~ClauseDatabaseBufferPerEntity() override = default;

	/**
	 * @brief Adds a clause to the appropriate entity's buffer.
	 * @param clause The clause to be added.
	 * @return true if the clause was successfully added, false otherwise.
	 * @note This method is thread-safe and allows concurrent additions to different buffers.
	 */
	bool addClause(ClauseExchangePtr clause) override;

	/**
	 * @brief Selects clauses up to a specified total size.
	 * @param selectedCls Vector to store the selected clauses.
	 * @param literalCountLimit The maximum number of literals to be selected.
	 * @return The number of literals in the selected clauses.
	 * @note This method acquires a shared lock and can be called concurrently with other read operations.
	 */
	size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit) override;

	/**
	 * @brief Retrieves all clauses from all entity buffers.
	 * @param v_cls Vector to store the retrieved clauses.
	 * @note This method acquires a shared lock and can be called concurrently with other read operations.
	 */
	void getClauses(std::vector<ClauseExchangePtr>& v_cls) override;

	/**
	 * @brief Retrieves a single clause from any entity buffer.
	 * @param cls Reference to store the retrieved clause.
	 * @return true if a clause was retrieved, false if all buffers are empty.
	 * @note This method acquires a shared lock and can be called concurrently with other read operations.
	 */
	bool getOneClause(ClauseExchangePtr& cls) override;

	/**
	 * @brief Gets the total number of clauses across all entity buffers.
	 * @return The total number of clauses.
	 * @note This method acquires a shared lock and can be called concurrently with other read operations.
	 */
	size_t getSize() const override;

	/**
	 * @brief Clears all export buffers.
	 * @note This method acquires an exclusive lock and cannot be called concurrently with other operations.
	 */
	void clearDatabase() override;

	/**
	 * @brief Does nothing in this implementation.
	 * @return The maximum size_t value.
	 */
	size_t shrinkDatabase() override
	{
		LOGDEBUG3("This does nothing!");
		return (size_t)-1;
	};

  private:
	/**
	 * @brief Map of entity IDs to their corresponding clause buffers.
	 */
	std::unordered_map<int, std::unique_ptr<ClauseBuffer>> entityDatabases;

	/**
	 * @brief Mutex for protecting concurrent access to entityDatabases.
	 *
	 * This shared mutex allows multiple concurrent readers (shared lock)
	 * or a single writer (unique lock) to access the entityDatabases.
	 */
	mutable std::shared_mutex dbmutex;

	/**
	 * @brief The maximum clause size accepted in this clauseDatabase.
	 * @note If <= 0, there is no limit.
	 */
	unsigned int maxClauseSize;
};