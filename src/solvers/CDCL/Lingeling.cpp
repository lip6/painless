#include "Lingeling.hpp"
#include "utils/ErrorCodes.hpp"

#include <cassert>
#include <iomanip>

// Include lingeling
extern "C"
{
#include "lingeling/lglib.h"
}

#include <ctype.h>

int
termCallback(void* solverPtr)
{
  Lingeling* lp = (Lingeling*)solverPtr;
  return lp->stopSolver;
}

void
produceUnit(void* sp, int lit)
{
  Lingeling* lp = (Lingeling*)sp;

  // Create new clause
  ClauseExchangePtr ncls = ClauseExchange::create(1, 0, lp->getSharingId());

  ncls->lits[0] = lit;

  LOGD4("Lingeling %u produced a unit : %s",
        lp->getSolverTypeId(),
        ncls->toString().c_str());

  /* filtering defined by a sharing strategy */
  lp->exportClause(ncls);
}

void
produce(void* sp, int* cls, int glue)
{
  // If unit clause, call produceUnit
  if (cls[1] == 0) {
    produceUnit(sp, cls[0]);
    return;
  }

  Lingeling* lp = (Lingeling*)sp;

  // Create new clause
  int size = 0;
  while (cls[size] != 0) {
    size++;
  }

  ClauseExchangePtr ncls =
    ClauseExchange::create(size, glue, lp->getSharingId());

  memcpy(ncls->lits, cls, sizeof(int) * size);

  LOGD4("Lingeling %u produced: %s",
        lp->getSolverTypeId(),
        ncls->toString().c_str());

  /* filtering defined by a sharing strategy */
  lp->exportClause(ncls);
}

void
consumeUnits(void* sp, int** start, int** end)
{
  Lingeling* lp = (Lingeling*)sp;

  // used a vector to a get a constant state of the units lockfree queue (to
  // check if worth it)
  std::vector<int> tmp;

  lp->m_unitsToImport.consume_all([&tmp](int unit) { tmp.push_back(unit); });

  LOGD2("Lingeling %u will assign %u units", lp->getSolverTypeId(), tmp.size());

  if (tmp.empty()) {
    *start = lp->unitsBuffer;
    *end = lp->unitsBuffer;
    return;
  }

  if (tmp.size() >= lp->unitsBufferSize) {
    lp->unitsBufferSize = 1.6 * tmp.size();
    lp->unitsBuffer =
      (int*)realloc((void*)lp->unitsBuffer, lp->unitsBufferSize * sizeof(int));
  }

  for (size_t i = 0; i < tmp.size(); i++) {
    lp->unitsBuffer[i] = tmp[i];
  }

  *start = lp->unitsBuffer;
  *end = *start + tmp.size();
}

void
consumeCls(void* sp, int** clause, int* glue)
{
  Lingeling* lp = (Lingeling*)sp;

  ClauseExchangePtr cls;

  if (lp->m_clausesToImport->getOneClause(cls) == false) {
    *clause = NULL;
    lp->m_clausesToImport->shrinkDatabase();
    return;
  }

  if (cls->size + 1 >= lp->clsBufferSize) {
    lp->clsBufferSize = 1.6 * cls->size;
    lp->clsBuffer =
      (int*)realloc((void*)lp->clsBuffer, lp->clsBufferSize * sizeof(int));
  }

  if (cls->lbd)
    LOGD4("Lingeling %u will import redundant clause %s",
          lp->getSharingId(),
          cls->toString().c_str());
  else
    LOG2("Lingeling %u will import irredundant clause %s",
         lp->getSharingId(),
         cls->toString().c_str());

  *glue = cls->lbd;

  memcpy(lp->clsBuffer, cls->lits, sizeof(int) * cls->size);
  lp->clsBuffer[cls->size] = 0;

  *clause = lp->clsBuffer;
}

void
Lingeling::initialize()
{
  // BCA has to be disabled for valid clause sharing (or freeze all literals)
  lglsetopt(solver, "bca", 0);
  // Based on mallob suggestion
  // Sync (i.e., export) unit clauses more frequently
  lglsetopt(solver, "syncunint", 11111); // down from 111'111

  stopSolver = false;
  unitsBufferSize = clsBufferSize = 100;
  unitsBuffer = (int*)malloc(unitsBufferSize * sizeof(int));
  clsBuffer = (int*)malloc(clsBufferSize * sizeof(int));
  lglsetproducecls(solver, produce, this);
  lglsetproduceunit(solver, produceUnit, this);
  lglsetconsumeunits(solver, consumeUnits, this);
  lglsetconsumecls(solver, consumeCls, this);
  lglseterm(solver, termCallback, this);
  initializeTypeId<Lingeling>();
}

Lingeling::Lingeling(int id,
                     const std::shared_ptr<ClauseDatabase>& clauseDB,
                     FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::LINGELING)
  , m_clausesToImport(clauseDB)
  , m_unitsToImport(256)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
{
  solver = lglinit();
  initialize();
}

Lingeling::~Lingeling()
{
  lglrelease(solver);

  free(unitsBuffer);
  free(clsBuffer);
}

