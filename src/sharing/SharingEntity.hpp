#pragma once

#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>

/**
 * @defgroup sharing Sharing
 * @brief Different Classes for Sharing clauses between solvers and other type of entities
 * @{
 */

/**
 * @brief A base class representing entities that can exchange clauses between themselves.
 *
 * @warning This class assumes all SharingEntity objects are managed by std::shared_ptr.
 * Improper use of raw pointers or other smart pointer types may lead to undefined behavior.
 * 
 * @todo Test userspace RCU for m_clients list
 * @todo shared_from_this is needed here ? Or only when we deal with producers in a SharingStrategy ?
 */
class SharingEntity : public std::enable_shared_from_this<SharingEntity>
{
  public:
	/**
	 * @brief Construct a new SharingEntity object.
	 *
	 * Automatically assigns a unique sharing ID to the entity.
	 *
	 * @warning This constructor should only be called through std::make_shared
	 * or a similar mechanism that ensures the object is owned by a shared_ptr.
	 */
	SharingEntity()
		: m_sharingId(s_currentSharingId.fetch_add(1))
		, m_clients(0)
	{
		LOGDEBUG1("I am sharing entity %d", m_sharingId);
	}

	/**
	 * @brief Construct a new SharingEntity object.
	 *
	 * Automatically assigns a unique sharing ID to the entity.
	 *
	 * @warning This constructor should only be called through std::make_shared
	 * or a similar mechanism that ensures the object is owned by a shared_ptr.
	 */
	SharingEntity(const std::vector<std::shared_ptr<SharingEntity>>& clients)
		: m_sharingId(s_currentSharingId.fetch_add(1))
		, m_clients(clients.begin(), clients.end())
	{
		LOGDEBUG1("I am sharing entity %d", m_sharingId);
	}

	/**
	 * @brief Destroy the SharingEntity object.
	 */
	virtual ~SharingEntity() {}

	/**
	 * @brief Import a single clause to this sharing entity.
	 * @param clause The clause to add.
	 * @return true if clause was imported, false otherwise.
	 *
	 * @warning This method may be called concurrently from multiple threads.
	 * Derived classes should ensure thread-safety in their implementation.
	 */
	virtual bool importClause(const ClauseExchangePtr& clause) = 0;

	/**
	 * @brief Import multiple clauses to this sharing entity.
	 * @param v_clauses The vector with the clauses to add.
	 *
	 * @warning This method may be called concurrently from multiple threads.
	 * Derived classes should ensure thread-safety in their implementation.
	 */
	virtual void importClauses(const std::vector<ClauseExchangePtr>& v_clauses) = 0;

	/**
	 * @brief Get the sharing ID of this entity.
	 * @return The sharing ID.
	 */
	int getSharingId() const { return this->m_sharingId; }

	/**
	 * @brief Set the sharing ID of this entity.
	 * @param _id The new sharing ID.
	 */
	void setSharingId(int _id) { this->m_sharingId = _id; }

	/**
	 * @brief Add a client to this entity.
	 * @param client shared pointer to the client SharingEntity to add.
	 *
	 */
	virtual void addClient(std::shared_ptr<SharingEntity> client)
	{
		std::unique_lock<std::shared_mutex> lock(m_clientsMutex);
		LOGDEBUG3("Sharing Entity %d: new client %p (counts: %d)", m_sharingId, client.get(), client.use_count());
		m_clients.push_back(client);
	}


    /**
     * @brief Remove a specific client from this entity.
     * @param client shared pointer to the client SharingEntity to remove.
     *
     * This method is thread-safe and can be called concurrently.
     */
    virtual void removeClient(std::shared_ptr<SharingEntity> client)
    {
        std::unique_lock<std::shared_mutex> lock(m_clientsMutex);
        auto initialSize = m_clients.size();
        m_clients.erase(
            std::remove_if(m_clients.begin(),
                           m_clients.end(),
                           [&client](const std::weak_ptr<SharingEntity>& wp) { return wp.lock() == client; }),
            m_clients.end());
        if (m_clients.size() < initialSize) {
            LOGDEBUG3("Sharing Entity %d: removed client %p", m_sharingId, client.get());
        }
    }

	/**
     * @brief Get the current number of clients.
     * @return The number of clients currently registered with this entity.
     *
     * This method is thread-safe and can be called concurrently.
     * Note: This count includes expired weak pointers that haven't been cleaned up yet.
     */
    size_t getClientCount() const
    {
        std::shared_lock<std::shared_mutex> lock(m_clientsMutex);
        return m_clients.size();
    }

	/**
	 * @brief Remove all clients.
	 */
	void clearClients()
	{
		std::unique_lock<std::shared_mutex> lock(m_clientsMutex);
		m_clients.clear();
	}

  protected:
	/**
	 * @brief Export a clause to a specific client.
	 * @param clause The clause to export.
	 * @param client The client to export the clause to.
	 * @return true if the clause was exported to the client, false otherwise.
	 *
	 * @note This is a primitive method intended to be redefined by subclasses.
	 * It is used by the exportClauses method to handle the export of individual clauses.
	 *
	 * @warning This method is not thread-safe and cannot be called concurrently.
	 */
	virtual bool exportClauseToClient(const ClauseExchangePtr& clause, std::shared_ptr<SharingEntity> client)
	{
		return client->importClause(clause);
	}

	/**
	 * @brief Export a clause to all registered clients.
	 * @param clause The clause to export.
	 * @return true if the clause was exported to any client, false otherwise.
	 * 
	 * @note This method uses the exportClauseToClient primitive for each clause and client combination.
	 * Subclasses can customize the behavior of clause export by overriding the exportClauseToClient method.
	 */
	bool exportClause(const ClauseExchangePtr& clause)
	{
		std::shared_lock<std::shared_mutex> lock(m_clientsMutex);
		bool exported = false;
		for (const auto& client : m_clients) {
			if (auto sharedClient = client.lock()) {
				if (exportClauseToClient(clause, sharedClient))
					exported = true;
			}
		}
		return exported;
	}

	/**
	 * @brief Export multiple clauses to all registered clients.
	 * @param clauses A vector of clauses to export.
	 *
	 * @note This method uses the exportClauseToClient primitive for each clause and client combination.
	 * Subclasses can customize the behavior of clause export by overriding the exportClauseToClient method.
	 */
	void exportClauses(const std::vector<ClauseExchangePtr>& clauses)
	{
		std::shared_lock<std::shared_mutex> lock(m_clientsMutex);
		for (const auto& weakClient : m_clients) {
			if (auto client = weakClient.lock()) {
				for (const ClauseExchangePtr& clause : clauses) {
					exportClauseToClient(clause, client);
				}
			}
		}
	}

  private:
	/// The sharing ID of this entity.
	int m_sharingId;

	/// Static atomic counter for generating unique sharing IDs.
	inline static std::atomic<int> s_currentSharingId{ 0 };

  protected:
	/// List of weak pointers to client SharingEntities.
	std::vector<std::weak_ptr<SharingEntity>> m_clients;

	/// Mutex to protect access to m_clients
	mutable std::shared_mutex m_clientsMutex;
};

/**
 * @} // end of sharing group
 */
