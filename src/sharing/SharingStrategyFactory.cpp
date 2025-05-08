#include "painless.hpp"

#include "sharing/LocalStrategies/HordeSatSharing.hpp"
#include "sharing/LocalStrategies/SimpleSharing.hpp"

#include "sharing/GlobalStrategies/AllGatherSharing.hpp"
#include "sharing/GlobalStrategies/GenericGlobalSharing.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "SharingStrategyFactory.hpp"
#include "containers/ClauseDatabases/ClauseDatabaseFactory.hpp"

int SharingStrategyFactory::selectedLocal = 0;
int SharingStrategyFactory::selectedGlobal = 0;

void
SharingStrategyFactory::instantiateLocalStrategies(int strategyNumber,
												   std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
												   std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers)
{
	std::vector<std::shared_ptr<SharingEntity>> allEntities;
	allEntities.insert(allEntities.end(), cdclSolvers.begin(), cdclSolvers.end());

	if (!allEntities.size()) {
		LOGWARN("No SharingEntity, SharingStrategy %d will not be instantiated", strategyNumber);
		return;
	}

	if (strategyNumber == 0) {
		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<std::mt19937::result_type> distShr(1, LOCAL_SHARING_STRATEGY_COUNT);
		strategyNumber = distShr(rng);
	}

	// Initialize required fields of the Database Factory
	ClauseDatabaseFactory::initialize(__globalParameters__.maxClauseSize, 100'000, 2, 1);

	unsigned currentSize = localStrategies.size();

	std::shared_ptr<ClauseDatabase> lsharedDB =
		ClauseDatabaseFactory::createDatabase(__globalParameters__.localSharingDB.at(0));
	std::shared_ptr<ClauseDatabase> lsharedDB2 =
		ClauseDatabaseFactory::createDatabase(__globalParameters__.localSharingDB.at(0));

	switch (strategyNumber) {
		case 1:
			LOG0("LSTRAT>> HordeSatSharing(1Grp)");
			localStrategies.emplace_back(new HordeSatSharing(lsharedDB,
															 __globalParameters__.sharedLiteralsPerProducer,
															 __globalParameters__.hordeInitialLbdLimit,
															 __globalParameters__.hordeInitRound,
															 allEntities,
															 allEntities));
			break;
		case 2:
			if (cdclSolvers.size() <= 2) {
				LOGERROR("Please select another sharing strategy other than 2 if you want to have %d solvers.",
						 cdclSolvers.size());
				LOGERROR("If you used -dist option, the strategies may not work");
				exit(PWARN_LSTRAT_CPU_COUNT);
			}

			LOG0("LSTRAT>> HordeSatSharing (2Grp of producers, Common Per Size Database)");

			localStrategies.emplace_back(
				new HordeSatSharing(lsharedDB,
									__globalParameters__.sharedLiteralsPerProducer,
									__globalParameters__.hordeInitialLbdLimit,
									__globalParameters__.hordeInitRound,
									{ allEntities.begin(), allEntities.begin() + allEntities.size() / 2 },
									{ allEntities.begin(), allEntities.end() }));
			localStrategies.emplace_back(
				new HordeSatSharing(lsharedDB2,
									__globalParameters__.sharedLiteralsPerProducer,
									__globalParameters__.hordeInitialLbdLimit,
									__globalParameters__.hordeInitRound,
									{ allEntities.begin() + allEntities.size() / 2, allEntities.end() },
									{ allEntities.begin(), allEntities.end() }));
			break;
		case 3:
			LOG0("LSTRAT>> SimpleSharing (Common Per Size Database)");

			localStrategies.emplace_back(new SimpleSharing(lsharedDB,
														   __globalParameters__.simpleShareLimit,
														   __globalParameters__.sharedLiteralsPerProducer,
														   allEntities,
														   allEntities));
			break;
		default:
			LOGERROR("The sharing strategy number chosen isn't correct. Sharing is disabled !");
			break;
	}

	for (unsigned i = currentSize; i < localStrategies.size(); i++) {
		localStrategies.at(i)->connectConstructorProducers();
	}
	SharingStrategyFactory::selectedLocal = strategyNumber;
}

void
SharingStrategyFactory::instantiateGlobalStrategies(
	int strategyNumber,
	std::vector<std::shared_ptr<GlobalSharingStrategy>>& globalStrategies)
{
	int right_neighbor = (mpi_rank - 1 + mpi_world_size) % mpi_world_size;
	int left_neighbor = (mpi_rank + 1) % mpi_world_size;
	std::vector<int> subscriptions;
	std::vector<int> subscribers;

	ClauseDatabaseFactory::initialize(
		__globalParameters__.maxClauseSize, __globalParameters__.globalSharedLiterals * 10, 2, 1);

	std::shared_ptr<ClauseDatabase> gsharedDB =
		ClauseDatabaseFactory::createDatabase(__globalParameters__.globalSharingDB.at(0));

	switch (strategyNumber) {
		case 0:
			LOGWARN("For now, gshr-strat at 0 is AllGatherSharing, a future default one is in dev");
		case 1:
			LOG0("GSTRAT>> AllGatherSharing");
			globalStrategies.emplace_back(new AllGatherSharing(gsharedDB,
															   __globalParameters__.globalSharedLiterals));
			break;
		case 2:
			LOG0("GSTRAT>> MallobSharing");
			globalStrategies.emplace_back(new MallobSharing(gsharedDB,
															__globalParameters__.globalSharedLiterals,
															__globalParameters__.mallobMaxBufferSize,
															__globalParameters__.mallobLBDLimit,
															__globalParameters__.mallobSizeLimit,
															__globalParameters__.mallobSharingsPerSecond,
															__globalParameters__.mallobMaxCompensation,
															__globalParameters__.mallobResharePeriod));
			break;
		case 3:
			LOG0("GSTRAT>> GenericGlobalSharing As RingSharing");
			subscriptions.push_back(right_neighbor);
			subscriptions.push_back(left_neighbor);
			subscribers.push_back(right_neighbor);
			subscribers.push_back(left_neighbor);

			globalStrategies.emplace_back(new GenericGlobalSharing(gsharedDB,
																   subscriptions,
																   subscribers,
																   __globalParameters__.globalSharedLiterals));
			break;
		default:
			LOGERROR("Global Strategy %d is not defined", strategyNumber);
			std::abort();
			break;
	}

	/*Since it is bootstraping, loop is not optimzed*/
	for (unsigned int i = 0; i < globalStrategies.size() && dist; i++) {
		if (!globalStrategies.at(i)->initMpiVariables()) {
			LOGERROR("The global sharing strategy %d wasn't able to initalize its MPI variables", i);
			TESTRUNMPI(MPI_Finalize());
			dist = false;
			globalStrategies.erase(globalStrategies.begin() + i);
		}
	}

	SharingStrategyFactory::selectedGlobal = strategyNumber;
}

void
SharingStrategyFactory::launchSharers(std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies,
									  std::vector<std::unique_ptr<Sharer>>& sharers)
{
	if (__globalParameters__.oneSharer) {
		sharers.emplace_back(new Sharer(0, sharingStrategies));
	} else {
		for (unsigned int i = 0; i < sharingStrategies.size(); i++) {
			sharers.emplace_back(new Sharer(i, sharingStrategies[i]));
		}
	}
}

void
SharingStrategyFactory::addEntitiesToLocal(std::vector<std::shared_ptr<SharingStrategy>>& localStrategies,
										   std::vector<std::shared_ptr<SolverCdclInterface>>& newSolvers)
{
	switch (SharingStrategyFactory::selectedLocal) {
		case 1:
		case 4:
		case 5:
			LOG0("UPDATE>> 1Grp");
			for (auto newSolver : newSolvers) {
				localStrategies[0]->addClient(newSolver);
				localStrategies[0]->addProducer(newSolver);
				localStrategies[0]->connectProducer(newSolver);
			}
			break;
		case 2:
		case 3:
			LOG0("UPDATE>> 2Grp of producers");

			for (unsigned int i = 0; i < newSolvers.size() / 2; i++) {
				localStrategies[0]->addClient(newSolvers[i]);
				localStrategies[0]->addProducer(newSolvers[i]);
				localStrategies[0]->connectProducer(newSolvers[i]);

				localStrategies[1]->addClient(newSolvers[i]);
			}

			for (unsigned int i = newSolvers.size() / 2; i < newSolvers.size(); i++) {
				localStrategies[0]->addClient(newSolvers[i]);

				localStrategies[1]->addProducer(newSolvers[i]);
				localStrategies[1]->connectProducer(newSolvers[i]);
				localStrategies[1]->addClient(newSolvers[i]);
			}
			break;
		default:
			LOGWARN("The sharing strategy number chosen isn't correct, use a value between 1 and %d",
					LOCAL_SHARING_STRATEGY_COUNT);
			break;
	}
}
