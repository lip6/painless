#include "AllGatherSharing.hpp"
#include "containers/ClauseUtils.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include <random>

AllGatherSharing::AllGatherSharing(const std::shared_ptr<ClauseDatabase>& clauseDB, unsigned long bufferSize)
	: totalSize(bufferSize)
	, GlobalSharingStrategy(clauseDB)
{
	requests_sent = false;
}

AllGatherSharing::~AllGatherSharing() {}

void
AllGatherSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int>& model)
{
	this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool
AllGatherSharing::initMpiVariables()
{
	if (mpi_world_size < 2) {
		LOGWARN(
			"[Allgather] I am alone or MPI was not initialized, no need for distributed mode, initialization aborted");
		return false;
	}

	return GlobalSharingStrategy::initMpiVariables();
}

bool
AllGatherSharing::doSharing()
{
	bool can_break;
	MPI_Status status;

	// Sharing Management
	int yes_comm_size;
	MPI_Comm yes_comm;
	int sizeExport;

	/* Ending Detection */
	if (GlobalSharingStrategy::doSharing()) {
		this->joinProcess(mpi_winner, finalResult, {});
		return true;
	}

	// SHARING
	//========

	// At wake up, get the clauses to export and check if empty
	if (requests_sent) {
		// if empty, not worth it or local is ending
		this->color = MPI_UNDEFINED; // to not include this mpi process in the yes_comm
	} else {
		this->color = COLOR_YES;
	}

	// The call must be done only when all global sharers can arrive here.
	TESTRUNMPI(MPI_Comm_split(MPI_COMM_WORLD, this->color, mpi_rank, &yes_comm));

	if (this->color == MPI_UNDEFINED) {
		LOG2("[Allgather] is not willing to share (%d)", mpi_rank);
	} else // do this only if member of yes_comm
	{
		TESTRUNMPI(MPI_Comm_set_errhandler(yes_comm, MPI_ERRORS_RETURN));

		TESTRUNMPI(MPI_Comm_size(yes_comm, &yes_comm_size));

		if (yes_comm_size < 2) {
			return true; // can break and end the global sharer thread since it is doing nothing
		} else {
			LOG3("[Allgather] %d global sharers will share their clauses", yes_comm_size);
		}

		// get clauses to send and serialize
		clausesToSendSerialized.clear();

		gstats.sharedClauses += serializeClauses(clausesToSendSerialized);
		// limit the receive buffer size to contain all the exported buffers
		// resize is used to really allocate the memory (reserve doesn't work with C array access)
		receivedClauses.clear();
		receivedClauses.resize((totalSize)*yes_comm_size);

		LOGDEBUG3("[Allgather] before allgather", mpi_rank);

		//  C like fashion, it overwrites using pointer arithmetic
		TESTRUNMPI(MPI_Allgather(
			&clausesToSendSerialized[0], totalSize, MPI_INT, &receivedClauses[0], totalSize, MPI_INT, yes_comm));
		gstats.messagesSent += yes_comm_size;

		// Now I have a vector of the all gathered buffers
		deserializeClauses(receivedClauses, yes_comm_size);
	}

	LOG2("[Allgather] received cls %u shared cls %d", this->gstats.receivedClauses.load(), this->gstats.sharedClauses);

	return false;
}

//===============================
// Serialization/Deseralization
//===============================

int
AllGatherSharing::serializeClauses(std::vector<int>& serialized_v_cls)
{
	int nb_clauses = 0;
	unsigned int dataCount = serialized_v_cls.size(); // it is an append operation so datacount do not start from zero
	ClauseExchangePtr tmp_cls;

	while (dataCount < totalSize &&
		   m_clauseDB->getOneClause(tmp_cls)) // stops when buffer is full or no more clauses are available
	{
		if (dataCount + 2 + tmp_cls->size > totalSize) {
			LOGDEBUG2("[Allgather] Serialization overflow avoided, %d/%d, wanted to add %d",
					  dataCount,
					  totalSize,
					  tmp_cls->size + 2);
			this->importClause(tmp_cls); // reinsert the clause to the database to not lose it
			break;
		} else {
			// check with bloom filter if clause will be sent. If already sent, the clause is directly released
			if (!this->b_filter.contains(tmp_cls->lits, tmp_cls->size)) {
				serialized_v_cls.push_back(tmp_cls->size);
				serialized_v_cls.push_back(tmp_cls->lbd);
				serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->begin(), tmp_cls->end());
				this->b_filter.insert(tmp_cls->lits, tmp_cls->size);
				nb_clauses++;

				dataCount += (tmp_cls->size+2);
			} else {
				gstats.sharedDuplicasAvoided++;
			}
		}
	}

	// add final zero and fill with zero if needed
	serialized_v_cls.insert(serialized_v_cls.end(), totalSize - dataCount + 1, 0);

	LOGDEBUG1("Serialized %u clauses into buffer of size %u", nb_clauses, serialized_v_cls.size());
	return nb_clauses;
}

void
AllGatherSharing::deserializeClauses(const std::vector<int>& serialized_v_cls, int num_buffers)
{
	unsigned int i = 0;
	int size, lbd;
	ClauseExchangePtr p_cls;
	int bufferSize = serialized_v_cls.size();
	int single_buffer_size = bufferSize / num_buffers;

	LOGDEBUG2("Deserializing Buffer of Size %u", bufferSize);

	while (i < bufferSize) {
		// Check for the end marker
		if (serialized_v_cls[i] == 0) {
			// Move to the next buffer
			i = ((i / single_buffer_size) + 1) * single_buffer_size;
			continue;
		}

		size = serialized_v_cls[i++];
		lbd = serialized_v_cls[i++];

		if (i + size > bufferSize) {
			LOGERROR("Deserialization error: Incomplete clause data");
			break;
		}

		if (!this->b_filter.contains(serialized_v_cls.data() + i, size)) {
			p_cls =
				ClauseExchange::create(&serialized_v_cls[i], &serialized_v_cls[i + size], lbd, this->getSharingId());
			if (this->exportClause(p_cls)) {
				gstats.receivedClauses++;
			}
			this->b_filter.insert(serialized_v_cls.data() + i, size);
		} else {
			gstats.receivedDuplicas++;
		}

		i += size;
	}
}