#pragma once

#include "SharingEntity.hpp"
#include "containers/ClauseDatabase.hpp"
#include "sharing/SharingStatistics.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>


/**
 * @brief SharingStrategy class, inheriting from SharingEntity.
 * 
 * SharingStrategy serves as a central manager that receives clauses from producers
 * and distributes them to consumers. It orchestrates the exchange of clauses
 * between different entities in a solver.
 * 
 * @ingroup sharing
 * @todo Private constructors (+ move and copy constructors), wrappers for constructors that calls connectProducers and returns a smart pointer
 * @todo Test userspace RCU for m_producers
 */
class SharingStrategy : public SharingEntity
{
  public:
	/**
	 * @brief Constructor for SharingStrategy. Be sure to call connectConstructorProducers after construction, this is
	 * due to the base class shared_from_this.
	 *
	 * @param producers A vector of shared pointers to SharingEntity objects that produce clauses.
	 * @param consumers A vector of shared pointers to SharingEntity objects that consume clauses.
	 * @param clauseDB A shared pointer to the ClauseDatabase where exported clauses are stored.
	 *
	 * This constructor initializes the SharingStrategy with the given producers, consumers, and
	 * clause database. It stores weak pointers to the producers and consumers internally since
	 * it doesn't own the object but just communicate with it if possible.
	 * 
	 * @warning Be sure to call connectConstructorProducers after construction, this is due to the base class shared_from_this
	 */
	SharingStrategy(const std::vector<std::shared_ptr<SharingEntity>>& producers,
					const std::vector<std::shared_ptr<SharingEntity>>& consumers,
					const std::shared_ptr<ClauseDatabase>& clauseDB)
		: SharingEntity(consumers)
		, m_producers(producers.begin(), producers.end())
		, m_clauseDB(clauseDB)
	{
		LOGDEBUG1("Be sure to call connectConstructorProducers after construction, this is due to the base class "
				"shared_from_this.");
	}

	/**
	 * @brief Virtual destructor.
	 */
	virtual ~SharingStrategy() {}

	/**
	 * @brief Executes the sharing process from producers to consumers.
	 * @return true if the sharer should end, false otherwise.
	 */
	virtual bool doSharing() = 0;

	/**
	 * @brief Determines the sleeping time for the sharer.
	 * @return Number of microseconds to sleep.
	 */
	virtual std::chrono::microseconds getSleepingTime() { return std::chrono::microseconds(__globalParameters__.sharingSleep); };

	/**
	 * @brief Prints the statistics of the strategy.
	 */
	virtual void printStats()
	{
		LOGSTAT("Strategy Basic Stats: receivedCls %d, sharedCls %d, filteredAtImport: %d",
				stats.receivedClauses.load(),
				stats.sharedClauses,
				stats.filteredAtImport.load());
	}

	/**
	 * @brief Add this to the producers' clients list
	 * @warning Be Careful! connect only constructor lists, (otherwise this strategy can be added twice)
	 */
	void connectConstructorProducers()
	{
		std::shared_lock<std::shared_mutex> lock(m_producersMutex);
		for (auto& weakProducer : m_producers) {
			if (auto producer = weakProducer.lock())
				producer->addClient(shared_from_this());
		}
	}

	/**
	 * @brief Adds a producer to this strategy.
	 * @param producer The producer SharingEntity to add.
	 */
	virtual void addProducer(std::shared_ptr<SharingEntity> producer)
	{
		std::unique_lock<std::shared_mutex> lock(m_producersMutex);
		m_producers.push_back(producer);
		LOGDEBUG2("[SharingStrategy] Added new producer");
	}

	/**
	 * @brief Connect a producer to this strategy. Add (this) to clients of (producer)
	 * @param producer The producer SharingEntity to connect to.
	 * @warning A producer should be connect only if it was added beforehand (necessary initialization done)
	 */
	virtual void connectProducer(std::shared_ptr<SharingEntity> producer)
	{
		producer->addClient(shared_from_this());
		LOGDEBUG2("[SharingStrategy] Producer %u is connected!", producer->getSharingId());
	}

	/**
	 * @brief Removes a producer from this strategy.
	 * @param producer The producer SharingEntity to remove.
	 */
	virtual void removeProducer(std::shared_ptr<SharingEntity> producer)
	{
		std::unique_lock<std::shared_mutex> lock(m_producersMutex);
		producer->removeClient(shared_from_this());
		m_producers.erase(
			std::remove_if(m_producers.begin(),
						   m_producers.end(),
						   [&producer](const std::weak_ptr<SharingEntity>& wp) { return wp.lock() == producer; }),
			m_producers.end());
		LOGDEBUG2("[SharingStrategy] Removed producer");
	}

  protected:
	/**
	 * @brief A SharingStrategy doesn't send a clause to the source client (->from must store the sharingId of its producer)
	 */
	bool exportClauseToClient(const ClauseExchangePtr& clause, std::shared_ptr<SharingEntity> client) override
	{
		if (clause->from != client->getSharingId())
			return client->importClause(clause);
		else
			return false;
	}

	/**
	 * @brief Clause database where exported clauses are stored.
	 */
	std::shared_ptr<ClauseDatabase> m_clauseDB;

	/// Sharing statistics.
	SharingStatistics stats;

	/* Producers Management */

	/// The set holding the references to the producers
	std::vector<std::weak_ptr<SharingEntity>> m_producers;

	/// Shared mutex for producers list (to become RCU ?)
	mutable std::shared_mutex m_producersMutex;
};