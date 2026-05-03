// MapleCOMSPS includes
#include <mapleCOMSPS/mapleCOMSPS/core/Dimacs.h>
#include <mapleCOMSPS/mapleCOMSPS/simp/SimpSolver.h>
#include <mapleCOMSPS/mapleCOMSPS/utils/System.h>

#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/System.hpp"

#include "solvers/CDCL/MapleCOMSPSSolver.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"

#include <iomanip>

// Due to namespace expansion
#define mp_True                                                                \
  (MapleCOMSPS::lbool((uint8_t)0)) // gcc does not do constant propagation if
                                   // these are real constants.
#define mp_False (MapleCOMSPS::lbool((uint8_t)1))
#define mp_Undef (MapleCOMSPS::lbool((uint8_t)2))

// Macros for minisat literal representation conversion
#define MINI_LIT(lit)                                                          \
  lit > 0 ? MapleCOMSPS::mkLit(lit - 1, false)                                 \
          : MapleCOMSPS::mkLit((-lit) - 1, true)

#define INT_LIT(lit)                                                           \
  MapleCOMSPS::sign(lit) ? -(MapleCOMSPS::var(lit) + 1)                        \
                         : (MapleCOMSPS::var(lit) + 1)

static bool
checkLiteral(const MapleCOMSPS::Lit& l, MapleCOMSPS::Solver* maple)
{
  // Assert if not negative (error)
  assert(l.x >= 0);
  // Check if the literal is not out of bound
  if (maple->nVars() <= var(l)) {
    LOGWARN("Literal %d is out of bound for MapleCOMSPS", l.x);
    return false;
  }
  return true;
}

static bool
makeMiniVec(ClauseExchangePtr cls,
            MapleCOMSPS::vec<MapleCOMSPS::Lit>& mcls,
            MapleCOMSPS::Solver* maple)
{
  for (size_t i = 0; i < cls->size; i++) {
    MapleCOMSPS::Lit internalLit = MINI_LIT(cls->lits[i]);
    mcls.push(internalLit);
    // Checks
    if (!checkLiteral(internalLit, maple))
      return false;
  }
  return true;
}

void
cbkMapleCOMSPSExportClause(void* issuer,
                           unsigned int lbd,
                           MapleCOMSPS::vec<MapleCOMSPS::Lit>& cls)
{
  MapleCOMSPSSolver* mp = (MapleCOMSPSSolver*)issuer;

  ClauseExchangePtr ncls =
    ClauseExchange::create(cls.size(), lbd, mp->getSharingId());

  for (int i = 0; i < cls.size(); i++) {
    ncls->lits[i] = INT_LIT(cls[i]);
  }
  /* filtering defined by a sharing strategy */
  mp->exportClause(ncls);
}

MapleCOMSPS::Lit
cbkMapleCOMSPSImportUnit(void* issuer)
{
  MapleCOMSPSSolver* mp = (MapleCOMSPSSolver*)issuer;

  MapleCOMSPS::Lit l;

  std::vector<int> units;

  mp->m_unitsToImport.consume_all(
    [&units](int unit) { units.push_back(unit); });

  for (lit_t lit : units) {
    l = MINI_LIT(lit);

    if (checkLiteral(l, mp->solver) == true) {
      assert(l.x >= 0);
      LOGD2("Importing to Maple %u unit literal %d", mp->getSharingId(), l.x);
      return l;
    }
  }
  return MapleCOMSPS::lit_Undef;
}

bool
cbkMapleCOMSPSImportClause(void* issuer,
                           unsigned int* lbd,
                           MapleCOMSPS::vec<MapleCOMSPS::Lit>& mcls)
{
  MapleCOMSPSSolver* mp = (MapleCOMSPSSolver*)issuer;

  ClauseExchangePtr cls;

  while (mp->m_clausesToImport->getOneClause(cls)) {
    if (makeMiniVec(cls, mcls, mp->solver)) {
      *lbd = cls->lbd;

      if (cls->lbd)
        LOGD4("Maple %u will import redundant clause %s",
              mp->getSharingId(),
              cls->toString().c_str());
      else
        LOG2("Maple %u will import irredundant clause %s",
             mp->getSharingId(),
             cls->toString().c_str());

      assert(mcls.size() > 1 && mcls[0].x >= 0 && *lbd > 1);

      return true;
    }
    mcls.clear();
  }

  // If we couldn't get any clause
  mp->m_clausesToImport->shrinkDatabase();
  return false;
}

MapleCOMSPSSolver::MapleCOMSPSSolver(
  int id,
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::MAPLECOMSPS)
  , m_clausesToImport(clauseDB)
  , m_unitsToImport(256)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , mcls(std::make_unique<MapleCOMSPS::vec<MapleCOMSPS::Lit>>())
{
  solver = new MapleCOMSPS::SimpSolver();

  solver->cbkExportClause = cbkMapleCOMSPSExportClause;
  solver->cbkImportClause = cbkMapleCOMSPSImportClause;
  solver->cbkImportUnit = cbkMapleCOMSPSImportUnit;
  solver->issuer = this;

  initializeTypeId<MapleCOMSPSSolver>();
}