void
Lingeling::loadFormula(const char* filename)
{
  std::vector<clause_t> initClauses;
  unsigned int varCount = 0;
  if (!Parsers::parseCNF(filename, initClauses, &varCount)) {
    PABORT(PERR_PARSING, "Error at parsing!");
  }

  for (auto clause : initClauses) {
    addClause(clause);
  }

  lglsimp(solver, 10);
}

bool
Lingeling::addClause(clause_view_t clause)
{
  for (lit_t lit : clause)
    lgladd(solver, lit);
  lgladd(solver, 0);

  return true;
}

uint
Lingeling::getVariableCount()
{
  return lglnvars(solver);
}

// Get a variable suitable for search splitting
var_t
Lingeling::getDivisionVariable()
{
  lglsimp(solver, 1);

  int oldjwhred = lglgetopt(solver, "jwhred");

  int lit = lglookahead(solver);

  lglsetopt(solver, "jwhred", oldjwhred);

  return lit;
}

// Set initial phase for a given variable
void
Lingeling::setPhase(const var_t var, const bool phase)
{
  lglsetphase(solver, phase ? var : -var);
}

// Bump activity for a given variable
void
Lingeling::bumpVariableActivity(const var_t var, const int times)
{
  for (int i = times; i < times; i++) {
    // TODO: is this correct ?
    // var must be positif otherwise phase will be set to fale
    lglsetimportant(solver, var);
  }
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
Lingeling::setSolverInterrupt()
{
  LOGD1("Asking Lingeling (%d, %u) to end",
        this->getSolverId(),
        this->getSolverTypeId());
  stopSolver = true;
}

void
Lingeling::unsetSolverInterrupt()
{
  stopSolver = false;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatAnswer
Lingeling::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  // set the assumptions
  for (size_t i = 0; i < cube.size(); i++) {
    // freezing problems ???
    if (lglusable(solver, cube[i])) {
      lglassume(solver, cube[i]);
    }
  }

  this->m_initialized = true;

  LOG2("Launching Lingeling %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  // Simplify the problem
  int res = lglsimp(solver, 0);

  switch (res) {
    case LGL_SATISFIABLE:
      LOG2("Lingeling Simp %d responded with SAT", this->getSolverId());
      return SatAnswer::SAT;
    case LGL_UNSATISFIABLE:
      LOG2("Lingeling Simp %d responded with UNSAT", this->getSolverId());
      return SatAnswer::UNSAT;
  }

  // Solve the problem
  res = lglsat(solver);

  switch (res) {
    case LGL_SATISFIABLE:
      LOG2("Lingeling Sat %d responded with SAT", this->getSolverId());
      return SatAnswer::SAT;
    case LGL_UNSATISFIABLE:
      LOG2("Lingeling Sat %d responded with UNSAT", this->getSolverId());
      return SatAnswer::UNSAT;
  }

  return SatAnswer::UNKNOWN;
}

// Add a learned clause to the formula
bool
Lingeling::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0);

  if (clause->size == 1) {
    m_unitsToImport.push(clause->lits[0]);
  } else {
    m_clausesToImport->addClause(clause);
  }

  return true;
}

uint
Lingeling::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

std::string
Lingeling::statisticsToString()
{
  std::ostringstream oss;
  SolverCDCLInterface::Statistics stats;

  stats.conflicts = lglgetconfs(solver);
  stats.decisions = lglgetdecs(solver);
  stats.propagations = lglgetprops(solver);
  stats.memPeak = lglmaxmb(solver);
  stats.restarts = lglgetrestarts(solver);

  oss << stats.toString(getSolverId(), "Lingeling");
  return oss.str();
}

void
Lingeling::diversify(const SeedGenerator& getSeed)
{
  int id = this->getSolverTypeId();
  lglsetopt(solver, "seed", id);
  lglsetopt(solver, "classify", 0);
  lglsetopt(solver, "phase", (id % 2) ? 1 : -1);
  switch (id % 10) {
    case 0:
      lglsetopt(solver, "gluescale", 5);
      break; // from 3 (value "ld" moved)
    case 1:
      lglsetopt(solver, "plain", 1);
      lglsetopt(solver, "decompose", 1);
      break;
    case 2:
      lglsetopt(solver, "restartint", 100);
      break;
    case 3:
      lglsetopt(solver, "sweeprtc", 1);
      break;
    case 4:
      lglsetopt(solver, "restartint", 1000);
      break;
    case 5:
      lglsetopt(solver, "scincinc", 50);
      break;
    case 6:
      lglsetopt(solver, "restartint", 4);
      break;
    case 7:
      lglsetopt(solver, "phase", 1);
      break;
    case 8:
      lglsetopt(solver, "phase", -1);
      break;
    case 9:
      lglsetopt(solver, "block", 0);
      lglsetopt(solver, "cce", 0);
      break;
  }
}

void Lingeling::setOption(const std::string& key, int value){
  lglsetopt(solver, key.c_str(), value);
}

model_t
Lingeling::getModel()
{
  std::vector<int> model;
  for (int i = 1; i <= lglmaxvar(solver); i++) {
    int lit = (lglderef(solver, i) > 0) ? i : -i;
    model.push_back(lit);
  }

  return model;
}

clause_t
Lingeling::getFinalAnalysis()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
}

cube_t
Lingeling::getSatAssumptions()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
};