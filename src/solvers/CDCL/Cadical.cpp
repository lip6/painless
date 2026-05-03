#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include <cassert>
#include <iomanip>

#include "Cadical.hpp"
#include "cadical/src/stats.hpp"

/*----------------------Main Class------------------------*/

Cadical::Cadical(int id,
                 const std::shared_ptr<ClauseDatabase>& clauseDB,
                 FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::CADICAL)
  , m_clausesToImport(clauseDB)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
{
  solver = std::make_unique<CaDiCaL::Solver>();
  solver->connect_learner(this);
  solver->connect_terminator(this);
  this->initCadicalOptions();

  initializeTypeId<Cadical>();
}

Cadical::~Cadical()
{
  solver->terminate(); /* just in case */
  solver->disconnect_learner();
  solver->disconnect_terminator();
}

// ==================================================================
// Execution
// ==================================================================

SatAnswer
Cadical::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  /* use add to add unit clauses for permanent assumption */
  for (int lit : cube)
    solver->assume(lit);

  // If solver was called previously -> No saved phase diversfication
  if (!this->m_initialized) {
    std::mt19937 engine;
    std::bernoulli_distribution dist;
    for (uint i = 1; i <= this->getVariableCount(); i++) {
      bool val = dist(engine);
      // LOGD1("Variable %u is set to %u", i, static_cast<uint>(val));
      this->setPhase(i, val);
    }
  }

  this->m_initialized = true;

  LOG2("Launching CaDiCaL %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  int res = solver->solve();

  if (res == 10) {
    LOG2("Cadical %d responded with SAT", this->getSolverId());
    return SatAnswer::SAT;
  }
  if (res == 20) {
    LOG2("Cadical %d responded with UNSAT", this->getSolverId());
    return SatAnswer::UNSAT;
  }
  LOGD2("Cadical %d responded with %d (UNKNOWN)", this->getSolverId(), res);
  return SatAnswer::UNKNOWN;
}

void
Cadical::setSolverInterrupt()
{
  this->stopSolver = true;
  LOGD1("Asking Cadical (%d, %u) to end",
        this->getSolverId(),
        this->getSolverTypeId());
}

void
Cadical::unsetSolverInterrupt()
{
  LOGD1("Making Cadical (%d, %u) able to resume",
        this->getSolverId(),
        this->getSolverTypeId());
  this->stopSolver = false;
}

void
Cadical::initCadicalOptions()
{
#ifndef NDEBUG
  this->setOption("quiet", 0);
#else
  this->setOption("quiet", 1);
#endif
  this->setOption("stabilize", 1);
  this->setOption("stabilizeonly", 0);
  /*
   * target = 1: in stable phase only
   * target = 2: always choose target
   */
  this->setOption("target", 1);

  // Shuffling
  this->setOption("shuffle", 0);       /* shuffle variables */
  this->setOption("shufflequeue", 0);  /* shuffle variable queue */
  this->setOption("shufflescores", 0); /* shuffle variable queue */
  this->setOption("shufflerandom", 0);

  // Random Walks
  this->setOption("walkredundant", 0);
  this->setOption("walknonstable", 1);
  this->setOption("walk", 1);

  // Search Configuration
  this->setOption("chrono", 1);
  this->setOption("chronoalways", 0);
  this->setOption("chronolevelim", 100);

  // Restart Management
  this->setOption("restart", 1);
  this->setOption("restartint", 1);

  // Decision
  this->setOption("score", 1); // 1: VSIDS, 0: no score computing

  // Phase
  this->setOption("phase", 1);
  this->setOption("rephase", 1);
  this->setOption("rephaseint", 1e3);
  this->setOption("forcephase", 0);

  // Simplification Techniques
  this->setOption("block", 0);      /* Blocked clause elimination */
  this->setOption("elim", 0);       /* Bounded Variable elimination */
  this->setOption("factor", 0);     /* Bounded Variable Addition */
  this->setOption("congruence", 0); /* Congruence Closure (LE or E ?) */
  this->setOption("sweep", 0);      /* SAT Sweeping (LE or E ?) */
  this->setOption("otfs", 1);       /* On the fly subsumption */
  this->setOption("condition", 0);  /* Globally blocked clause elimination */
  this->setOption("cover", 0);      /* Covered clause elimination */
  this->setOption(
    "inprocessing",
    1); /* Enable inprocessing (search is stopped, simplification is resumed)*/

  // Learnt Clauses
  this->setOption("reducetier1glue", 2);
  this->setOption("reducetier2glue", 6);

  // Random seed
  this->setOption("seed", this->getSolverTypeId());
}

