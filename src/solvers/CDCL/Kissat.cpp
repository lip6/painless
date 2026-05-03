#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include <cassert>
#include <iomanip>

#include "solvers/CDCL/Kissat.hpp"

// loadFormula includes
extern "C"
{
#include <kissat/src/parse.h>
}

// ==================================================================
// C Wrappers
// ==================================================================

int
kissatTerminate(void* vpkissat)
{
  Kissat* pkissat = (Kissat*)vpkissat;
  return static_cast<int>(pkissat->m_stopSolver.load());
}

unsigned char
kissatHasClauseToImport(void* vpkissat)
{
  Kissat* pkissat = static_cast<Kissat*>(vpkissat);
  return pkissat->backendHasClauseToImport();
}

unsigned char
kissatImportClause(void* vpkissat)
{
  Kissat* pkissat = static_cast<Kissat*>(vpkissat);
  return pkissat->backendImportClause();
}

char
kissatExportClause(void* vpkissat)
{
  Kissat* pkissat = static_cast<Kissat*>(vpkissat);
  return pkissat->backendExplortClause();
}

// ==================================================================

Kissat::Kissat(int id,
               const std::shared_ptr<ClauseDatabase>& clauseDB,
               FullClauseReader fullReader)
  : SolverCDCLInterface(id, SolverCDCLType::KISSAT)
  , m_clausesToImport(clauseDB)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , m_originalVars(0)
{
  m_solver = kissat_init();

  kissat_set_terminate(m_solver, this, kissatTerminate);
  kissat_set_export_call(m_solver, kissatExportClause);
  kissat_set_import_call(m_solver, kissatImportClause);
  kissat_set_import_check(m_solver, kissatHasClauseToImport);
  kissat_set_painless(m_solver, this);
  kissat_set_id(m_solver, id);

  initKissatOptions(); /* Must be called before reserve or initshuffle */

  initializeTypeId<Kissat>();
}

Kissat::~Kissat()
{
  // kissat_print_sharing_stats(m_solver);
  kissat_release(m_solver);
}

// ==================================================================
// Execution
// ==================================================================

SatAnswer
Kissat::solve(cube_view_t cube)
{
  if (kissat_check_searches(m_solver)) {
    PABORT(PERR_BAD_BEHAVIOR,
           "Kissat m_solver %d was asked to solve more than once !!",
           this->getSolverId());
  }

  if (this->m_initialized) {
    // Already run before need to delete previous m_solver and re-initialize it
    kissat_release(m_solver);

    m_solver = kissat_init();

    // Empty import database
    m_clausesToImport->clearDatabase();
    // Reset index to read full formula
    m_fullReaderIndex = 0;

    kissat_set_terminate(m_solver, this, kissatTerminate);
    kissat_set_export_call(m_solver, kissatExportClause);
    kissat_set_import_call(m_solver, kissatImportClause);
    kissat_set_import_check(m_solver, kissatHasClauseToImport);
    kissat_set_painless(m_solver, this);

    // Set all params done for the previous instances
    for (auto& strInt : m_kissatParameters) {
      setOption(strInt.first, strInt.second);
    }

    this->m_initialized = false;

    LOG2("Kissat reset for incremental solving");
  }
  // Will update the originalVars correctly
  uint readClauses = loadClauses();

  LOGWARNIF(cube.size(),
            "Kissat doesn't support assumptions, it will use units instead");
  for (lit_t lit : cube) {
    kissat_add(m_solver, lit);
    kissat_add(m_solver, 0);
    m_assumptions.push_back(lit);
  }

  this->m_initialized = true;

  LOG2("Launching Kissat %u with %u clauses (%u added this round), %u "
       "variables and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       m_originalVars,
       cube.size());

  int res = kissat_solve(m_solver);

  if (res == 10) {
    LOG2("Kissat %d responded with SAT", this->getSolverId());
    return SatAnswer::SAT;
  }
  if (res == 20) {
    LOG2("Kissat %d responded with UNSAT", this->getSolverId());
    return SatAnswer::UNSAT;
  }
  LOGD2("Kissat %d responded with %d (UNKNOWN)", this->getSolverId(), res);
  return SatAnswer::UNKNOWN;
}

