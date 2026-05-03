// Glucose includes
#include "GlucoseSyrup.hpp"

#include <glucose/core/Dimacs.h>
#include <glucose/parallel/ParallelSolver.h>
#include <glucose/utils/System.h>

#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"

#include <iomanip>

// Macros to converte glucose literals
#define GLUE_LIT(lit)                                                          \
  lit > 0 ? Glucose::mkLit(lit - 1, false) : Glucose::mkLit((-lit) - 1, true)

#define INT_LIT(lit)                                                           \
  Glucose::sign(lit) ? -(Glucose::var(lit) + 1) : (Glucose::var(lit) + 1)

static bool
checkLiteral(const Glucose::Lit& l, Glucose::Solver* glucose)
{
  // Assert if not negative (error)
  assert(l.x >= 0);
  // Check if the literal is not out of bound
  if (glucose->nVars() <= var(l)) {
    LOGWARN("Literal %d is out of bound for Glucose", l.x);
    return false;
  }
  return true;
}

bool
makeGlueVec(ClauseExchangePtr cls,
            Glucose::vec<Glucose::Lit>& gcls,
            Glucose::Solver* glucose)
{
  for (size_t i = 0; i < cls->size; i++) {
    Glucose::Lit internalLit = GLUE_LIT(cls->lits[i]);
    gcls.push(internalLit);
    // Checks
    if (!checkLiteral(internalLit, glucose))
      return false;
  }

  return true;
}

void
glucoseExportUnary(void* issuer, Glucose::Lit& l)
{
  GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

  ClauseExchangePtr ncls = ClauseExchange::create(1, 1, gs->getSharingId());

  ncls->lits[0] = INT_LIT(l);

  /* filtering defined by a sharing strategy */
  gs->exportClause(ncls);
}

void
glucoseExportClause(void* issuer, Glucose::Clause& cls)
{
  GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

  ClauseExchangePtr ncls =
    ClauseExchange::create(cls.size(), cls.lbd(), gs->getSharingId());

  for (unsigned int i = 0; i < cls.size(); i++) {
    ncls->lits[i] = INT_LIT(cls[i]);
  }

  /* filtering defined by a sharing strategy */
  gs->exportClause(ncls);
}

Glucose::Lit
glucoseImportUnary(void* issuer)
{
  GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

  Glucose::Lit l;

  std::vector<int> units;

  gs->m_unitsToImport.consume_all(
    [&units](int unit) { units.push_back(unit); });

  for (lit_t lit : units) {
    l = GLUE_LIT(lit);

    if (checkLiteral(l, gs->solver) == true) {
      assert(l.x >= 0);
      LOGD2("Importing to Glucose %u unit literal %d", gs->getSharingId(), l.x);
      return l;
    }
  }
  return Glucose::lit_Undef;
}

bool
glucoseImportClause(void* issuer,
                    int* from,
                    int* lbd,
                    Glucose::vec<Glucose::Lit>& gcls)
{
  GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

  ClauseExchangePtr cls;

  while (gs->m_clausesToImport->getOneClause(cls)) {
    if (makeGlueVec(cls, gcls, gs->solver)) {
      *from = cls->from;
      *lbd = cls->lbd;

      if (cls->lbd)
        LOGD4("Glucose %u will import redundant clause %s",
              gs->getSharingId(),
              cls->toString().c_str());
      else
        LOG2("Glucose %u will import irredundant clause %s",
             gs->getSharingId(),
             cls->toString().c_str());

      assert(gcls.size() > 1 && gcls[0].x >= 0);

      return true;
    }
    gcls.clear();
  }

  gs->m_clausesToImport->shrinkDatabase();
  return false;
}

GlucoseSyrup::GlucoseSyrup(int id,
                           const std::shared_ptr<ClauseDatabase>& clauseDB,
                           FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::GLUCOSE)
  , m_clausesToImport(clauseDB)
  , m_unitsToImport(256)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , gcls(std::make_unique<Glucose::vec<Glucose::Lit>>())
{
  /* use sharing id to not have the assert(importedFromThread != thn) fail */
  solver = new Glucose::ParallelSolver(this->getSharingId());

  solver->exportUnary = glucoseExportUnary;
  solver->exportClause = glucoseExportClause;
  solver->importUnary = glucoseImportUnary;
  solver->importClause = glucoseImportClause;
  solver->issuer = this;

  initializeTypeId<GlucoseSyrup>();
}

