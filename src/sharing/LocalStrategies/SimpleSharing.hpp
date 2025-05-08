#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStrategy.hpp"

#include <vector>

/**
 * @brief This strategy is a simple sharing strategy using size to filter out long clauses.
 * @ingroup local_sharing
 */
class SimpleSharing : public SharingStrategy
{
  public:
	/**
	 * @brief Constructor for SimpleSharing.
	 * @param clauseDB Shared pointer to the clause database.
	 * @param producers Vector of shared pointers to producer entities.
	 * @param consumers Vector of shared pointers to consumer entities.
	 * @param sizeLimitAtImport The maximum size of allowed clauses at import.
	 * @param literalsPerRoundPerProducer  The number of literals each producer should export at each round.
	 */
	SimpleSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
				  unsigned int  sizeLimitAtImport,
				  unsigned long literalsPerRoundPerProducer,
				  const std::vector<std::shared_ptr<SharingEntity>>& producers = {},
				  const std::vector<std::shared_ptr<SharingEntity>>& consumers = {});

	/**
	 * @brief Destructor for SimpleSharing.
	 */
	~SimpleSharing();

	// SharingEntity Interface
	// =======================
	/**
	 * @brief Imports a single clause. Clauses are tested against sizeLimit before
	 * @param clause Pointer to the clause to be imported.
	 * @return True if the clause was successfully imported, false otherwise.
	 */
	bool importClause(const ClauseExchangePtr& clause) override;

	/**
	 * @brief Imports multiple clauses.
	 * @param v_clauses Vector of clause pointers to be imported.
	 */
	void importClauses(const std::vector<ClauseExchangePtr>& v_clauses) override
	{
		for (auto clause : v_clauses)
			importClause(clause);
	}

	// SharingStrategy Interface
	// =========================

	/**
	 * @brief Performs the sharing operation.
	 * @return True if sharing is complete or should be terminated, false otherwise.
	 */
	bool doSharing() override;

  protected:
	/// Number of shared literals per round.
	int literalPerRound;

	/// Used to manipulate clauses (as a member to reduce number of allocations).
	std::vector<ClauseExchangePtr> selection;

	/// Maximum clause size
	unsigned int sizeLimit;
};