void
Kissat::setSolverInterrupt()
{
  m_stopSolver = true;
  kissat_terminate(m_solver);
  LOGD1("Asking Kissat (%d, %u) to end",
        this->getSolverId(),
        this->getSolverTypeId());
}

void
Kissat::unsetSolverInterrupt()
{
  m_stopSolver = false;
}

void
Kissat::initKissatOptions()
{
  LOG3("Initializing Kissat configuration ..");
  // initial configuration (mostly defaults of kissat_mab)

  // Not Needed in practice
  // #ifndef NDEBUG
  //   this->setOption(
  //     "check",
  //     1); // (only on debug) 1: check model only, 2: check derived clauses
  //   this->setOption("quiet", 0);
  // #else
  // this->setOption("quiet", 1);
  // this->setOption("check", 0);
  // #endif

  // this->setOption("log", 0);
  // this->setOption("verbose", 0); // verbosity level

  /*
   * stable = 0: always focused mode (more restarts) good for UNSAT
   * stable = 1: switching between stable and focused
   * stable = 2: always in stable mode
   */
  this->setOption("stable", 1);
  /*
   * If m_solver in stable mode or target > 1 it takes the target phase
   */
  this->setOption("target", 1);

  // this->setOption("initshuffle",
  //                 0); /* Shuffle variables in queue before kissat_add */

  // Garbage Collection
  this->setOption("compact", 1);     // enable compacting garbage collection
  this->setOption("compactlim", 10); // compact inactive limit (in percent)

  /*----------------------------------------------------------------------------*/

  // Initialization
  // this->setOption("walkinitially", 0);
  this->setOption("warmup", 1); // initialize phases by unit propagation

  /*----------------------------------------------------------------------------*/

  // (Pre/In)processing Simplifications
  this->setOption("otfs",
                  1); // on-the-fly strengthening when deducing learned clause
  this->setOption("reduce", 1); // learned clause reduction

  this->setOption("probe", 1);  // Enable all different types of inprocessing
                                // (bva, congruence, vivification, sweeping ...)
  this->setOption("vivify", 1); // vivify clauses, many suboptions
  this->setOption("factor", 0); // bounded variable addition (inprocessing)
                                // (breaks sharing soundness)
  this->setOption("congruence", 1); // congruence closure on extracted gates
  this->setOption("sweep", 1);      // enable SAT sweeping

  // Minimization options
  this->setOption("minimize", 1); // learned clause minimization
  this->setOption("minimizedepth",
                  1e3); // already exists: used in mallob diversification

  // Subsumption options: many other suboptions
  this->setOption("subsumeclslim", 1e3); // subsumption clause size limit,
  this->setOption("eagersubsume",
                  4); // eagerly subsume recently learned clauses (max 4)

  /*-- Equisatisfiable Simplifications --*/
  this->setOption("substitute",
                  1); // equivalent literal substitution, many subsumptions
  // this->setOption("autarky", 1); // autarky reasoning
  this->setOption("eliminate",
                  1); // bounded variable elimination (BVE), many suboptions

  // Enhancements for variable elimination
  this->setOption("forward", 1); // forward subsumption in BVE
  this->setOption("extract", 1); // extract gates in variable elimination
  this->setOption("fastelsub",
                  1);           // forward subsuming fast variable elimination
  this->setOption("fastel", 1); // initial fast variable elimination

  // Gates detection
  this->setOption("ands", 1);         // and gates detection
  this->setOption("equivalences", 1); // extract and eliminate equivalence gates
  this->setOption("ifthenelse", 1); // extract and eliminate if-then-else gates

  // Lucky assignments
  this->setOption("lucky", 1);      // try some lucky assignments
  this->setOption("luckyearly", 1); // lucky assignments before preprocessing
  this->setOption("luckylate", 1);  // lucky assignments after preprocessing

  // Preprocessors
  this->setOption("preprocess", 1);           // initial preprocessing
  this->setOption("preprocessbackbone", 1);   // backbone preprocessing
  this->setOption("preprocesscongruence", 1); // congruence preprocessing
  this->setOption("preprocessfactor", 1);     // variable addition preprocessing
  this->setOption("preprocessprobe", 1);      // probing preprocessing
  this->setOption("preprocessrounds", 1);     // initial preprocessing rounds
  this->setOption("preprocessweep", 1);       // sweep preprocessing

  /*----------------------------------------------------------------------------*/

  // Search Configuration
  this->setOption("chrono", 1); // chronological backtracking, according the
                                // original paper, it is benefical for UNSAT
  this->setOption("chronolevels",
                  100); // if conflict analysis want to jump more than this
                        // amount of levels, chronological will be used

  // Random decisions
  this->setOption("randec", 1);        // random decisions
  this->setOption("randecfocused", 1); // random decisions in focused mode
  this->setOption("randecinit", 500);  // random decisions interval
  this->setOption("randecint", 500);   // initial random decisions interval
  this->setOption("randeclength", 10); // random conflicts length
  // this->setOption("randecstable", 0);  // random decisions in stable mode

  // Restart Management
  this->setOption("restart", 1);
  this->setOption("restartint", 1);     // base restart interval
  this->setOption("restartmargin", 10); // fast/slow margin in percent
  this->setOption("reluctant", 1);      // stable reluctant doubling restarting

  // Clause and Literal Related Heuristic
  // In Kissat glue = 1 means lbd = 2 with one of the levels being 0
  this->setOption("tier1", 2); // glue limit for tier1
  this->setOption("tier2", 6); // glue limit for tier2

  // Phase
  this->setOption("phase", 1); /* initial phase: set the macro INITIAL_PHASE */
  this->setOption("phasesaving", 1);
  this->setOption("rephase",
                  1); // reinitialization of decision phases, have two
                      // suboptions that are never accessed outside of tests
  // this->setOption("forcephase",
  //                 0); // force initial phase, forces the target option to
  //                 false

  /*----------------------------------------------------------------------------*/

  // Diverse (used in mallob diversification)
  // this->setOption("reducefraction", 75);
  this->setOption("vivifyeffort", 100);

  // random seed
  this->setOption("seed", this->getSolverId()); // used in walk and rephase
}

