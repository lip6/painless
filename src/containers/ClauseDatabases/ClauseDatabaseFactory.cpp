#include "ClauseDatabaseFactory.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabases/ClauseDatabaseMallob.hpp"
#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"

unsigned int ClauseDatabaseFactory::s_maxClauseSize = __globalParameters__.maxClauseSize;
int ClauseDatabaseFactory::s_mallobMaxPartitioningLbd = 2;
size_t ClauseDatabaseFactory::s_maxCapacity = __globalParameters__.importDBCap;
int ClauseDatabaseFactory::s_mallobMaxFreeSize = 1;
bool ClauseDatabaseFactory::s_initialized = __globalParameters__.maxClauseSize;

void
ClauseDatabaseFactory::initialize(unsigned int maxClauseSize,
								  size_t maxCapacity,
								  int mallobMaxPartitioningLbd,
								  int mallobMaxFreeSize)
{
	s_maxClauseSize = maxClauseSize;
	s_mallobMaxPartitioningLbd = mallobMaxPartitioningLbd;
	s_maxCapacity = maxCapacity;
	s_mallobMaxFreeSize = mallobMaxFreeSize;
	s_initialized = true;

	LOG0("DB>> ClauseDatabaseFactory initialized: maxClauseSize=%u, mallobCapacity=%zu, mallobLbd=%d,"
		 "mallobFreeSize=%d",
		 maxClauseSize,
		 maxCapacity,
		 mallobMaxPartitioningLbd,
		 mallobMaxFreeSize);
}

std::shared_ptr<ClauseDatabase>
ClauseDatabaseFactory::createDatabase(char dbTypeChar)
{
	if (!s_initialized) {
		LOGERROR("ClauseDatabaseFactory not initialized. Using default values.");
		s_initialized = true;
	}

	switch (dbTypeChar) {
		case 's': {
			LOG0("DB>> Creating Single Buffer database with max clause capacity %u", s_maxCapacity);
			return std::make_shared<ClauseDatabaseSingleBuffer>(s_maxCapacity);
		}
		case 'd': {
			LOG0("DB>> Creating PerSize database with max clause size %u", s_maxClauseSize);
			return std::make_shared<ClauseDatabasePerSize>(s_maxClauseSize);
		}

		case 'e': {
			LOG0("DB>> Creating PerEntity database with max clause size %u", s_maxClauseSize);
			return std::make_shared<ClauseDatabaseBufferPerEntity>(s_maxClauseSize);
		}

		case 'm': {
			LOG0("DB>> Creating Mallob database with max clause size %u, lbd %d, capacity %zu, freeSize %d",
				 s_maxClauseSize,
				 s_mallobMaxPartitioningLbd,
				 s_maxCapacity,
				 s_mallobMaxFreeSize);
			return std::make_shared<ClauseDatabaseMallob>(
				s_maxClauseSize, s_mallobMaxPartitioningLbd, s_maxCapacity, s_mallobMaxFreeSize);
		}

		default: {
			LOGWARN("Unknown database type '%c', defaulting to PerSize", dbTypeChar);
			LOG0("DB>> Creating PerSize database with max clause size %u", s_maxClauseSize);
			return std::make_shared<ClauseDatabasePerSize>(s_maxClauseSize);
		}
	}
}

bool
ClauseDatabaseFactory::isValidDatabaseType(char dbTypeChar)
{
	return dbTypeChar == 's' || dbTypeChar == 'p' || dbTypeChar == 'e' || dbTypeChar == 'm';
}