MapleCOMSPSSolver::~MapleCOMSPSSolver()
{
  delete solver;
}

void
MapleCOMSPSSolver::loadFormula(const char* filename)
{
  gzFile in = gzopen(filename, "rb");

  parse_DIMACS(in, *solver);

  gzclose(in);
}

bool
MapleCOMSPSSolver::addClause(clause_view_t clause)
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
MapleCOMSPSSolver::loadClauses()
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
MapleCOMSPSSolver::getVariableCount()
{
  return solver->nVars();
}

// Get a variable suitable for search splitting
var_t
MapleCOMSPSSolver::getDivisionVariable()
{
  return (rand() % getVariableCount()) + 1;
}

// Set initial phase for a given variable
void
MapleCOMSPSSolver::setPhase(const var_t var, const bool phase)
{
  solver->setPolarity(var - 1, phase ? true : false);
}

// Bump activity for a given variable
void
MapleCOMSPSSolver::bumpVariableActivity(const var_t var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
MapleCOMSPSSolver::setSolverInterrupt()
{
  stopSolver = true;

  solver->interrupt();

  LOGD1("Asking MapleCOMSPS (%d, %u) to end",
        this->getSolverId(),
        this->getSolverTypeId());
}

void
MapleCOMSPSSolver::unsetSolverInterrupt()
{
  stopSolver = false;

  solver->clearInterrupt();
}

// Diversify the solver
void
MapleCOMSPSSolver::diversify(const SeedGenerator& getSeed)
{
  /* TODO enhance this diversification */

  int seed = this->getSolverTypeId();
  if (seed == ID_XOR) {
    solver->GE = true;
  } else {
    solver->GE = false;
  }

  if (seed % 2) {
    solver->VSIDS = false;
  } else {
    solver->VSIDS = true;
  }

  if (seed % 4 >= 2) {
    solver->verso = false;
  } else {
    solver->verso = true;
  }
}

void
MapleCOMSPSSolver::setOption(const std::string& key, int value)
{
  /* test switch case with compile time hash(string) */
  if (key == "enable-ge")
    solver->GE = static_cast<bool>(value);
  else if (key == "enable-vsids")
    solver->VSIDS = value;
  else if (key == "enabled-verso")
    solver->verso = value;
  else if (key == "seed")
    solver->random_seed = value;
  else
    PABORT(PERR_ARGS,"Option %s is not recognized by MapleCOMSPSSolver!", key.c_str());
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatAnswer
MapleCOMSPSSolver::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  MapleCOMSPS::vec<MapleCOMSPS::Lit> miniAssumptions;
  for (size_t ind = 0; ind < cube.size(); ind++) {
    miniAssumptions.push(MINI_LIT(cube[ind]));
  }

  this->m_initialized = true;

  LOG2("Launching MapleCOMSPS %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  MapleCOMSPS::lbool res = solver->solveLimited(miniAssumptions);

  if (res == mp_True)
    return SatAnswer::SAT;

  if (res == mp_False)
    return SatAnswer::UNSAT;

  return SatAnswer::UNKNOWN;
}

bool
MapleCOMSPSSolver::importClause(const ClauseExchangePtr& clause)
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
MapleCOMSPSSolver::statisticsToString()
{
  std::ostringstream oss;
  SolverCDCLInterface::Statistics stats;

  stats.conflicts = solver->conflicts;
  stats.propagations = solver->propagations;
  stats.restarts = solver->starts;
  stats.decisions = solver->decisions;

  oss << stats.toString(getSolverId(), "Maple");

  return oss.str();
}

model_t
MapleCOMSPSSolver::getModel()
{
  std::vector<int> model;

  for (int i = 0; i < solver->nVars(); i++) {
    if (solver->model[i] != mp_Undef) {
      int lit = solver->model[i] == mp_True ? i + 1 : -(i + 1);
      model.push_back(lit);
    }
  }

  return model;
}

clause_t
MapleCOMSPSSolver::getFinalAnalysis()
{
  std::vector<int> outCls;

  for (int i = 0; i < solver->conflict.size(); i++) {
    outCls.push_back(INT_LIT(solver->conflict[i]));
  }

  return outCls;
}

cube_t
MapleCOMSPSSolver::getSatAssumptions()
{
  std::vector<int> outCls;
  MapleCOMSPS::vec<MapleCOMSPS::Lit> lits;
  solver->getAssumptions(lits);
  for (int i = 0; i < lits.size(); i++) {
    outCls.push_back(-(INT_LIT(lits[i])));
  }
  return outCls;
};

void
MapleCOMSPSSolver::setStrengthening(bool b)
{
  solver->setStrengthening(b);
}