// Diversify the m_solver
void
Kissat::diversify(const SeedGenerator& getSeed)
{
  if (this->isInitialized()) {
    LOGERROR("Diversification must be done before adding clauses because of "
             "kissat_reserve()");
    exit(PERR_NOT_SUPPORTED);
  }

  int typeId = this->getSolverTypeId();
  int generalSeed = getSeed(this);

  /* Global Diversification (across all solvers)*/
  this->setOption("seed", generalSeed);
  this->setOption("phase", generalSeed % 2);

  /* Kissat Group Diversification */
  Kissat::Family family = static_cast<Kissat::Family>(
    getSolverTypeId() % static_cast<int>(Kissat::Family::COUNT));

  // Attempt to have all possible combinations
  // Number of options is 5
  this->setOption("preprocessbackbone", typeId % (1 << 5));
  this->setOption("preprocesscongruence", typeId % (1 << 4));
  this->setOption("preprocessfactor", typeId % (1 << 3));
  this->setOption("preprocessprobe", typeId % (1 << 2));
  this->setOption("preprocessweep", typeId % (1));

  // this->setOption("preprocessrounds",(typeId % 3) + 1);

  switch (family) {
    // 1/3 Focus on UNSAT
    case Kissat::Family::UNSAT_FOCUSED:
      kissat_set_configuration(m_solver, "unsat");
      this->setOption("restartint", 1);
      this->setOption("chrono", 0);

      if (getSolverTypeId() % 4 == 0) // 1 in 4 from UNSAT_FOCUSED
      {
        this->setOption("initshuffle", 1);
      }
      break;

    // Focus on SAT ; target at 2 to enable target phase
    case Kissat::Family::SAT_STABLE:
      kissat_set_configuration(m_solver, "sat");

      // Some solvers (1 in 2 in this family) do initial walk and walk further
      // in rephasing ( benifical for SAT formulae)
      if (getSolverTypeId() % 2 == 0) {
        /* less restarts when in focused mode */
        this->setOption("restartint", 50 + typeId % 100);
        this->setOption("restartmargin", typeId % 25 + 10);
        /* to start at stable and to not switch to focused*/
        this->setOption("stable", 2);
        this->setOption("walkinitially", 1);
        /* Oh's expriment showed that learned clauses are not that important for
         * SAT*/
        this->setOption("tier1", 2);
        this->setOption("tier2", 3);

        // (1 in 4 in SAT)  more aggressive chronological backtracking
        if (getSolverTypeId() % 4 == 0) {
          this->setOption("chronolevels", typeId % 200);
        }
      }

      break;

    // Switch mode
    default:
      this->setOption("walkinitially", 1);
      /* Find other diversification parameters : restarts ?*/
      this->setOption("initshuffle", 1); /* takes some time */
      // Mallob ISC24 diversfication
      switch (typeId % 8) {
        case 0:
          this->setOption("eliminate", 0);
          break;
        case 1:
          this->setOption("restartint", 10);
          break;
        case 2:
          this->setOption("walkinitially", 1);
          break;
        case 3:
          this->setOption("restartint", 0);
          break;
        case 4:
          this->setOption("sweep", 0);
          break;
        case 5:
          this->setOption("probe", 0);
          break;
        case 6:
          this->setOption("minimizedepth", 1e4);
          break;
        case 7:
          this->setOption("vivifyeffort", 1000);
          break;
      }
  }

  LOGD1("Diversified Kissat (%d,%d) of type %u",
        this->getSolverId(),
        typeId,
        family);
}

