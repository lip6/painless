#include "MallobSharing.hpp"
#include "containers/ClauseUtils.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include <algorithm>
#include <cmath>
#include <random>

#define MALLOB_MPI_ROOT 0
#define COMPENSATED_SIZE (unsigned int)(std::ceil(this->compensationFactor * this->defaultBufferSize))

MallobSharing::MallobSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
				  unsigned long baseBufferSize,
				  unsigned long maxBufferSize,
				  unsigned int lbdLimitAtImport,
				  unsigned int sizeLimitAtImport,
				  unsigned int roundsPerSecond,
				  float maxCompensation,
				  unsigned int resharePeriodMicroSec)
	: GlobalSharingStrategy(clauseDB)
	, baseSize(baseBufferSize)
	, maxSize(maxBufferSize)
	, myBitVector(10, 0) // 640 clauses
	, sleepTime(10000) // not important since it is recomputed at each round
	, lbdLimitAtImport(lbdLimitAtImport)
	, sizeLimitAtImport(sizeLimitAtImport)
	, maxCompensationFactor(maxCompensation)
{
	// Initialize remaining member variables
	m_freeSize = 1;
	defaultBufferSize = 0;
	requests_sent = false;
	accumulatedAdmittedLiterals = 0;
	accumulatedDesiredLiterals = 0;
	lastEpochAdmittedLits = 0;
	lastEpochReceivedLits = 0;
	compensationFactor = 1.0f;
	estimatedIncomingLits = 0.0f;
	estimatedSharedLits = -1.0f;

	// Initialize filter
	initializeFilter(resharePeriodMicroSec, roundsPerSecond);

	// Print parameters
	if (mpi_rank == MALLOB_MPI_ROOT) {
		LOGSTAT("MallobSharing Parameters:");
		LOGSTAT("  Base Size: %d", baseSize);
		LOGSTAT("  Max Size: %d", maxSize);
		LOGSTAT("  Sleep Time: %d", sleepTime);
		LOGSTAT("  Reshare Period: %d", resharePeriodMicroSec);
		LOGSTAT("  Shares Per Second: %d", roundsPerSecond);
		LOGSTAT("  Size Limit At Import: %d", sizeLimitAtImport);
		LOGSTAT("  Lbd Limit At Import: %d", lbdLimitAtImport);
	}
}

MallobSharing::~MallobSharing() {}

