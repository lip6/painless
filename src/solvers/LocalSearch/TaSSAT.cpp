#include "TaSSAT.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"


TaSSAT::TaSSAT(int _id,
               FullClauseReader fullReader)
  : mcbk_fullReader(fullReader)
  , LocalSearchInterface(_id, LocalSearchType::TASSAT)
{
  // Yals * tass_new ();
  initializeTypeId<TaSSAT>();
  this->solver = tass_new();

  tass_seterm(
    solver,
    [](void* cpp) {
      TaSSAT* cppWrapper = reinterpret_cast<TaSSAT*>(cpp);
      if (cppWrapper->terminateSolver)
        return 1;
      else
        return 0;
    },
    this);
}

TaSSAT::TaSSAT(int _id,
               unsigned long flipsLimit,
               unsigned long maxNoise,
               FullClauseReader fullReader)
  : m_flipsLimit(flipsLimit)
  , m_maxNoise(maxNoise)
  , mcbk_fullReader(fullReader)
  , m_fullReaderIndex(0)
  , LocalSearchInterface(_id, LocalSearchType::TASSAT)
{
  // Yals * tass_new ();
  initializeTypeId<TaSSAT>();
  this->solver = tass_new();

  tass_seterm(
    solver,
    [](void* cpp) {
      TaSSAT* cppWrapper = reinterpret_cast<TaSSAT*>(cpp);
      if (cppWrapper->terminateSolver)
        return 1;
      else
        return 0;
    },
    this);

  // Default options
  m_enableMultiplePicks = true;
  m_multiplePicksUnsatThreshold = 10'000;

  m_initpct = 1.f;
  m_basepct = 0.175f;
  m_currpct = 0.075f;
  m_initialweight = 100.f;
  m_randomPick = 0.1f;
}

TaSSAT::~TaSSAT()
{
  // void tass_del (Yals *);
  tass_del(this->solver);
}

uint
TaSSAT::getVariableCount()
{
  // -1 because of: if (idx >= yals->nvars) yals->nvars = idx + 1; in tass_add
  return tass_get_var_count(this->solver) - 1;
}

var_t
TaSSAT::getDivisionVariable()
{
  // TODO: a better division according to the trail (read about divide and
  // conquer)
  return (rand() % getVariableCount()) + 1;
}

void
TaSSAT::setSolverInterrupt()
{
  if (!this->terminateSolver) {
    LOG1("Asked TaSSAT %d to terminate", this->getSolverId());
    this->terminateSolver = true;
  }
}

void
TaSSAT::unsetSolverInterrupt()
{
  this->terminateSolver = false;
}

// Todo use this instead of built one in order to sort out the negative weights
// by not substracting the inflation from the victim clause
float
TaSSAT::tassatWeightToTransfer(const float victimWeight)
{
  if (victimWeight == TaSSAT::m_initialweight)
    return TaSSAT::m_initpct * TaSSAT::m_initialweight;
  else
    return TaSSAT::m_currpct * victimWeight +
           TaSSAT::m_basepct * TaSSAT::m_initialweight;
}

SatAnswer
TaSSAT::solve(cube_view_t cube)
{
  uint readClauses = loadClauses();

  PABORTIF(cube.size(),
           PERR_BAD_BEHAVIOR,
           "TaSSAT doesn't offer assumptions (To be added)");

  this->m_initialized = true;

  LOG2("Launching TaSSAT %u with %u clauses (%u added this round), and %u "
       "assumptions",
       getSolverTypeId(),
       m_fullReaderIndex,
       readClauses,
       cube.size());

  // do unit propagation as preprocessing
  SatAnswer result =
    static_cast<SatAnswer>(tass_init(solver, static_cast<char>(true)));
  if (result == SatAnswer::SAT)
    return result;

  int res = tass_sat(this->solver);

  this->lsStats.numberUnsatClauses = tass_nunsat_external(solver);

  LOGSTAT(
    "[TaSSAT %u] finished with %u tries, %lld flips, and %d remaining unsats",
    this->getSolverTypeId(),
    1,
    tass_flips(solver),
    tass_minimum(solver));

  return static_cast<SatAnswer>(res);
}

