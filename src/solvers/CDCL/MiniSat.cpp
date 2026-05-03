
// MiniSat includes
#include <minisat/core/Dimacs.h>
#include <minisat/simp/SimpSolver.h>
#include <minisat/utils/System.h>

#include "utils/ErrorCodes.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"

#include "MiniSat.hpp"
#include <iomanip>

// Macros for minisat literal representation conversion
#define MINI_LIT(lit)                                                          \
  lit > 0 ? Minisat::mkLit(lit - 1, false) : Minisat::mkLit((-lit) - 1, true)

#define INT_LIT(lit)                                                           \
  Minisat::sign(lit) ? -(Minisat::var(lit) + 1) : (Minisat::var(lit) + 1)

static bool
checkLiteral(const Minisat::Lit& l, Minisat::Solver* minisat)
{
  // Assert if not negative (error)
  assert(l.x >= 0);
  // Check if the literal is not out of bound
  if (minisat->nVars() <= var(l)) {
    LOGWARN("Literal %d is out of bound for Minisat", l.x);
    return false;
  }
  return true;
}

static bool
makeMiniVec(ClauseExchangePtr cls,
            Minisat::vec<Minisat::Lit>& mcls,
            Minisat::Solver* minisat)
{
  for (size_t i = 0; i < cls->size; i++) {
    Minisat::Lit internalLit = MINI_LIT(cls->lits[i]);
    mcls.push(internalLit);
    // Checks
    if (!checkLiteral(internalLit, minisat))
      return false;
  }
  return true;
}

void
minisatExportClause(void* issuer, Minisat::vec<Minisat::Lit>& cls)
{
  /* TODO: a better fake glue management ?*/
  MiniSat* ms = (MiniSat*)issuer;

  // Fake glue value
  ClauseExchangePtr ncls =
    ClauseExchange::create(cls.size(), cls.size(), ms->getSharingId());

  for (int i = 0; i < cls.size(); i++) {
    ncls->lits[i] = INT_LIT(cls[i]);
  }

  ncls->from = ms->getSharingId();

  ms->exportClause(ncls);
}

Minisat::Lit
minisatImportUnit(void* issuer)
{
  MiniSat* ms = (MiniSat*)issuer;

  Minisat::Lit l;

  std::vector<int> units;

  ms->m_unitsToImport.consume_all(
    [&units](int unit) { units.push_back(unit); });

  for (lit_t lit : units) {
    l = MINI_LIT(lit);

    if (checkLiteral(l, ms->solver) == true) {
      assert(l.x >= 0);
      LOGD2("Importing to Minisat#%u unit literal %d", ms->getSharingId(), l.x);
      return l;
    }
  }
  return Minisat::lit_Undef;
}

bool
minisatImportClause(void* issuer, Minisat::vec<Minisat::Lit>& mcls)
{
  MiniSat* ms = (MiniSat*)issuer;

  ClauseExchangePtr cls;

  while (ms->m_clausesToImport->getOneClause(cls)) {
    if (makeMiniVec(cls, mcls, ms->solver)) {
      if (cls->lbd)
        LOGD4("MiniSat %u will import redundant clause %s",
              ms->getSharingId(),
              cls->toString().c_str());
      else
        LOG2("MiniSat %u will import irredundant clause %s",
             ms->getSharingId(),
             cls->toString().c_str());

      assert(mcls.size() > 1 && mcls[0].x >= 0);

      return true;
    }
    mcls.clear();
  }

  ms->m_clausesToImport->shrinkDatabase();
  return false;
}

MiniSat::MiniSat(int id,
                 const std::shared_ptr<ClauseDatabase>& clauseDB,
                 FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::MINISAT)
  , m_clausesToImport(clauseDB)
  , m_unitsToImport(256)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , mcls(std::make_unique<Minisat::vec<Minisat::Lit>>())
{
  solver = new Minisat::SimpSolver();
  solver->remove_satisfied = false;

  solver->exportClauseCallback = minisatExportClause;
  solver->importUnitCallback = minisatImportUnit;
  solver->importClauseCallback = minisatImportClause;
  solver->issuer = this;

  initializeTypeId<MiniSat>();
}

MiniSat::~MiniSat()
{
  delete solver;
}

void
MiniSat::loadFormula(const char* filename)
{
  gzFile in = gzopen(filename, "rb");

  parse_DIMACS(in, *solver);

  gzclose(in);
}