void
MallobSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int>& model)
{
	this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool
MallobSharing::initMpiVariables()
{
	// [0]: parent, [1]: right, [2]: left. If it has one child it must be a right
	// one
	right_child = (mpi_rank * 2 + 1 < mpi_world_size) ? mpi_rank * 2 + 1 : MPI_UNDEFINED;
	left_child = (mpi_rank * 2 + 2 < mpi_world_size) ? mpi_rank * 2 + 2 : MPI_UNDEFINED;
	nb_children = (right_child == MPI_UNDEFINED) ? 0 : (left_child == MPI_UNDEFINED) ? 1 : 2;
	father = (mpi_rank == MALLOB_MPI_ROOT) ? MPI_UNDEFINED : (mpi_rank - 1) / 2;
	LOG2("[Tree] parent:%d, left: %d, right: %d", father, left_child, right_child);

	buffers.reserve(nb_children);

	return GlobalSharingStrategy::initMpiVariables();
}

void
MallobSharing::computeCompensation()
{
	if (estimatedIncomingLits <= 0) {
		estimatedIncomingLits = lastEpochReceivedLits;
	}
	if (estimatedSharedLits <= 0)
		estimatedSharedLits = lastEpochAdmittedLits;
	else {
		accumulatedAdmittedLiterals = 0.9f * accumulatedAdmittedLiterals + lastEpochAdmittedLits;
		accumulatedDesiredLiterals = std::max(
			1.f, 0.9f * accumulatedDesiredLiterals + std::min(lastEpochReceivedLits, (size_t)defaultBufferSize));

		estimatedIncomingLits = 0.6f * estimatedIncomingLits + 0.4f * (lastEpochReceivedLits / compensationFactor);
		estimatedSharedLits = 0.6f * estimatedSharedLits + 0.4f * (lastEpochAdmittedLits / compensationFactor);
	}

	compensationFactor = estimatedSharedLits <= 0
							 ? 1.0
							 : ((accumulatedDesiredLiterals - accumulatedAdmittedLiterals + estimatedIncomingLits) /
								estimatedSharedLits);

	compensationFactor =
		std::max(0.1f, std::min(maxCompensationFactor, compensationFactor));
}

void
MallobSharing::computeBufferSize(unsigned int count)
{
	// This paper describes the equation and gives the optimal alpha value:
	// https://doi.org/10.1007/978-3-030-80223-3_35 int nb_buffers_aggregated =
	// countDescendants(mpi_world_size, mpi_rank); this->defaultBufferSize =
	// nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) *
	// default_size;

	// 2024 version
	float baseSizeFloat = (float)baseSize;
	float tmpFloat =
		maxSize - (maxSize - baseSize) * std::exp((baseSizeFloat / (baseSizeFloat - maxSize)) * (count - 1));
	defaultBufferSize = std::ceil(tmpFloat);
	LOGDEBUG1("Totalize is: %f -> %u", tmpFloat, defaultBufferSize);
}

/* Called by Solvers !!!*/
bool
MallobSharing::importClause(const ClauseExchangePtr& cls)
{
	LOGDEBUG3("Mallob Strategy %d importing a cls %p", this->getSharingId(), cls.get());
	// Filter is updated only when strategy gets the clause from clauseDB
	if (cls->size > sizeLimitAtImport || cls->lbd > lbdLimitAtImport) {
		return false;
	}
	return m_clauseDB->addClause(cls);
};

bool
MallobSharing::exportClauseToClient(const ClauseExchangePtr& cls, std::shared_ptr<SharingEntity> client)
{
	// Hypothesis: isClauseShared returned false
	// check in filter if should import to client
	// Must be called by the strategy (filter is not thread safe !!)
	if (canConsumerImportClause(cls, client->getSharingId())) {
		// LOGDEBUG1("Clause %s will be imported by %u", cls->toString().c_str(), client->getSharingId());
		return client->importClause(cls);
	} else
		return false;
}

std::chrono::microseconds
MallobSharing::getSleepingTime()
{
	return std::chrono::microseconds(this->sleepTime);
}

bool
MallobSharing::doSharing()
{
	LOGDEBUG1("-------------------------[%d]-------------------------", m_currentEpoch);
	auto total_start = std::chrono::high_resolution_clock::now();

	/* Ending Detection */
	if (GlobalSharingStrategy::doSharing()) {
		this->joinProcess(mpi_winner, finalResult, {});
		return true;
	}

	MPI_Status status;

	// Sharing Management
	int received_buffer_size = 0;
	int root_size = 0;
	int buffer_size = 0;
	int nb_buffers_aggregated = 1; // my buffer is accounted here

	// Compute Compensation Factor
	if (father == MPI_UNDEFINED)
		computeCompensation();

	// Share Compensation Factor
	MPI_Bcast(&compensationFactor, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);

	// Reset Last Epoch Compensation
	lastEpochReceivedLits = 0;
	lastEpochAdmittedLits = 0;

	// SHARING
	//========
	// if not leaf , wait for children clauses
	if (nb_children >= 1) // has a right child
	{
		receivedClausesRight.clear();
		buffers.clear();

		TESTRUNMPI(MPI_Probe(right_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
		TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &received_buffer_size));
		receivedClausesRight.resize(received_buffer_size);

		LOGDEBUG2("Parent %d waiting for right child %d on MPI_Recv", mpi_rank, right_child);
		TESTRUNMPI(MPI_Recv(&receivedClausesRight[0],
							received_buffer_size,
							MPI_INT,
							right_child,
							MYMPI_CLAUSES,
							MPI_COMM_WORLD,
							&status));

		nb_buffers_aggregated += receivedClausesRight.back(); // get nb buffers aggregated of my right child
		receivedClausesRight.pop_back();					  // remove the nb_buffers_aggregated

		// add the clauses to the mergeBuffer
		buffers.push_back(std::ref(receivedClausesRight));

		if (nb_children == 2) // has a left child
		{
			receivedClausesLeft.clear();

			TESTRUNMPI(MPI_Probe(left_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
			TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &received_buffer_size));
			receivedClausesLeft.resize(received_buffer_size);
			LOGDEBUG2("Parent %d waiting for left child %d on MPI_Recv", mpi_rank, left_child);
			TESTRUNMPI(MPI_Recv(&receivedClausesLeft[0],
								received_buffer_size,
								MPI_INT,
								left_child,
								MYMPI_CLAUSES,
								MPI_COMM_WORLD,
								&status));

			nb_buffers_aggregated += receivedClausesLeft.back(); // get nb buffers aggregated of my right child
			receivedClausesLeft.pop_back();						 // remove the nb_buffers_aggregated

			// add the clause to the mergeBuffer
			buffers.push_back(std::ref(receivedClausesLeft));
		}
	}

	this->computeBufferSize(nb_buffers_aggregated);

	clausesToSendSerialized.clear();
	// Gets also clauses from local database, leafs do only that.
	int clauseCount = mergeSerializedBuffersWithMine(buffers, clausesToSendSerialized, lastEpochReceivedLits);
	gstats.sharedClauses += clauseCount;

	if (father != MPI_UNDEFINED)								  // not needed in root
		clausesToSendSerialized.push_back(nb_buffers_aggregated); // number of buffers aggregated

	LOGDEBUG1("[Tree] TotalSize = %d(%d)(%f%%), Buffer to send size=%u(nflits:%u), clauses %u",
			  COMPENSATED_SIZE,
			  nb_buffers_aggregated,
			  ((float)lastEpochReceivedLits) / COMPENSATED_SIZE * 100,
			  clausesToSendSerialized.size(),
			  lastEpochReceivedLits,
			  clauseCount);

	// If not root wait for parent
	if (father != MPI_UNDEFINED) {
		receivedClausesFather.clear();
		LOGDEBUG1("%d->%d : buff:%d", mpi_rank, father, clausesToSendSerialized.size());

		// Send to my parent my clauses.
		TESTRUNMPI(MPI_Send(&clausesToSendSerialized[0],
							clausesToSendSerialized.size(),
							MPI_INT,
							father,
							MYMPI_CLAUSES,
							MPI_COMM_WORLD));
		gstats.messagesSent++;

		// Wait for my parent's response
		LOGDEBUG2("Me %d waiting for my parent's %d response ", mpi_rank, father);
		TESTRUNMPI(MPI_Probe(father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
		TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &received_buffer_size));
		receivedClausesFather.resize(received_buffer_size);

		TESTRUNMPI(MPI_Recv(
			&receivedClausesFather[0], received_buffer_size, MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
	}

	// If it is the root just send merged buffer, otherwise pass on the received buffer from the father
	std::reference_wrapper<std::vector<int>> finalMergedBuffer =
		(father == MPI_UNDEFINED) ? std::ref(clausesToSendSerialized) : std::ref(receivedClausesFather);

	// Response to my children
	if (nb_children >= 1) {
		LOGDEBUG2("Me %d responding to my right child %d", mpi_rank, right_child);
		TESTRUNMPI(MPI_Send(&finalMergedBuffer.get()[0],
							finalMergedBuffer.get().size(),
							MPI_INT,
							right_child,
							MYMPI_CLAUSES,
							MPI_COMM_WORLD));
		gstats.messagesSent++;
		if (nb_children == 2) {
			LOGDEBUG2("Me %d responding to my left child %d", mpi_rank, left_child);
			TESTRUNMPI(MPI_Send(&finalMergedBuffer.get()[0],
								finalMergedBuffer.get().size(),
								MPI_INT,
								left_child,
								MYMPI_CLAUSES,
								MPI_COMM_WORLD));
			gstats.messagesSent++;
		}
	}

	deserializedClauses.clear();
	// deserialize for all
	deserializeClauses(finalMergedBuffer.get());

	// Generate the bitvector using deserialized clauses and isClauseShared
	size_t deserializedCount = deserializedClauses.size();
	size_t ullBlockCounts = myBitVector.num_blocks();

	LOGDEBUG1("Deserialized %u clauses from %u integers", deserializedCount, finalMergedBuffer.get().size());
	LOGDEBUG1("BitVector size %u(%u)", myBitVector.num_blocks(), ullBlockCounts);

	// Nodes will wait on recv
	if (nb_children >= 1) // has right child
	{
		std::vector<pl::Bitset> bitVectorsToMerge;

		bitVectorsToMerge.emplace_back(deserializedCount, 0);

		TESTRUNMPI(MPI_Recv(bitVectorsToMerge.back().data(),
							ullBlockCounts,
							MPI_LONG_LONG,
							right_child,
							MYMPI_BITSET,
							MPI_COMM_WORLD,
							&status));

		if (nb_children == 2) // has left child
		{
			bitVectorsToMerge.emplace_back(deserializedCount, 0);

			TESTRUNMPI(MPI_Recv(bitVectorsToMerge.back().data(),
								ullBlockCounts,
								MPI_LONG_LONG,
								left_child,
								MYMPI_BITSET,
								MPI_COMM_WORLD,
								&status));
		}

		// merge all bitVectors
		myBitVector.merge_or(bitVectorsToMerge);
	}

	// leafs will be execute this directly
	if (father != MPI_UNDEFINED) // send to father
	{
		TESTRUNMPI(MPI_Send(myBitVector.data(), ullBlockCounts, MPI_LONG_LONG, father, MYMPI_BITSET, MPI_COMM_WORLD));
		gstats.messagesSent++;
	} else {
		// I am the root, I received both buffers previously, now broadcast
	}
	MPI_Bcast(myBitVector.data(), ullBlockCounts, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
	gstats.receivedClauses += deserializedCount;
	// loop to export using aggregated vector
	for (size_t i = 0; i < deserializedCount; i++) {
		// export if not shared before
		if (!myBitVector[i]) {
			this->exportClause(deserializedClauses[i]);
			if (deserializedClauses[i]->size >
				m_freeSize) // only non free are counted in receivedCount, thus admittedCount also do not count them
				lastEpochAdmittedLits += deserializedClauses[i]->size;
			// mark as shared
			this->markClauseAsShared(deserializedClauses[i]);
		} else {
			gstats.receivedDuplicas++;
		}
	}

	if (father == MPI_UNDEFINED) {
		LOG1(
			"[Tree][%d] received cls %u (duplicates: %d) shared cls %d, last sharing: %i/%i/%i globally passed ~> c=%.3f",
			m_currentEpoch,
			this->gstats.receivedClauses.load(),
			this->gstats.receivedDuplicas,
			this->gstats.sharedClauses,
			lastEpochAdmittedLits,
			lastEpochReceivedLits,
			COMPENSATED_SIZE,
			compensationFactor);
	}

	incrementEpoch();
	this->m_clauseDB->shrinkDatabase();
	this->shrinkFilter();

	auto total_end = std::chrono::high_resolution_clock::now();
	auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start);

	// update sleepingTime: sleepTime = 1/sharingPerSec - total_duration (if negative
	// set to 0): very naive and basic
	long sleepMicroSec = (1000000 / m_sharingPerSecond) - total_duration.count();
	this->sleepTime = (sleepMicroSec < 0) ? 0 : sleepMicroSec;

	return false;
}

//==============================
// Serialization/Deseralization
//==============================

void
MallobSharing::deserializeClauses(const std::vector<int>& serialized_v_cls)
{
	const unsigned int buffer_size = serialized_v_cls.size();
	unsigned int i = 0;
	int size, lbd;

	deserializedClauses.clear();
	myBitVector.clear();
	myBitVector.resize(0); // Reset the bitset size

	while (i < buffer_size) {
		size = serialized_v_cls[i++];
		lbd = serialized_v_cls[i++];

		if (i + size > buffer_size) {
			LOGERROR("Deserialization error: Incomplete clause data");
			break;
		}

		ClauseExchangePtr p_cls =
			ClauseExchange::create(&serialized_v_cls[i], &serialized_v_cls[i + size], lbd, this->getSharingId());
		if (p_cls->size > 1) {
			p_cls->lbd += 1; // increment lbd value for non units
		}
		deserializedClauses.push_back(p_cls);

		// Resize the bitset if necessary, the bitVector will be bigger than deserializedClauses after a maximal sharing
		if (deserializedClauses.size() > myBitVector.size()) {
			myBitVector.resize(deserializedClauses.size());
		}

		// Compute whether the clause is shared and set the corresponding bit
		bool isShared = isClauseShared(p_cls);
		myBitVector.set(deserializedClauses.size() - 1, isShared);

		i += size;
	}
}

struct simpleSpan
{
	int size = 0;
	int lbd = 0;
	int source = -1;
	int* lits = nullptr;

	simpleSpan(int size, int lbd, int source, int* lits)
		: size(size)
		, lbd(lbd)
		, source(source)
		, lits(lits)
	{
	}
};

/* TODO send units in a consecutive manner, after 0 starts normal serialization */
int
MallobSharing::mergeSerializedBuffersWithMine(std::vector<std::reference_wrapper<std::vector<int>>>& buffers,
											  std::vector<int>& result,
											  size_t& nonFreeLiteralsCount)
{
	unsigned int dataCount = 0;
	unsigned int nb_clauses = 0;
	unsigned int buffer_count = buffers.size();
	int winner = -1; // the buffer that had its tmp_clause selected

	ClauseExchangePtr localClause;

	std::vector<unsigned int> indexes(buffer_count, 0);
	std::vector<unsigned int> bufferSizes(buffer_count);
	std::vector<simpleSpan> tmp_clauses; // + current node

	// Temporary bloom filter to not push twice the same clause
	BloomFilter filter;

	// bootstrap tmp_clauses
	for (unsigned int k = 0; k < buffer_count; k++) {
		bufferSizes[k] = buffers[k].get().size();
		LOGDEBUG2("Buffer %u has size: %u", k, bufferSizes[k]);

		if (indexes[k] + 2 <= bufferSizes[k]) {
			LOGDEBUG2("Buffer at %d will generate a clause", k);
			int size = buffers[k].get().operator[](indexes[k]);
			int lbd = buffers[k].get().operator[](indexes[k] + 1);

			if (indexes[k] + 2 + size <= bufferSizes[k]) {
				tmp_clauses.emplace_back(size, lbd, k, buffers[k].get().data() + indexes[k] + 2);
				indexes[k] += 2 + size;
				LOGDEBUG2("ADDING CLAUSE: %d,%d,%d,%p",
						  tmp_clauses.back().size,
						  tmp_clauses.back().lbd,
						  tmp_clauses.back().source,
						  tmp_clauses.back().lits);
				assert(size > 1 && lbd >= 1 || size == 1 && lbd == 0);
			} else {
				LOGDEBUG2("Bad Serialization, found two last remaining elements, size and lbd with no literals");
				std::abort();
			}
		} else {
			continue;
		}
	}

	if (this->getOneClauseWrapper(localClause)) {
		LOGDEBUG2("Local database will generate a clause", buffer_count);
		tmp_clauses.emplace_back(localClause->size, localClause->lbd, buffer_count, localClause->begin());
	}

	LOGDEBUG1("Tmp_clauses is bootstrapped with %u clauses", tmp_clauses.size());

	while (dataCount < COMPENSATED_SIZE) // while we can add data
	{
		LOGDEBUG2("Current Winner is %d, dataCount %u (real:%u) vs compensated size %u, tmp_clauses sizes: %u",
				  winner,
				  dataCount,
				  result.size(),
				  COMPENSATED_SIZE,
				  tmp_clauses.size());

		assert(tmp_clauses.size() <= buffer_count + 1);

		// Select best current clause
		if (!tmp_clauses.empty()) {
			// Choose the clause with minimum size at(0), then minimum LBD at(1)
			auto min_cls =
				std::min_element(tmp_clauses.begin(), tmp_clauses.end(), [](const simpleSpan& a, const simpleSpan& b) {
					return (a.size < b.size || (a.size == b.size && a.lbd < b.lbd));
				});

			LOGDEBUG2("SELECTED CLAUSE: %d,%d,%d,%p from %u",
					  min_cls->size,
					  min_cls->lbd,
					  min_cls->source,
					  min_cls->lits,
					  tmp_clauses.size());

			winner = min_cls->source;

			assert(((min_cls->size > 1 && min_cls->lbd >= 1) || (min_cls->size == 1 && min_cls->lbd == 0)) &&
				   min_cls->source >= 0 && min_cls->lits != nullptr);
			// lbd and size are not counted in dataCount
			if (min_cls->size > m_freeSize && dataCount + min_cls->size > COMPENSATED_SIZE) {
				LOGDEBUG2("Breaking due to overflow: %u vs %u", dataCount + min_cls->size, COMPENSATED_SIZE);
				break; // Avoid overflow
			}

			if (!filter.contains_or_insert(min_cls->lits, min_cls->size)) {
				if (min_cls->size > m_freeSize) {
					dataCount += min_cls->size;
				}
				result.push_back(min_cls->size);
				result.push_back(min_cls->lbd);
				result.insert(result.end(), min_cls->lits, min_cls->lits + min_cls->size);
				nb_clauses++;
			} else {
				gstats.sharedDuplicasAvoided++;
			}

			tmp_clauses.erase(min_cls);
		} else {
			LOGDEBUG1("Breaking due to empty buffers");
			break;
		}

		// Try to get clauses
		for (int k = 0; k < buffer_count; k++) {
			LOGDEBUG2("Buffer at %d , index %u, size %u", k, indexes[k], bufferSizes[k]);

			// Extract a clause if previous one was serialized and we have enough data
			if (winner == k && indexes[k] + 2 <= bufferSizes[k]) {
				LOGDEBUG2("Buffer at %d will generate a clause", k);
				int size = buffers[k].get().operator[](indexes[k]);
				int lbd = buffers[k].get().operator[](indexes[k] + 1);

				if (indexes[k] + 2 + size <= bufferSizes[k]) {
					tmp_clauses.emplace_back(size, lbd, k, buffers[k].get().data() + indexes[k] + 2);
					indexes[k] += 2 + size;
					LOGDEBUG2("ADDING CLAUSE: %d,%d,%d,%p",
							  tmp_clauses.back().size,
							  tmp_clauses.back().lbd,
							  tmp_clauses.back().source,
							  tmp_clauses.back().lits);
					assert(size > 1 && lbd >= 1 || size == 1 && lbd == 0);
				} else {
					LOGDEBUG2("Bad Serialization, found two last remaining elements, size and lbd with no literals");
					std::abort();
				}
			} else {
				// not the winner or no remaining data
				continue;
			}
		}

		// Get a local clause
		if (buffer_count == winner && this->getOneClauseWrapper(localClause)) {
			LOGDEBUG2("Local database will add a clause of size %u", buffer_count, localClause->size);

			tmp_clauses.emplace_back(localClause->size, localClause->lbd, buffer_count, localClause->begin());

			LOGDEBUG2("ADDING LOCAL CLAUSE: %d,%d,%d,%p",
					  tmp_clauses.back().size,
					  tmp_clauses.back().lbd,
					  tmp_clauses.back().source,
					  tmp_clauses.back().lits);
		}
	}

	if (!result.size()) {
		for (unsigned int i = 0; i < buffer_count; i++) {
			assert(bufferSizes[i] == 0);
		}
	}

	// Process remaining clauses from both tmp_clauses and buffers
	auto processRemainingClauses = [this, &filter](const simpleSpan& cls) {
		if (!filter.contains_or_insert(cls.lits, cls.size)) {
			importClause(ClauseExchange::create(cls.lits, cls.lits + cls.size, cls.lbd, this->getSharingId()));
		}
	};

	// Process remaining clauses in tmp_clauses
	for (const auto& cls : tmp_clauses) {
		processRemainingClauses(cls);
	}

	// Process remaining clauses in buffers
	for (unsigned int i = 0; i < buffer_count; i++) {
		while (indexes[i] < bufferSizes[i]) {
			if (indexes[i] + 2 <= bufferSizes[i]) {
				int size = buffers[i].get().operator[](indexes[i]);
				int lbd = buffers[i].get().operator[](indexes[i] + 1);

				if (indexes[i] + 2 + size <= bufferSizes[i]) {
					simpleSpan cls = simpleSpan(size, lbd, -1, buffers[i].get().data() + indexes[i] + 2);
					processRemainingClauses(cls);
					indexes[i] += 2 + size;
				} else {
					LOGERROR("Bad Serialization, found two last remaining elements, size and lbd with no literals");
					std::abort();
				}
			} else {
				// Not enough data for size and LBD, move to next buffer
				break;
			}
		}
	}

	nonFreeLiteralsCount = dataCount; // only non free literals are counted

	LOGDEBUG1("Merged %d clauses, dataCount = %u, compensated size = %u", nb_clauses, dataCount, COMPENSATED_SIZE);

	return nb_clauses;
}