void
TaSSAT::setPhase(const var_t var, const bool phase)
{
  tass_setphase(this->solver, (phase) ? var : -var);
}

bool
TaSSAT::addClause(clause_view_t clause)
{
  for (int lit : clause) {
    tass_add(this->solver, lit);
  }
  tass_add(this->solver, 0);

  // The moment a clause is added, the solver is considered initialized (cannot
  // change some options)
  if (!this->m_initialized)
    this->m_initialized = true;

  return true;
}

void
TaSSAT::loadFormula(const char* filename)
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

uint
TaSSAT::loadClauses()
{
  uint oldIndex = m_fullReaderIndex;
  m_fullReaderIndex = mcbk_fullReader(
    [this](clause_view_t clause) -> bool { return this->addClause(clause); },
    m_fullReaderIndex);
  // m_fullReaderIndex is now equal to the number of clauses read in total

  return m_fullReaderIndex - oldIndex;
}

model_t
TaSSAT::getModel()
{
  std::vector<int> model;
  unsigned int varCount = this->getVariableCount();

  for (unsigned int i = 1; i <= varCount; i++) {
    /* tass_deref returns the best value */
    model.emplace_back(tass_deref(this->solver, i) > 0 ? i : -i);
  }

  return model;
}

std::string
TaSSAT::statisticsToString()
{
  return "";
}

void
TaSSAT::diversify(const SeedGenerator& getSeed)
{
  if (!getVariableCount()) {
    LOGERROR("Please call diversify after initializing the solver and adding "
             "the problem's clauses");
    exit(PERR_NOT_SUPPORTED);
  }

  tass_setflipslimit(this->solver, m_flipsLimit);

  // Seed the random number generator for the solver
  tass_srand(this->solver, getSeed(this));

  std::mt19937 rng_engine(std::random_device{}());
  std::uniform_int_distribution<int> uniform_dist(1, m_maxNoise);

  // Todo use c++ weight compute for better management
  // The initial weight is defined as a macro #define BASE_WEIGHT 100.0

  tass_setopt(this->solver, "currpmille", 75);
  tass_setopt(this->solver, "basepmille", 175);
  tass_setopt(this->solver, "initpmille", 1000);

  /* Restarts */
  tass_setopt(this->solver, "cached", 0);
  int noise = uniform_dist(rng_engine);
  if (noise < m_maxNoise / 3) {
    tass_setopt(this->solver, "best", 1);
  } else if (noise > m_maxNoise > 2 * (m_maxNoise / 3)) {
    tass_setopt(this->solver, "cacheduni", 1);
  } else {
    tass_setopt(this->solver, "cached", 1);
  }

  LOG2("Diversification of TaSSAT(%d,%u) done",
       this->getSolverId(),
       this->getSolverTypeId());
}