void
Cadical::diversify(const SeedGenerator& getSeed)
{
  unsigned int typeId = this->getSolverTypeId();
  unsigned int generalSeed = getSeed(this);

  unsigned int cadicalCount = this->getSolverTypeCount();

  this->setOption("seed", generalSeed);

  this->setOption("elim", 0);
  this->setOption("factor", 0);
  this->setOption("congruence", 1);
  this->setOption("sweep", 1);
  this->setOption("condition", 0);

  // if (typeId == 0)
  // 	solver->configure("unsat");
  // else
  // 	solver->configure("sat");

  this->setOption("phase", generalSeed % 2);

  // From Mallob Native Diversification
  switch (typeId % 10) {
    case 0:
      this->setOption("phase", 0);
      break;
    case 1:
      solver->configure("sat");
      // ~1/4
      if (typeId % 4 == 0) {
        this->setOption("target", 2);
        this->setOption("chrono", 1);
        this->setOption("chronoalways", 1);
        this->setOption("walkredundant", 1);
        this->setOption("reducetier1glue", 2);
        this->setOption("reducetier2glue", 3 + generalSeed % 2);
        this->setOption("restartint", 90 + (1 + typeId % 10));
      }
      break;
    case 2:
      this->setOption("elim", 0);
      break;
    case 3:
      solver->configure("unsat");
      this->setOption("restartint", 1);
      if (typeId % 4 == 0) {
        this->setOption("walk", 0);
        this->setOption("target", 0);
        this->setOption("chrono", 0);
      }
      break;
    case 4:
      this->setOption("condition", 1);
      break;
    case 5:
      this->setOption("walk", 0);
      break;
    case 6:
      this->setOption("restartint", 100);
      break;
    case 7:
      this->setOption("cover", 1);
      break;
    case 8:
      this->setOption("shuffle", 1);
      this->setOption("shufflerandom", 1);
      break;
    case 9:
      this->setOption("inprocessing", 0);
      break;
  }
}

void
Cadical::setOption(const std::string& key, int value)
{
  PABORTIF(!solver->set(key.c_str(), value),
           PERR_ARGS,
           "Option %s is not recognized by CaDiCaL!",
           key.c_str());
}

// ==================================================================
// Clause Management
// ==================================================================

void
Cadical::loadFormula(const char* filename)
{
  int nbVars;
  int strict = 2;
  solver->read_dimacs(filename, nbVars, strict);
  m_initialized = true;
}

bool
Cadical::addClause(clause_view_t clause)
{
  LOGDVECTOR4(clause.data(),
              clause.size(),
              "Adding to CaDiCaL %u:",
              this->getSolverTypeId());
  solver->clause(clause.data(), clause.size());

  return true;
}

uint
Cadical::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

// ==================================================================
// Sharing
// ==================================================================

bool
Cadical::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0);
  m_clausesToImport->addClause(clause);
  return true;
}

// Learner
// =======

bool
Cadical::learning(int size, int glue)
{
  if (size > 0) {
    LOGD4("Cadical %d will export clause of size %d, glue %d",
          this->getSolverId(),
          size,
          glue);
    m_tempClause.reserve(size);
    m_tempLbd = glue;
    return true;
  } else {
    return false;
  }
}

void
Cadical::learn(int lit)
{
  if (lit)
    m_tempClause.push_back(lit);
  else {
    assert(m_tempClause.size() > 0 && m_tempLbd >= 0);
    auto exportedClause =
      ClauseExchange::create(m_tempClause, m_tempLbd, this->getSharingId());

    assert(m_tempClause.size() == exportedClause->size);
    assert((exportedClause->size > 1 && exportedClause->lbd > 0) ||
           (exportedClause->size == 1 && exportedClause->lbd >= 0));

    /* filtering defined by a sharing strategy, done here in case it checks its
     * literals */
    if (this->exportClause(exportedClause)) {
      LOGDVECTOR4(exportedClause->lits,
                  exportedClause->size,
                  "Cadical %d exported Clause %p for sharing",
                  this->getSolverId(),
                  exportedClause.get());
    }
    m_tempClause.clear();
  }
}