bool
MiniSat::addClause(clause_view_t clause)
{
  assert(clause.size() > 0);
  mcls->clear();
  for (lit_t lit : clause) {
    // From Minisat Parser
    uint var = abs(lit) - 1;
    while (var >= solver->nVars())
      solver->newVar();
    mcls->push(MINI_LIT(lit));
  }

  if (solver->addClause(*mcls) == false) {
    LOGWARN("UNSAT when adding cls");
    return false;
  }

  return true;
}

uint
MiniSat::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

// Get the number of variables of the formula
uint
MiniSat::getVariableCount()
{
  return solver->nVars();
}

// Get a variable suitable for search splitting
var_t
MiniSat::getDivisionVariable()
{
  return (rand() % getVariableCount()) + 1;
}

// Set initial phase for a given variable
void
MiniSat::setPhase(const var_t var, const bool phase)
{
  solver->setPolarity(var - 1, phase ? Minisat::l_True : Minisat::l_False);
}

// Bump activity for a given variable
void
MiniSat::bumpVariableActivity(const var_t var, const int times)
{
  for (int i = 0; i < times; i++) {
    solver->varBumpActivity(var - 1);
  }
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
MiniSat::setSolverInterrupt()
{
  LOGD1("Asking Minisat (%d, %u) to end",
        this->getSolverId(),
        this->getSolverTypeId());

  solver->interrupt();
}

void
MiniSat::unsetSolverInterrupt()
{
  solver->clearInterrupt();
}

// Diversify the solver
void
MiniSat::diversify(const SeedGenerator& getSeed)
{
  solver->random_seed = (double)getSeed(this);
}

void
MiniSat::setOption(const std::string& key, int value)
{
  if (key == "var-decay")
    solver->var_decay = static_cast<double>(value) / 1'000;
  else if (key == "clause-decay")
    solver->clause_decay = static_cast<double>(value) / 1'000;
  else if (key == "random-var-freq")
    solver->random_var_freq = static_cast<double>(value) / 1'000;
  else if (key == "seed")
    solver->random_seed = value;
  else if (key == "verbosity")
    solver->verbosity = value;
  else if (key == "luby-restart")
    solver->luby_restart = static_cast<bool>(value);
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
  else if (key == "min-learnts-lim")
    solver->min_learnts_lim = value;
  else if (key == "restart-first")
    solver->restart_first = value;
  else if (key == "restart-inc")
    solver->restart_inc = static_cast<double>(value) / 1'000;
  else if (key == "learntsize-factor")
    solver->learntsize_factor = static_cast<double>(value) / 1'000;
  else if (key == "learntsize-inc")
    solver->learntsize_inc = static_cast<double>(value) / 1'000;
  else if (key == "learntsize-adjust-start-confl")
    solver->learntsize_adjust_start_confl = value;
  else if (key == "learntsize-adjust-inc")
    solver->learntsize_adjust_inc = static_cast<double>(value) / 1'000;
  else
    PABORT(PERR_ARGS,"Option %s is not recognized by Minisat!", key.c_str());
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatAnswer
MiniSat::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  Minisat::vec<Minisat::Lit> miniAssumptions;
  for (size_t ind = 0; ind < cube.size(); ind++) {
    miniAssumptions.push(MINI_LIT(cube[ind]));
  }

  this->m_initialized = true;

  LOG2("Launching MiniSat %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  Minisat::lbool res = solver->solveLimited(miniAssumptions);

  if (res == Minisat::l_True)
    return SatAnswer::SAT;

  if (res == Minisat::l_False)
    return SatAnswer::UNSAT;

  return SatAnswer::UNKNOWN;
}

bool
MiniSat::importClause(const ClauseExchangePtr& clause)
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
MiniSat::statisticsToString()
{
  std::ostringstream oss;
  SolverCDCLInterface::Statistics stats;

  stats.conflicts = solver->conflicts;
  stats.propagations = solver->propagations;
  stats.restarts = solver->starts;
  stats.decisions = solver->decisions;

  oss << stats.toString(getSolverId(), "MiniSat");

  return oss.str();
}

model_t
MiniSat::getModel()
{
  std::vector<int> model;

  for (int i = 0; i < solver->nVars(); i++) {
    if (solver->model[i] != Minisat::l_Undef) {
      int lit = solver->model[i] == Minisat::l_True ? i + 1 : -(i + 1);
      model.push_back(lit);
    }
  }

  return model;
}

clause_t
MiniSat::getFinalAnalysis()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
}

cube_t
MiniSat::getSatAssumptions()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
};