#include "YalSAT.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

/* For termination callback check */
int
YalSAT_terminate(void* p_yalsat)
{
  YalSAT* cpp_yalsat = (YalSAT*)p_yalsat;
  if (cpp_yalsat->terminateSolver)
    return 1;
  else
    return 0;
}

YalSAT::YalSAT(int _id, FullClauseReader fullReader)
  : mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , LocalSearchInterface(_id, LocalSearchType::YALSAT)
{
  // Yals * yals_new ();
  initializeTypeId<YalSAT>();
  this->solver = yals_new();
  yals_seterm(this->solver, YalSAT_terminate, this);
  this->clausesCount = 0;
}

YalSAT::YalSAT(int _id,
               unsigned long flipsLimit,
               unsigned long maxNoise,
               FullClauseReader fullReader)
  : m_flipsLimit(flipsLimit)
  , m_maxNoise(maxNoise)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , LocalSearchInterface(_id, LocalSearchType::YALSAT)
{
  // Yals * yals_new ();
  initializeTypeId<YalSAT>();
  this->solver = yals_new();
  yals_seterm(this->solver, YalSAT_terminate, this);
  this->clausesCount = 0;

  markConfigured();
}

YalSAT::~YalSAT()
{
  // void yals_del (Yals *);
  yals_del(this->solver);
  LOGD1("YalSAT %d deleted!", this->getSolverId());
}

uint
YalSAT::getVariableCount()
{
  // -1 because of if (idx >= yals->nvars) yals->nvars = idx + 1; in yals_add
  return yals_getnvars(this->solver) - 1;
}

var_t
YalSAT::getDivisionVariable()
{
  // TODO: a better division according to the trail (read about divide and
  // conquer)
  return (rand() % getVariableCount()) + 1;
}

void
YalSAT::setSolverInterrupt()
{
  if (!this->terminateSolver) {
    LOG1("Asked YalSAT %d to terminate", this->getSolverId());
    this->terminateSolver = true;
  }
}

void
YalSAT::unsetSolverInterrupt()
{
  this->terminateSolver = false;
}

SatAnswer
YalSAT::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  int res;

  PABORTIF(cube.size(),
           PERR_BAD_BEHAVIOR,
           "YalSAT doesn't offer assumptions (To be added)");

  this->m_initialized = true;

  LOG2("Launching YalSAT %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  res = yals_sat(this->solver);

  this->lsStats.numberFlips = yals_flips(this->solver);
  this->lsStats.numberUnsatClauses = yals_minimum(this->solver);

  LOGSTAT("[YalSAT %d] Number of remaining unsats %d / %d, Number of Flips %d.",
          this->getSolverId(),
          this->lsStats.numberUnsatClauses,
          this->clausesCount,
          this->lsStats.numberFlips);

  if (static_cast<int>(SatAnswer::SAT) != res) {
    return SatAnswer::UNKNOWN;
  }

  return SatAnswer::SAT;
}

uint
YalSAT::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

void
YalSAT::setPhase(const var_t var, const bool phase)
{
  yals_setphase(this->solver, (phase) ? var : -var);
}

bool
YalSAT::addClause(clause_view_t clause)
{
  for (int lit : clause) {
    yals_add(this->solver, lit);
  }
  yals_add(this->solver, 0);

  return true;
}

// void
// YalSAT::addInitialClauses(const std::vector<clause_t>& clauses, unsigned int
// nbVars)
// {
// 	if (clauses.size() > 33 * MILLION) {
// 		LOGERROR("The number of clauses %u is too high for YalSAT!",
// clauses.size()); 		exit(PERR_NOT_SUPPORTED);
// 	}
// 	for (auto clause : clauses) {
// 		for (int lit : clause) {
// 			yals_add(this->solver, lit);
// 		}
// 		yals_add(this->solver, 0);
// 	}

// 	this->setInitialized(true);

// 	this->clausesCount = clauses.size();
// 	LOG2("YalSAT %d loaded all the %d clauses with %u variables",
// this->getSolverId(), this->clausesCount, nbVars);
// }

// void
// YalSAT::addInitialClauses(const lit_t* literals, unsigned int clsCount,
// unsigned int nbVars)
// {
// 	this->clausesCount = 0;
// 	int lit;
// 	for (lit = *literals; this->clausesCount < clsCount; literals++,
// lit=*literals) { 		yals_add(this->solver, lit); 		if(!lit)
// this->clausesCount++;
// 	}

// 	this->setInitialized(true);

// 	LOG2("YalSAT %d loaded all the %d clauses with %u variables",
// this->getSolverId(), this->clausesCount, nbVars);
// }

void
YalSAT::loadFormula(const char* filename)
{
  unsigned int parsedVarCount;
  std::vector<std::vector<int>> clauses;
  if (!Parsers::parseCNF(filename, clauses, &parsedVarCount)) {
    PABORT(PERR_PARSING, "Error at parsing!");
  }
  for (auto& clause : clauses) {
    addClause(clause);
  }
}

