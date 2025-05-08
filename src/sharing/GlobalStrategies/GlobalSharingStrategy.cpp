#include "GlobalSharingStrategy.hpp"
#include "painless.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include <list>

// for now the loops works only for root = 0
#define MY_MPI_ROOT 0

GlobalSharingStrategy::GlobalSharingStrategy(const std::shared_ptr<ClauseDatabase>& clauseDB,
											 const std::vector<std::shared_ptr<SharingEntity>>& producers,
											 const std::vector<std::shared_ptr<SharingEntity>>& consumers)
	: SharingStrategy(producers, consumers, clauseDB)
	, requests_sent(false)
{
}

GlobalSharingStrategy::~GlobalSharingStrategy() {}

void
GlobalSharingStrategy::printStats()
{
	LOGSTAT("Global Strategy: receivedCls %d, sharedCls %d, receivedDuplicas %d, sharedDuplicasAvoided %d, "
			"messagesSent %d",
			gstats.receivedClauses.load(),
			gstats.sharedClauses,
			gstats.receivedDuplicas,
			gstats.sharedDuplicasAvoided,
			gstats.messagesSent);
}

std::chrono::microseconds
GlobalSharingStrategy::getSleepingTime()
{
	return std::chrono::microseconds(__globalParameters__.globalSharingSleep);
}

bool
GlobalSharingStrategy::initMpiVariables()
{
	// For End Management
	if (mpi_rank == MY_MPI_ROOT) {
		this->receivedFinalResultRoot.resize(mpi_world_size - 1);
		this->recv_end_requests.resize(mpi_world_size - 1);

		for (int i = 0; i < mpi_world_size - 1; i++) {
			TESTRUNMPI(MPI_Irecv(&receivedFinalResultRoot[i],
								 1,
								 MPI_INT,
								 i + 1,
								 MY_MPI_END,
								 MPI_COMM_WORLD,
								 &this->recv_end_requests[i]));
		}
	}
	return true;
}

void
GlobalSharingStrategy::joinProcess(int winnerRank, SatResult res, const std::vector<int>& model)
{
	//=================================================================
	// Temporary
	// ---------
	MPI_Status status;

	if (MY_MPI_ROOT != mpi_rank) {
		// Non-root processes
		assert(requests_sent);
		TESTRUNMPI(MPI_Wait(&this->send_end_request, &status));
		LOGDEBUG1("Root received my end message");

	} else {
		// Root process
		for (int i = 0; i < mpi_world_size - 1; i++) {
			TESTRUNMPI(MPI_Wait(&this->recv_end_requests[i], &status));
			LOGDEBUG1("Root finalized recv with %d", i + 1);
		}
	}
	//=================================================================
	globalEnding = true;

	finalResult = res;

	mpi_winner = winnerRank;

	if (res == SatResult::SAT && model.size() > 0) {
		finalModel = model;
	}

	if (res != SatResult::UNKNOWN && res != SatResult::TIMEOUT)
		LOGSTAT("The winner is mpi process %d", winnerRank);
	/* TODO becomes a worker to working strategy */
	mutexGlobalEnd.lock();
	condGlobalEnd.notify_all();
	mutexGlobalEnd.unlock();
	LOGDEBUG1("Broadcasted the end locally");
}

bool
GlobalSharingStrategy::doSharing()
{
	// Ending Management
	int end_flag;
	short int rank_winner = 0; // must be zero for receivedFinalResultBcast to be zero

	MPI_Status status;

	int receivedFinalResultBcast = 0;

	if (globalEnding && !requests_sent && mpi_rank != MY_MPI_ROOT) {
		LOGDEBUG1("[GStrat] It is the end, now I will send end to the root %d",(int)finalResult.load());
		TESTRUNMPI(
			MPI_Isend(&finalResult, 1, MPI_INT, MY_MPI_ROOT, MY_MPI_END, MPI_COMM_WORLD, &this->send_end_request));
		requests_sent = true;
	}

	if (mpi_rank == MY_MPI_ROOT) {
		if (globalEnding) // to check if the root found the solution
		{
			receivedFinalResultBcast = static_cast<int>(finalResult.load());
			LOGDEBUG1("[GStrat] It is the end, now I will send end to all descendants (%d)", receivedFinalResultBcast);
			if (receivedFinalResultBcast != (int)SatResult::TIMEOUT)
				rank_winner = MY_MPI_ROOT;
		} else // check recv only if root didn't end
		{
			for (int i = 0; i < mpi_world_size - 1; i++) {
				TESTRUNMPI(MPI_Test(&recv_end_requests[i], &end_flag, &status));
				if (end_flag) {
					LOGDEBUG1("[GStrat] Ending received from node %d (value: %d)",
							status.MPI_SOURCE,
							receivedFinalResultRoot[i]);
					receivedFinalResultBcast = receivedFinalResultRoot[i];
					if (receivedFinalResultBcast != (int)SatResult::TIMEOUT)
						rank_winner = status.MPI_SOURCE;
					// end arrived to the root. Know it should tell everyone
				}
			}
		}
		// send winner and solution in one integer
		receivedFinalResultBcast |= (int)(rank_winner << 16);
		LOGDEBUG2("toSend: %d, rank_winner: %d, shifted: %d",
				  receivedFinalResultBcast,
				  rank_winner,
				  (int)(rank_winner << 16));
	}

	// Broadcast from MY_MPI_ROOT "most significant 16bits are the winner_rank"
	TESTRUNMPI(MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, MY_MPI_ROOT, MPI_COMM_WORLD));

	if (receivedFinalResultBcast != 0) {
		finalResult = static_cast<SatResult>(receivedFinalResultBcast & 0x0000FFFF);
		mpi_winner = (receivedFinalResultBcast & 0xFFFF0000) >> 16;
		receivedFinalResultBcast = static_cast<int>(finalResult.load());
		LOGDEBUG1("[GStrat] It is the mpi end: %d", receivedFinalResultBcast);

		if (!requests_sent && MY_MPI_ROOT != mpi_rank) {
			LOGDEBUG1("[GStrat] Sending last synchro message to root");
			TESTRUNMPI(MPI_Isend(&receivedFinalResultBcast,
								 1,
								 MPI_INT,
								 MY_MPI_ROOT,
								 MY_MPI_END,
								 MPI_COMM_WORLD,
								 &this->send_end_request));
			requests_sent = true;
		}

		return true;
	}
	return false;
}
