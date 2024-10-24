#include "sharing/GlobalStrategies/MallobSharing.hpp"
#include <cmath>
#include <stdexcept>

void
MallobSharing::initializeFilter(unsigned int resharingPeriod,
								unsigned int sharingsPerSecond,
								unsigned int maxProducerId)
{
	// Ensure the filter supports a maximum of 64 producers
	if (maxProducerId > 63) {
		throw std::invalid_argument("ExactFilter doesn't support more than 64 producers (maxId = 63)");
	}
	if (sharingsPerSecond == 0) {
		throw std::invalid_argument("Sharings per second must be greater than zero");
	}

	m_maxProducerId = maxProducerId;
	float epochDurationMicroS = 1000000.0f / sharingsPerSecond;
	m_resharingPeriodInEpochs = std::ceil(resharingPeriod / epochDurationMicroS);
	m_currentEpoch = 1; // Start at 1 to ensure all clauses aren't initially flagged as shared
	m_sharingPerSecond = sharingsPerSecond;
}

bool
MallobSharing::doesClauseExist(const ClauseExchangePtr& cls) const
{
	return m_clauseMetaMap.find(cls) != m_clauseMetaMap.end();
}

void
MallobSharing::updateClause(const ClauseExchangePtr& cls)
{
	ClauseMeta& currentMeta = m_clauseMetaMap.at(cls);
	currentMeta.sources |= 1ULL << cls->from;
	currentMeta.productionEpoch = m_currentEpoch;
}

bool
MallobSharing::insertClause(const ClauseExchangePtr& cls)
{
	if (!doesClauseExist(cls)) {
		ClauseMeta newClauseMeta = { .productionEpoch = m_currentEpoch,
									 .sharedEpoch = -m_resharingPeriodInEpochs,
									 .sources = 1ULL << cls->from };
		m_clauseMetaMap.emplace(cls, newClauseMeta);
		return true;
	} else {
		updateClause(cls);
		return true;
	}
}

bool
MallobSharing::isClauseShared(const ClauseExchangePtr& cls) const
{
	auto it = m_clauseMetaMap.find(cls);
	if (it != m_clauseMetaMap.end()) {
		const ClauseMeta& currentMeta = it->second;
		return m_currentEpoch - currentMeta.sharedEpoch <= m_resharingPeriodInEpochs;
	}
	return false;
}

bool
MallobSharing::canConsumerImportClause(const ClauseExchangePtr& cls, unsigned consumerId)
{
	// Ensure consumer ID is within supported range
	assert(consumerId <= 63 && "Do not support more than 64 producer ids!");

	auto it = m_clauseMetaMap.find(cls);
	return it == m_clauseMetaMap.end() || !(it->second.sources & (1ULL << consumerId));
}

void
MallobSharing::markClauseAsShared(ClauseExchangePtr& cls)
{
	if (doesClauseExist(cls)) {
		ClauseMeta& currentMeta = m_clauseMetaMap.at(cls);
		currentMeta.sharedEpoch = m_currentEpoch;
		currentMeta.sources = 0; // Reset sources to allow all solvers to import it after periodEpoch
	}
}

void
MallobSharing::incrementEpoch()
{
	++m_currentEpoch;
}

size_t
MallobSharing::shrinkFilter()
{
	if (m_resharingPeriodInEpochs <= 0)
		return 0;
	size_t removedEntries = 0;
	auto it = m_clauseMetaMap.begin();
	while (it != m_clauseMetaMap.end()) {
		const ClauseMeta& currentMeta = it->second;
		if (m_currentEpoch - currentMeta.sharedEpoch > m_resharingPeriodInEpochs &&
			m_currentEpoch - currentMeta.productionEpoch > m_resharingPeriodInEpochs) {
			it = m_clauseMetaMap.erase(it);
			removedEntries++;
		} else {
			++it;
		}
	}
	return removedEntries;
}