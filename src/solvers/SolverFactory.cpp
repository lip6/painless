#include "solvers/SolverFactory.hpp"
#include "painless.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"

#include "solvers/CDCL/Cadical.hpp"
#include "solvers/CDCL/GlucoseSyrup.hpp"
#include "solvers/CDCL/Kissat.hpp"
#include "solvers/CDCL/KissatINCSolver.hpp"
#include "solvers/CDCL/KissatMABSolver.hpp"
#include "solvers/CDCL/Lingeling.hpp"
#include "solvers/CDCL/MapleCOMSPSSolver.hpp"
#include "solvers/CDCL/MiniSat.hpp"
#include "solvers/LocalSearch/YalSat.hpp"

#include "containers/ClauseDatabaseMallob.hpp"
#include "containers/ClauseDatabasePerSize.hpp"
#include "containers/ClauseDatabaseSingleBuffer.hpp"

#include <cassert>
#include <iomanip>
#include <map>
#include <random>

std::atomic<int> SolverFactory::currentIdSolver(0);

void
SolverFactory::diversification(const std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
							   const std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers,
							   const IDScaler& gIDScaler,
							   const IDScaler& typeIDScaler)
{
	for (auto cdclSolver : cdclSolvers) {
		cdclSolver->setSolverId(gIDScaler(cdclSolver));
		cdclSolver->setSolverTypeId(typeIDScaler(cdclSolver));
		LOGDEBUG1("Computing Id %d,%d", cdclSolver->getSolverId(), cdclSolver->getSolverTypeId());
	}

	for (auto localSolver : localSolvers) {
		localSolver->setSolverId(gIDScaler(localSolver));
		localSolver->setSolverTypeId(typeIDScaler(localSolver));
	}

	for (auto cdclSolver : cdclSolvers) {
		cdclSolver->diversify();
	}

	for (auto localSolver : localSolvers) {
		localSolver->diversify();
	}

	LOG0("Diversification done");
}

SolverAlgorithmType
SolverFactory::createSolver(char type, char importDBType, std::shared_ptr<SolverInterface>& createdSolver)
{
	int id = currentIdSolver.fetch_add(1);
	LOGDEBUG1("Creating Solver %d, type %c, importDB %c", id, type, importDBType);

	if (id >= __globalParameters__.cpus) {
		LOGWARN("Solver of type '%c' will not be instantiated, the number of solvers %d reached the maximum %d.",
				type,
				id,
				__globalParameters__.cpus);
		return SolverAlgorithmType::UNKNOWN;
	}

	std::shared_ptr<ClauseDatabase> importDB;
	uint maxClauseSize = __globalParameters__.maxClauseSize;
	uint maxPartitioningLbd = 2;
	uint maxLiteralCapacity = __globalParameters__.importDBCap;
	uint maxFreeSize = 1;

	switch (importDBType) {
		case 's':
			importDB = std::make_shared<ClauseDatabaseSingleBuffer>(__globalParameters__.defaultClauseBufferSize);
			break;
		case 'm':
			importDB = std::make_shared<ClauseDatabaseMallob>(
				maxClauseSize, maxPartitioningLbd, maxLiteralCapacity, maxFreeSize);
			break;
		case 'd':
		default:
			importDB = std::make_shared<ClauseDatabasePerSize>(maxClauseSize);
			break;
	}

	switch (type) {
#ifdef GLUCOSE_
		case 'g':
			createdSolver = std::make_shared<GlucoseSyrup>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef LINGELING_
		case 'l':
			createdSolver = std::make_shared<Lingeling>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef MAPLECOMSPS_
		case 'M':
			createdSolver = std::make_shared<MapleCOMSPSSolver>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef MINISAT_
		case 'm':
			createdSolver = std::make_shared<MiniSat>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef KISSAT_INC_
		case 'I':
			createdSolver = std::make_shared<KissatINCSolver>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef KISSAT_MAB_
		case 'K':
			createdSolver = std::make_shared<KissatMABSolver>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef KISSAT_
		case 'k':
			createdSolver = std::make_shared<Kissat>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef CADICAL_
		case 'c':
			createdSolver = std::make_shared<Cadical>(id, importDB);
			return SolverAlgorithmType::CDCL;
			// break;
#endif

#ifdef YALSAT_
		case 'y':
			createdSolver = std::make_shared<YalSat>(id, __globalParameters__.localSearchFlips, __globalParameters__.maxDivNoise);
			return SolverAlgorithmType::LOCAL_SEARCH;
			// break;
#endif

		default:
			LOGERROR("The SolverCdclType %c specified is not available!", type);
			exit(PERR_UNKNOWN_SOLVER);
	}
}

SolverAlgorithmType
SolverFactory::createSolver(char type,
							char importDBType,
							std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
							std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers)
{
	std::shared_ptr<SolverInterface> createdSolver;
	switch (createSolver(type, importDBType, createdSolver)) {
		case SolverAlgorithmType::CDCL:
			cdclSolvers.push_back(std::static_pointer_cast<SolverCdclInterface>(createdSolver));
			return SolverAlgorithmType::CDCL;

		case SolverAlgorithmType::LOCAL_SEARCH:
			localSolvers.push_back(std::static_pointer_cast<LocalSearchInterface>(createdSolver));
			return SolverAlgorithmType::LOCAL_SEARCH;

		default: /* TODO add preprocessor case */
			return SolverAlgorithmType::UNKNOWN;
	}
}

void
SolverFactory::createSolvers(int maxSolvers,
							 char importDBType,
							 std::string portfolio,
							 std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
							 std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers)
{
	unsigned int typeCount = portfolio.size();
	LOGDEBUG1("Portfolio is '%s', of size %u", portfolio.c_str(), typeCount);
	for (size_t i = 0; i < maxSolvers && typeCount > 0; i++) {
		createSolver(portfolio.at(i % typeCount), importDBType, cdclSolvers, localSolvers);
	}
}

void
SolverFactory::printStats(const std::vector<std::shared_ptr<SolverCdclInterface>>& cdclSolvers,
						  const std::vector<std::shared_ptr<LocalSearchInterface>>& localSolvers)
{
	lockLogger();
	// Print header
	std::cout << std::string(93, '-') << "\n";
	std::cout << std::left << std::setw(15) << "| ID" << std::setw(20) << "| Conflicts" << std::setw(20)
			  << "| Propagations" << std::setw(17) << "| Restarts" << std::setw(20) << "| Decisions"
			  << "|\n";

	std::cout << std::string(93, '-') << "\n";

	for (auto s : cdclSolvers) {
		s->printStatistics();
	}
	// printf("*Kissat(K) statistics says that the number of conflicts and propagations is per second, while the number
	// "
	//    "of decisions is per conflict\n\n");
	unlockLogger();
}
