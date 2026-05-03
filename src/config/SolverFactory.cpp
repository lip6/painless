#include "config/SolverFactory.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/System.hpp"
#include "utils/StringUtils.hpp"

#include "solvers/CDCL/Cadical.hpp"
#include "solvers/CDCL/GlucoseSyrup.hpp"
#include "solvers/CDCL/Kissat.hpp"
#include "solvers/CDCL/Lingeling.hpp"
#include "solvers/CDCL/MapleCOMSPSSolver.hpp"
#include "solvers/CDCL/MiniSat.hpp"
#include "solvers/LocalSearch/TaSSAT.hpp"
#include "solvers/LocalSearch/YalSAT.hpp"

#include "config/ClauseDatabaseFactory.hpp"

#include <cassert>
#include <map>
#include <random>

std::shared_ptr<SolverCDCLInterface>
SolverFactory::createCDCLSolver(uint id,
                                const std::string& originalName,
                                std::shared_ptr<ClauseDatabase> importDB,
                                FullClauseReader& fullReader)
{
  std::shared_ptr<SolverCDCLInterface> createdSolver = nullptr;
  std::string name = pl::str::toLower(originalName);

  LOGD1("Creating Solver %s%d, importDB %s",
        name.c_str(),
        id,
        typeid(*importDB).name());

  if (name.empty()) {
    PABORT(PERR_UNKNOWN_SOLVER, "Empty name");
  }
#ifdef GLUCOSE_
  else if (name == "glucosesyrup") {
    createdSolver = std::make_shared<GlucoseSyrup>(id, importDB, fullReader);
  }
#endif

#ifdef LINGELING_
  else if (name == "lingeling") {
    createdSolver = std::make_shared<Lingeling>(id, importDB, fullReader);
  }
#endif

#ifdef MAPLECOMSPS_
  else if (name == "maplecomsps") {
    createdSolver =
      std::make_shared<MapleCOMSPSSolver>(id, importDB, fullReader);
  }
#endif

#ifdef MINISAT_
  else if (name == "minisat") {
    createdSolver = std::make_shared<MiniSat>(id, importDB, fullReader);
  }
#endif

#ifdef KISSAT_
  else if (name == "kissat") {
    createdSolver = std::make_shared<Kissat>(id, importDB, fullReader);
  }
#endif

#ifdef CADICAL_
  else if (name == "cadical") {
    createdSolver = std::make_shared<Cadical>(id, importDB, fullReader);
  }
#endif
  else {
    PABORT(PERR_UNKNOWN_SOLVER,
           "The SolverCDCLType %s specified is not available!",
           name.c_str());
  }

  return createdSolver;
}

std::shared_ptr<LocalSearchInterface>
SolverFactory::createLocalSearcher(uint id,
                                   const std::string& originalName,
                                   FullClauseReader& fullReader)
{
  std::shared_ptr<LocalSearchInterface> createdSolver = nullptr;
  std::string name = pl::str::toLower(originalName);

  LOGD1("Creating Solver %s%d", name.c_str(), id);

  if (name.empty()) {
    PABORT(PERR_UNKNOWN_SOLVER, "Empty name");
  }
#ifdef YALSAT_
  else if (name == "yalsat") {
    createdSolver = std::make_shared<YalSAT>(id, fullReader);
  }
#endif

#ifdef TASSAT_
  else if (name == "tassat") {
    createdSolver = std::make_shared<TaSSAT>(id, fullReader);
  }
#endif

  return createdSolver;
}

SolverFactory::SolverFactory(const Parameters& parameters)
  : m_currentIdSolver(0)
  , m_parameters(parameters)
  , m_clauseDBFactory(parameters.maxClauseSize,
                      parameters.importDBCap,
                      parameters.mallobDBPartitionLBD,
                      parameters.mallobDBFreeSize,
                      parameters.defaultClauseBufferSize)
{
}

std::shared_ptr<SolverInterface>
SolverFactory::createSolver(const char type,
                            std::shared_ptr<ClauseDatabase> importDB,
                            FullClauseReader& fullReader)
{
  int id = m_currentIdSolver.fetch_add(1);
  LOGD1("Creating Solver %d, type %c, importDB %s",
        id,
        type,
        typeid(*importDB).name());

  if (id >= m_parameters.cpus) {
    LOGWARN("Solver of type '%c' will not be instantiated, the number of "
            "solvers %d reached the maximum %d.",
            type,
            id,
            m_parameters.cpus);
    return nullptr;
  }

  std::shared_ptr<SolverInterface> createdSolver;
  SolverInterface::Type createdType;

  switch (type) {
#ifdef GLUCOSE_
    case 'g':
      createdSolver = std::make_shared<GlucoseSyrup>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef LINGELING_
    case 'l':
      createdSolver = std::make_shared<Lingeling>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef MAPLECOMSPS_
    case 'M':
      createdSolver =
        std::make_shared<MapleCOMSPSSolver>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef MINISAT_
    case 'm':
      createdSolver = std::make_shared<MiniSat>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef KISSAT_INC_
    case 'I':
      createdSolver =
        std::make_shared<KissatINCSolver>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef KISSAT_MAB_
    case 'K':
      createdSolver =
        std::make_shared<KissatMABSolver>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef KISSAT_
    case 'k':
      createdSolver = std::make_shared<Kissat>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef CADICAL_
    case 'c':
      createdSolver = std::make_shared<Cadical>(id, importDB, fullReader);
      createdType = SolverInterface::Type::CDCL;
      break;
#endif

#ifdef YALSAT_
    case 'y':
      createdSolver = std::make_shared<YalSAT>(id,
                                               m_parameters.localSearchFlips,
                                               m_parameters.maxDivNoise,
                                               fullReader);
      createdType = SolverInterface::Type::LOCAL_SEARCH;
      break;
#endif

#ifdef TASSAT_
    case 't':
      createdSolver = std::make_shared<TaSSAT>(id,
                                               m_parameters.localSearchFlips,
                                               m_parameters.maxDivNoise,
                                               fullReader);
      createdType = SolverInterface::Type::LOCAL_SEARCH;
      break;
#endif

    default:
      PABORT(PERR_UNKNOWN_SOLVER,
             "The SolverCDCLType %c specified is not available!",
             type);
  }

  return createdSolver;
}

void
SolverFactory::createSolvers(
  int maxSolvers,
  char importDBType,
  const std::string& portfolio,
  FullClauseReader& fullReader,
  std::vector<std::shared_ptr<SolverInterface>>& solvers)
{
  unsigned int typeCount = portfolio.size();
  LOGD1("Portfolio is '%s', of size %u", portfolio.c_str(), typeCount);
  std::shared_ptr<SolverInterface> createdSolver;
  for (size_t i = 0; i < maxSolvers && typeCount > 0; i++) {
    createdSolver = createSolver(portfolio.at(i % typeCount),
                                 m_clauseDBFactory.createDatabase(importDBType),
                                 fullReader);
    if (createdSolver != nullptr)
      solvers.push_back(createdSolver);
  }
}