bool
Cadical::hasClauseToImport()
{
  /* Always import any new clause added to painless to the importDB with the tag
   * ORIGINAL for the .from attribute*/
  // uint oldIndex = m_fullReaderIndex;
  // m_fullReaderIndex = mcbk_fullReader(
  // 	[this](clause_view_t clause) -> bool {
  // 		if (!this->importClause(ClauseExchange::create(clause, 0,
  // ClauseExchange::ORIGINAL))) {
  // PABORT(PERR_BAD_BEHAVIOR, "An irredundant clause was not able to be
  // imported");
  // 		}
  // 		return true;
  // 	},
  // 	m_fullReaderIndex);
  // // m_fullReaderIndex is now equal to the number of clauses read in total

  // if ((m_fullReaderIndex - oldIndex) > 0)
  // 	LOGD2("CaDiCaL %u added %u irredundant clauses to its importDB",
  // 		 getSolverTypeId(),
  // 		 (m_fullReaderIndex - oldIndex));

  if (this->m_clausesToImport->getOneClause(m_tempClauseToImport)) {
    if (m_tempClauseToImport->lbd)
      LOGD4("Cadical %u will import redundant clause %s",
            this->getSharingId(),
            m_tempClauseToImport->toString().c_str());
    else
      LOGD4("Cadical %u will import irredundant clause %s",
            this->getSharingId(),
            m_tempClauseToImport->toString().c_str());
    return true;
  } else {
    m_clausesToImport->shrinkDatabase();
    return false;
  }
}

void
Cadical::getClauseToImport(std::vector<int>& clause, int& glue)
{
  assert(clause.empty());
  clause.insert(
    clause.end(), m_tempClauseToImport->begin(), m_tempClauseToImport->end());
  assert(clause.size() && clause.size() == m_tempClauseToImport->size &&
         clause[0] == m_tempClauseToImport->lits[0]);

  glue = m_tempClauseToImport->lbd;
  LOGDVECTOR4(clause.data(),
              clause.size(),
              "Cadical %d will import Clause (lbd:%u)",
              this->getSolverId(),
              glue);
}

// ==================================================================
// Variable Management
// ==================================================================

uint
Cadical::getVariableCount()
{
  return solver->vars();
}

var_t
Cadical::getDivisionVariable()
{
  return (rand() % getVariableCount()) + 1;
}

void
Cadical::setPhase(const var_t var, const bool phase)
{
  solver->savePhase((phase) ? var : -var);
}

void
Cadical::bumpVariableActivity(const var_t var, const int times)
{
  LOGERROR("Not Implemented, yet");
  exit(PERR_NOT_SUPPORTED);
}

// ==================================================================
// Result & Solution
// ==================================================================

clause_t
Cadical::getFinalAnalysis()
{
  LOGERROR("Not Implemented, yet");
  exit(PERR_NOT_SUPPORTED);
}

cube_t
Cadical::getSatAssumptions()
{
  LOGERROR("Not Implemented, yet");
  exit(PERR_NOT_SUPPORTED);
}

model_t
Cadical::getModel()
{
  std::vector<int> model;
  unsigned int maxVar = this->getVariableCount();

  for (unsigned int i = 1; i <= maxVar; i++) {
    int tmp = solver->val(i);
    if (!tmp)
      tmp = i;
    model.push_back(tmp);
  }

  return model;
}

// ==================================================================
// Statistics
// ==================================================================

std::string
Cadical::statisticsToString()
{
  std::ostringstream oss;
  /* TODO get with prefix instead of print */
  CaDiCaL::Stats* cstats = solver->getStatistics();
  SolverCDCLInterface::Statistics stats;

  stats.conflicts = cstats->conflicts;
  stats.propagations = cstats->propagations.search;
  stats.restarts = cstats->restarts;
  stats.decisions = cstats->decisions;

  oss << stats.toString(getSolverId(), "CaDiCaL");

  return oss.str();
}