SatAnswer
TaSSAT::simpleInnerLoop()
{
  int res = 0;
  tass_init_inner_restart_interval(solver);
  LOGD1("Entering yals inner loop");

  while (!(res = tass_done(solver)) && !tass_need_to_restart_outer(solver) &&
         !this->terminateSolver) {
    if (tass_need_to_restart_inner(solver)) {
      tass_restart_inner(solver);
      if (!tass_getopt(solver, "liwetonly"))
        tass_disable_liwet(solver);
    } else {
      if (!tass_is_liwet_active(solver) && tass_needs_liwet(solver))
        tass_enable_liwet(solver);
      // if (tass_is_liwet_active(solver) && tass_needs_probsat (solver))
      //   tass_disable_liwet(solver);
      if (tass_is_liwet_active(solver)) {

        // save_stats_lm (solver);

        // At maximum we take the number of unsat clauses
        int maxToPick = tass_nunsat_external(solver);

        // if we do not reach the threshold (or is disabled) we will make a
        // simple flip
        if (maxToPick < m_multiplePicksUnsatThreshold ||
            !m_enableMultiplePicks) {
          tass_liwet_compute_uwrvs(solver);
          maxToPick = tass_liwet_get_uwrvs_size(solver);

          // If a positif was found the best will be flipped (default TaSSAT
          // behavior)
          if (maxToPick) {
            int lit = tass_pick_literal_liwet(solver);
            tass_flip_liwet(solver, lit);
            LOGD2("1Flip Mode: %d", lit);
            lsStats.numberFlips++;
          } // Else the maxToPick is zero thus the transfer weight condition
            // will be enabled

        }
        // In case we need to do the multiple pick since we have a great number
        // of unsat clauses
        else if (maxToPick >= m_multiplePicksUnsatThreshold) {

          // TaSSAT manages arrays for all the literals having a positif gain if
          // flipped. In this vector we store the indices to the N best literals
          // in these arrays
          picksIndices.resize(maxToPick);
          // Custom liwe_compute that just updates our vector using a c lower
          // bound algorithm to sort at
          tass_liwet_compute_uwrvs_top_n(
            solver, picksIndices.data(), maxToPick);

          // Count only none -1 indices
          auto newEnd = std::lower_bound(
            picksIndices.begin(), picksIndices.end(), -1, std::greater<int>());
          picksIndices.resize(newEnd - picksIndices.begin());
          maxToPick = picksIndices.size();

#ifndef NDEBUG
          printf("Multiple Picks Mode: ");
          for (int i : picksIndices) {
            int currLit = 0;
            double currLitScore = tass_liwet_get_lit_gain(solver, i, &currLit);
            printf(" %d (lit: %d, gain: %lf), ", i, currLit, currLitScore);
            assert(i >= 0 && currLitScore >= 0);
          }
          printf("\n");
#endif

          std::vector<bool> isLitToFlip(maxToPick, false);

          // Half chance to flips
          std::bernoulli_distribution flipLit(0.5);

          for (int i = 0; i < maxToPick; i++) {
            isLitToFlip[i] = flipLit(rng_engine);
          }

          // Flip selected
          uint flippedAtLeastOne = 0;
          for (int i = 0; i < maxToPick; i++) {
            if (isLitToFlip[i]) {
              int lit = tass_liwet_get_positive_lit(solver, picksIndices[i]);
              flippedAtLeastOne++;
              LOGD2("Flipping literal %d", lit);
              tass_flip_liwet(solver, lit);
            }
          }

          if (!flippedAtLeastOne) {
            // If no flip is to be made, force the system to update the weights
            // to move on
            maxToPick = 0;
          } else {
            // Number of flips is incremented by the number of flipped vars
            lsStats.numberFlips += flippedAtLeastOne;
          }
        }

        if (0 == maxToPick) {
          LOGD2("Weight Update");
          tass_liwet_transfer_weights(solver);
          continue; // No flip
        } else
          m_descentsCount++;
      } else // Default TaSSAT (inherited from the inner loop of tassat)
        tass_flip(solver);
    }
  }
  return static_cast<SatAnswer>(res);
}

void
TaSSAT::setOption(const std::string& key, int value)
{
  if (key == "flips-limit")
    m_flipsLimit = value;
  else if (key == "max-noise")
    m_maxNoise = value;
  else
    PABORTIF(!tass_setopt(this->solver, key.c_str(), value),
             PERR_ARGS,
             "Int Option %s is not recognized by TaSSAT!",
             key.c_str());
}

void
TaSSAT::setOption(const std::string& key, double value)
{
  long castedValue = static_cast<long>(value);
  if (key == "flips-limit")
    m_flipsLimit = castedValue;
  else if (key == "max-noise")
    m_maxNoise = castedValue;
  else
    PABORT(PERR_ARGS,
           "Double Option %s is not recognized by TaSSAT!",
           key.c_str());
}