GlucoseSyrup::~GlucoseSyrup()
{
  delete solver;
}

void
GlucoseSyrup::loadFormula(const char* filename)
{
  gzFile in = gzopen(filename, "rb");

  parse_DIMACS(in, *solver);

  gzclose(in);
}

uint
GlucoseSyrup::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

bool
GlucoseSyrup::addClause(clause_view_t clause)
{
  assert(clause.size() > 0);
  gcls->clear();
  for (lit_t lit : clause) {
    // From Minisat Parser
    uint var = abs(lit) - 1;
    while (var >= solver->nVars())
      solver->newVar();
    gcls->push(GLUE_LIT(lit));
  }

  if (solver->addClause(*gcls) == false) {
    LOGWARN("UNSAT when adding cls");
    return false;
  }

  return true;
}

// Get the number of variables of the formula
uint
GlucoseSyrup::getVariableCount()
{
  return solver->nVars();
}

// Get a variable suitable for search splitting
var_t
GlucoseSyrup::getDivisionVariable()
{
  Glucose::Lit res;

  switch (this->heuristic) {
    case 2:
      res = solver->pickBranchLitUsingFlipActivity();
      break;
    case 3:
      res = solver->pickBranchLitUsingPropagationRate();
      break;
    default:
      res = solver->pickBranchLit();
  }

  return INT_LIT(res);
}

// Set initial phase for a given variable
void
GlucoseSyrup::setPhase(const var_t var, const bool phase)
{
  solver->setPolarity(var - 1, phase);
}

// Bump activity for a given variable
void
GlucoseSyrup::bumpVariableActivity(const var_t var, const int times)
{
  for (int i = 0; i < times; i++) {
    solver->varBumpActivity(var - 1);
  }
}

// Diversify the solver
void
GlucoseSyrup::diversify(const SeedGenerator& getSeed)
{
  unsigned int id = this->getSolverTypeId();
  int idMod = id ? id <= 9 : id % 8;

  switch (idMod) {
    case 1:
      solver->var_decay = 0.94;
      solver->max_var_decay = 0.96;
      solver->firstReduceDB = 600;
      break;

    case 2:
      solver->var_decay = 0.90;
      solver->max_var_decay = 0.97;
      solver->firstReduceDB = 500;
      break;

    case 3:
      solver->var_decay = 0.85;
      solver->max_var_decay = 0.93;
      solver->firstReduceDB = 400;
      break;

    case 4:
      solver->var_decay = 0.95;
      solver->max_var_decay = 0.95;
      solver->firstReduceDB = 4000;
      solver->sizeLBDQueue = 100;
      solver->K = 0.7;
      solver->incReduceDB = 500;
      solver->lbdQueue.growTo(100);
      break;

    case 5:
      solver->var_decay = 0.93;
      solver->max_var_decay = 0.96;
      solver->firstReduceDB = 100;
      solver->incReduceDB = 500;
      break;

    case 6:
      solver->var_decay = 0.75;
      solver->max_var_decay = 0.94;
      solver->firstReduceDB = 2000;
      break;

    case 7:
      solver->var_decay = 0.94;
      solver->max_var_decay = 0.96;
      solver->firstReduceDB = 800;
      break;

    case 8:
      solver->reduceOnSize = true;
      break;

    case 9:
      solver->reduceOnSize = true;
      solver->reduceOnSizeSize = 14;
      break;

    case 0:
    default:
      break;
  }

  if (id > 9) {
    int noiseFactor = id / 8;
    double noisevar_decay = 0.005 + noiseFactor * 0.006;
    int noiseReduceDB = 50 + noiseFactor * 25;

    solver->var_decay += noisevar_decay;
    solver->firstReduceDB += noiseReduceDB;
  }
}