void
Kissat::setOption(const std::string& key, int value)
{
  PABORTIF(kissat_set_option(m_solver, key.c_str(), value) == INT_MIN,
           PERR_ARGS,
           "Option %s is not recognized by Kissat!",
           key.c_str());
  m_kissatParameters[key] = value;
}

// ==================================================================
// Clause Management
// ==================================================================

void
Kissat::loadFormula(const char* filename)
{
  strictness strict = NORMAL_PARSING;
  file in;
  uint64_t lineno;
  kissat_open_to_read_file(&in, filename);

  kissat_parse_dimacs(m_solver, strict, &in, &lineno, (int*)&m_originalVars);

  kissat_close_file(&in);

  m_initialized = true;
  LOG2("The Kissat Solver %d loaded all the formula with %u variables",
       this->getSolverId(),
       m_originalVars);
}

bool
Kissat::addClause(clause_view_t clause)
{
  LOGDVECTOR4(clause.data(),
              clause.size(),
              "Adding to Kissat %u:",
              this->getSolverTypeId());

  for (lit_t lit : clause) {
    m_originalVars = std::max<unsigned>(m_originalVars, std::abs(lit));
    kissat_add(m_solver, lit);
  }
  kissat_add(m_solver, 0);

  return true;
}

uint
Kissat::loadClauses()
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
Kissat::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0);
  m_clausesToImport->addClause(clause);
  return true;
}

bool
Kissat::backendHasClauseToImport()
{
  if (!this->m_clausesToImport->getOneClause(m_clauseToImport)) {
    this->m_clausesToImport->shrinkDatabase();
    return false;
  }

  return true;
}

