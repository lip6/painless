#include "GenericGlobalSharing.hpp"
#include "containers/ClauseUtils.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include <random>

GenericGlobalSharing::GenericGlobalSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
										   const std::vector<int>& subscriptions,
										   const std::vector<int>& subscribers,
										   unsigned long bufferSize)
	: GlobalSharingStrategy(clauseDB)
	, subscriptions(subscriptions)
	, subscribers(subscribers)
	, totalSize(bufferSize)
{
}

GenericGlobalSharing::~GenericGlobalSharing() {}

void
GenericGlobalSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int>& model)
{
	this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool
GenericGlobalSharing::initMpiVariables()
{
	if (mpi_world_size < 2) {
		LOGWARN(
			"[Generic] I am alone or MPI was not initialized , no need for distributed mode, initialization aborted.");
		return false;
	}

	LOGVECTOR(&this->subscriptions[0], subscriptions.size(), "[Generic Sharing] Producers: ");
	LOGVECTOR(&this->subscribers[0], subscribers.size(), "[Generic Sharing] Consumers: ");

	return GlobalSharingStrategy::initMpiVariables();
}

bool
GenericGlobalSharing::doSharing()
{
	// Sharing Management
	int received_size;
	int old_size;
	MPI_Status status;

	MPI_Request tmp_request;

	/* Ending Detection */
	if (GlobalSharingStrategy::doSharing()) {
		this->joinProcess(mpi_winner, finalResult, {});
		return true;
	}

	/* Sharing */
	clausesToSendSerialized.clear();
	gstats.sharedClauses += serializeClauses(clausesToSendSerialized);
	unsigned int serializedSize = clausesToSendSerialized.size();

	// Send to my subscribers my clauses
	for (unsigned int i = 0; i < subscribers.size(); i++) {
		TESTRUNMPI(MPI_Isend(&clausesToSendSerialized[0],
							 serializedSize,
							 MPI_INT,
							 subscribers[i],
							 MYMPI_CLAUSES,
							 MPI_COMM_WORLD,
							 &tmp_request));
		LOG2("[Generic] Sent a message of size %d to %d", serializedSize, subscribers[i]);
		sendRequests.push_back(tmp_request);
		gstats.messagesSent++;
	}

	// Check if my subscriptions sent me any clauses
	receivedClauses.clear();

	for (unsigned int i = 0; i < subscriptions.size(); i++) {

		old_size = receivedClauses.size();

		TESTRUNMPI(MPI_Probe(subscriptions[i], MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
		TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &received_size));

		receivedClauses.resize(old_size + received_size);
		LOGDEBUG2("Oldsize = %d, NewSize = %d", old_size, old_size + received_size);
		MPI_Recv(&receivedClauses[old_size],
				 received_size,
				 MPI_INT,
				 subscriptions[i],
				 MYMPI_CLAUSES,
				 MPI_COMM_WORLD,
				 &status);
		LOG2("[Generic] Received a message of size %d from %d", received_size, subscriptions[i]);
	}

	deserializeClauses(receivedClauses);

	for (auto sendRequest : this->sendRequests)
		TESTRUNMPI(MPI_Wait(&sendRequest, &status));

	sendRequests.clear();
	LOG2("[Generic] received cls %u shared cls %d", this->gstats.receivedClauses.load(), this->gstats.sharedClauses);

	return false;
}

//==============================
// Serialization/Deseralization
//==============================

int
GenericGlobalSharing::serializeClauses(std::vector<int>& serialized_v_cls)
{
	unsigned int clausesSelected = 0;
	ClauseExchangePtr tmp_cls;

	unsigned int dataCount = serialized_v_cls.size();

	while (m_clauseDB->getOneClause(tmp_cls) && (totalSize <= 0 || dataCount < totalSize)) {
		if (totalSize > 0 && dataCount + tmp_cls->size > totalSize) {
			LOGDEBUG2("[Generic] Serialization overflow avoided, %d/%d, wanted to add %d",
					  dataCount,
					  totalSize,
					  tmp_cls->size);
			this->importClause(tmp_cls); // reinsert to clauseToSend if doesn't fit
			break;
		}

		// check with bloom filter if clause will be sent. If already sent, the clause is directly released
		if (!this->b_filter_send.contains(tmp_cls->lits, tmp_cls->size)) {
			serialized_v_cls.push_back(tmp_cls->size);
			serialized_v_cls.push_back(tmp_cls->lbd);
			serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->begin(), tmp_cls->end());
			this->b_filter_send.insert(tmp_cls->lits, tmp_cls->size);
			clausesSelected++;

			dataCount += (tmp_cls->size);
		} else {
			gstats.sharedDuplicasAvoided++;
		}
	}

	if (totalSize > 0 && dataCount > totalSize + 2 * clausesSelected) {
		LOGWARN("Panic!! datacount(%d) > totalsize(%d).", dataCount, totalSize);
	}
	return clausesSelected;
}

void
GenericGlobalSharing::deserializeClauses(std::vector<int>& serialized_v_cls)
{
	unsigned int i = 0;
	int size;
	int lbd;
	ClauseExchangePtr p_cls;
	int bufferSize = serialized_v_cls.size();

	while (i < bufferSize) {
		size = serialized_v_cls[i++];
		lbd = serialized_v_cls[i++];

		if (i + size > bufferSize) {
			LOGERROR("Deserialization error: Incomplete clause data");
			break;
		}

		if (!this->b_filter_recv.contains(serialized_v_cls.data() + i, size)) {
			p_cls =
				ClauseExchange::create(&serialized_v_cls[i], &serialized_v_cls[i + size], lbd, this->getSharingId());
			if (this->exportClause(p_cls))
				gstats.receivedClauses++;
			this->b_filter_recv.insert(serialized_v_cls.data() + i,
									   size); // either added or not wanted (> maxClauseSize)
		} else {
			gstats.receivedDuplicas++;
		}

		i += size;
	}
}