void
GlucoseSyrup::setOption(const std::string& key, int value)
{
  /* test switch case with compile time hash(string) */
  if (key == "var-decay")
    solver->var_decay = static_cast<double>(value) / 1'000;
  else if (key == "max-var-decay")
    solver->max_var_decay = static_cast<double>(value) / 1'000;
  else if (key == "first-reduce-db")
    solver->firstReduceDB = value;
  else if (key == "size-lbd-queue")
    solver->sizeLBDQueue = value;
  else if (key == "K")
    solver->K = static_cast<double>(value) / 1'000;
  else if (key == "inc-reduce-db")
    solver->incReduceDB = value;
  else if (key == "lbd-queue-size")
    solver->lbdQueue.growTo(value);
  else if (key == "reduce-on-size")
    solver->reduceOnSize = static_cast<bool>(value);
  else if (key == "reduce-on-size-size")
    solver->reduceOnSizeSize = value;
  else if (key == "seed")
    solver->random_seed = value;
  else if (key == "clause-decay")
    solver->clause_decay = static_cast<double>(value) / 1'000;
  else if (key == "random-var-freq")
    solver->random_var_freq = static_cast<double>(value) / 1'000;
  else if (key == "verbosity")
    solver->verbosity = value;
  else if (key == "ccmin-mode")
    solver->ccmin_mode = value;
  else if (key == "phase-saving")
    solver->phase_saving = value;
  else if (key == "rnd-pol")
    solver->rnd_pol = static_cast<bool>(value);
  else if (key == "rnd-init-act")
    solver->rnd_init_act = static_cast<bool>(value);
  else if (key == "garbage-frac")
    solver->garbage_frac = static_cast<double>(value) / 1'000;
  else if (key == "use-flip")
    solver->useFlip = true;
  else if (key == "use-pr")
    solver->usePR = true;
  else
    PABORT(PERR_ARGS,"Option %s is not recognized by GlucoseSyrup!", key.c_str());
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
GlucoseSyrup::setSolverInterrupt()
{
  LOGD1("Asking Glucose %d to end.", this->getSolverId());
  solver->interrupt();
}

void
GlucoseSyrup::unsetSolverInterrupt()
{
  solver->clearInterrupt();
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatAnswer
GlucoseSyrup::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  Glucose::vec<Glucose::Lit> gAssumptions;
  for (unsigned int i = 0; i < cube.size(); i++) {
    Glucose::Lit l = GLUE_LIT(cube[i]);
    if (!solver->isEliminated(var(l))) {
      gAssumptions.push(l);
    }
  }

  this->m_initialized = true;

  LOG2("Launching Glucose %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  Glucose::lbool res = solver->solveLimited(gAssumptions);

  if (res == l_True)
    return SatAnswer::SAT;

  if (res == l_False)
    return SatAnswer::UNSAT;

  return SatAnswer::UNKNOWN;
}

bool
GlucoseSyrup::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0);

  if (clause->size == 1) {
    m_unitsToImport.push(clause->lits[0]);
  } else {
    m_clausesToImport->addClause(clause);
  }
  return true;
}

std::string
GlucoseSyrup::statisticsToString()
{
  std::ostringstream oss;
  SolverCDCLInterface::Statistics stats;

  stats.conflicts = solver->conflicts;
  stats.propagations = solver->propagations;
  stats.restarts = solver->starts;
  stats.decisions = solver->decisions;

  oss << stats.toString(getSolverId(), "Glucose");

  return oss.str();
}

model_t
GlucoseSyrup::getModel()
{
  std::vector<int> model;

  for (int i = 0; i < solver->nVars(); i++) {
    if (solver->model[i] != l_Undef) {
      int lit = solver->model[i] == l_True ? i + 1 : -(i + 1);

      model.push_back(lit);
    }
  }

  return model;
}

void
GlucoseSyrup::getHeuristicData(std::vector<int>** flipActivity,
                               std::vector<int>** nbPropagations,
                               std::vector<int>** nbDecisionVar)
{
  if (flipActivity != NULL) {
    *flipActivity = &(solver->flipActivity);
  }
  if (nbPropagations != NULL) {
    *nbPropagations = &(solver->nbPropagations);
  }
  if (nbDecisionVar != NULL) {
    *nbDecisionVar = &(solver->nbDecisionVar);
  }
}

void
GlucoseSyrup::setHeuristicData(std::vector<int>* flipActivity,
                               std::vector<int>* nbPropagations,
                               std::vector<int>* nbDecisionVar)
{
  if (flipActivity != NULL) {
    solver->flipActivity = *flipActivity;
  }
  if (nbPropagations != NULL) {
    solver->nbPropagations = *nbPropagations;
  }
  if (nbDecisionVar != NULL) {
    solver->nbDecisionVar = *nbDecisionVar;
  }
}

clause_t
GlucoseSyrup::getFinalAnalysis()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
}

cube_t
GlucoseSyrup::getSatAssumptions()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
};