bool
Kissat::backendImportClause()
{
  // Returns false if this clause shouldn't be imported after checking the
  // solver state
  kissat_set_pglue(m_solver, m_clauseToImport->lbd);

  bool willImport = kissat_import_pclause(
    m_solver, m_clauseToImport->lits, m_clauseToImport->size);

  if (willImport) {
    if (m_clauseToImport->lbd)
      LOGD3("Kissat %u will import redundant clause %s",
            this->getSharingId(),
            m_clauseToImport->toString().c_str());
    else
      LOGD3("Kissat %u will import irredundant clause %s",
            this->getSharingId(),
            m_clauseToImport->toString().c_str());
  }

  return willImport;
}

bool
Kissat::backendExplortClause()
{
  unsigned int lbd = kissat_get_pglue(m_solver);

  unsigned int size = kissat_pclause_size(m_solver);

  assert(size > 0);

  // In case there are assumptions we add their negation to have a sharable
  // clause
  m_clauseToExport.clear();

  for (unsigned int i = 0; i < size; i++) {
    m_clauseToExport.push_back(kissat_peek_plit(m_solver, i));
  }

  for (lit_t lit : m_assumptions)
    m_clauseToExport.push_back(-lit);

  ClauseExchangePtr new_clause =
    ClauseExchange::create(m_clauseToExport.begin(),
                           m_clauseToExport.end(),
                           lbd,
                           this->getSharingId());

  LOGD3("Kissat %u will export redundant clause %s",
        this->getSharingId(),
        new_clause->toString().c_str());

  /* filtering defined by a sharing strategy */
  if (this->exportClause(new_clause)) {
    return true;
  } else
    return false;
}

// ==================================================================
// Variable Management
// ==================================================================

uint
Kissat::getVariableCount()
{
  return kissat_get_var_count(m_solver);
}

var_t
Kissat::getDivisionVariable()
{
  return (rand() % getVariableCount()) + 1;
}

void
Kissat::setPhase(const var_t var, const bool phase)
{
  if (!this->isInitialized()) {
    LOGERROR("Cannot set a phase before initializing the m_solver!");
    return;
  }
  /* set target, best and saved phases: best is used in walk */
  kissat_set_phase(m_solver, var, (phase) ? 1 : -1);
}

void
Kissat::bumpVariableActivity(const var_t var, const int times)
{
  LOGERROR("Not Implemented, yet");
  exit(PERR_NOT_SUPPORTED);
}

// ==================================================================
// Statistics
// ==================================================================

std::string
Kissat::statisticsToString()
{
  std::ostringstream oss;
  KissatMainStatistics kstats;
  SolverCDCLInterface::Statistics stats;

  kissat_get_main_statistics(m_solver, &kstats);

  stats.conflicts = kstats.conflicts;
  stats.propagations = kstats.propagations;
  stats.restarts = kstats.restarts;
  stats.decisions = kstats.decisions;
  stats.memPeak = kstats.memoryPeak;
  stats.importedUnits = kstats.importedUnits;
  stats.importedBinaries = kstats.importedBinaries;
  stats.importedLarges = kstats.importedLarges;
  stats.exportedClauses = kstats.exportedClauses;
  stats.filteredExportedClauses = kstats.filteredExportedClauses;

  oss << stats.toString(getSolverId(), "Kissat");

  return oss.str();
}

model_t
Kissat::getModel()
{
  std::vector<int> model;

  for (unsigned int i = 1; i <= m_originalVars; i++) {
    int tmp = kissat_value(m_solver, i);
    if (!tmp)
      tmp = i;
    model.push_back(tmp);
  }

  return model;
}

clause_t
Kissat::getFinalAnalysis()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
}

cube_t
Kissat::getSatAssumptions()
{
  std::vector<int> outCls;
  LOGERROR("NOT IMPLEMENTED");
  return outCls;
};