model_t
YalSAT::getModel()
{
  std::vector<int> model;
  unsigned int varCount = this->getVariableCount();

  for (unsigned int i = 1; i <= varCount; i++) {
    /* yals_deref returns the best value */
    model.emplace_back(yals_deref(this->solver, i) > 0 ? i : -i);
  }

  return model;
}

std::string
YalSAT::statisticsToString()
{
  // yals_stats(this->solver);
  return "";
}

void
YalSAT::diversify(const SeedGenerator& getSeed)
{
  if (!getVariableCount()) {
    LOGERROR("Please call diversify after initializing the solver and adding "
             "the problem's clauses");
    exit(PERR_NOT_SUPPORTED);
  }

  yals_setflipslimit(this->solver, m_flipsLimit);

  // Seed the random number generator for the solver
  yals_srand(this->solver, getSeed(this));

  std::mt19937 rng_engine(std::random_device{}());
  std::uniform_int_distribution<int> uniform_dist(1, m_maxNoise);

  // Set options that influence the local search algorithm with random values

  // Walk probability: controls the probability of random walks during the local
  // search
  yals_setopt(
    this->solver, "walk", uniform_dist(rng_engine) > (m_maxNoise / 2));
  yals_setopt(this->solver,
              "walkprobability",
              (uniform_dist(rng_engine) * 100 + m_maxNoise) /
                m_maxNoise); /* shouldn't be < 1*/

  // Eager: controls whether to eagerly pick minimum break literals
  yals_setopt(this->solver, "eager", uniform_dist(rng_engine) % 2);

  // Unfair frequency: controls how often unfair picking strategy is used
  yals_setopt(
    this->solver, "unfairfreq", (uniform_dist(rng_engine) * 100) / m_maxNoise);

  // Reluctant: controls the reluctant doubling of the restart interval
  yals_setopt(this->solver, "reluctant", uniform_dist(rng_engine) % 2);

  // Dynamic break values (using critical literals)
  yals_setopt(this->solver, "crit", uniform_dist(rng_engine) % 2);

  // Geometric picking frequency: determines how often the geometric picking
  // strategy is used
  yals_setopt(
    this->solver, "geomfreq", (uniform_dist(rng_engine) * 100) / m_maxNoise);

  // Polarity: controls the polarity of literals
  yals_setopt(this->solver, "pol", uniform_dist(rng_engine) % 3 - 1);

  // Clause picking strategy for uniform formulas
  yals_setopt(this->solver, "unipick", uniform_dist(rng_engine) % 6 - 1);

  // Fixed strategy frequency: determines how often the fixed strategy is
  // applied
  yals_setopt(this->solver,
              "fixed",
              uniform_dist(rng_engine) % 5 +
                1); /* yals_rand_mod do not accept zero*/

  // Restart interval: basic (inner) restart interval
  yals_setopt(this->solver, "restart", uniform_dist(rng_engine) + 100000);

  // Outer restart interval factor: enables the outer restart interval factor
  yals_setopt(this->solver, "restartouter", uniform_dist(rng_engine) % 2);
  yals_setopt(
    this->solver, "restartouterfactor", uniform_dist(rng_engine) + 50);

  // Critical literals setting
  yals_setopt(this->solver, "crit", uniform_dist(rng_engine) % 2);

  // Correct CB value depending on maximum length
  yals_setopt(this->solver, "correct", uniform_dist(rng_engine) % 2);

  /* Restarts */
  yals_setopt(this->solver, "cached", 0);
  int noise = uniform_dist(rng_engine);
  if (noise < m_maxNoise / 3) {
    yals_setopt(this->solver, "best", 1);
  } else if (noise > m_maxNoise > 2 * (m_maxNoise / 3)) {
    yals_setopt(this->solver, "cacheduni", 1);
  } else {
    yals_setopt(this->solver, "cached", 1);
  }

  LOG2("Diversification of YalSAT(%d,%u) done",
       this->getSolverId(),
       this->getSolverTypeId());
}

void
YalSAT::setOption(const std::string& key, int value)
{
  if (key == "flips-limit")
    m_flipsLimit = value;
  else if (key == "max-noise")
    m_maxNoise = value;
  else
    PABORTIF(!yals_setopt(this->solver, key.c_str(), value),
             PERR_ARGS,
             "Int Option %s is not recognized by YalSAT!",
             key.c_str());
}

void
YalSAT::setOption(const std::string& key, double value)
{
  long castedValue = static_cast<long>(value);
  if (key == "flips-limit")
    m_flipsLimit = castedValue;
  else if (key == "max-noise")
    m_maxNoise = castedValue;
  else
    PABORT(PERR_ARGS,
           "Double Option %s is not recognized by YalSAT!",
           key.c_str());
}
