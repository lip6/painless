#include "TaSSAT.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

TaSSAT::TaSSAT(int _id, unsigned long flipsLimit, unsigned long maxNoise)
	: m_flipsLimit(flipsLimit)
	, m_maxNoise(maxNoise)
	, LocalSearchInterface(_id, LocalSearchType::TASSAT)
{
	// Yals * tass_new ();
	initializeTypeId<TaSSAT>();
	this->myyals = tass_new();
	this->clausesCount = 0;

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
	tass_del(this->myyals);
	LOGDEBUG1("TaSSAT %d deleted!", this->getSolverId());
}

unsigned int
TaSSAT::getVariablesCount()
{
	// -1 because of: if (idx >= yals->nvars) yals->nvars = idx + 1; in tass_add
	return tass_get_var_count(this->myyals) - 1;
}

int
TaSSAT::getDivisionVariable()
{
	// TODO: a better division according to the trail (read about divide and conquer)
	return (rand() % getVariablesCount()) + 1;
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

// Todo use this instead of built one in order to sort out the negative weights by not substracting the inflation from the victim clause
float
TaSSAT::tassatWeightToTransfer(const float victimWeight)
{
	if (victimWeight == TaSSAT::m_initialweight)
		return TaSSAT::m_initpct * TaSSAT::m_initialweight;
	else
		return TaSSAT::m_currpct * victimWeight + TaSSAT::m_basepct * TaSSAT::m_initialweight;
}

// std::unordered_map<ClikeClause, clsweight_t, ClauseUtils::ClauseHash> TaSSAT::getClauseWeights()
// {
// 	for(uint i = 0; i < this->clausesCount; i++)
// 	{

// 	}
// }

SatResult
TaSSAT::solve(const std::vector<int>& cube)
{
	if (!clausesCount) {
		LOGWARN("No Clause was added, returning SAT from solver");
		return SatResult::SAT;
	}

	// do unit propagation as preprocessing
	SatResult result = static_cast<SatResult>(tass_init(myyals, static_cast<char>(true)));
	if (result == SatResult::SAT)
		return result;

	// set phases using cube
	for (int lit : cube) {
		tass_setphase(this->myyals, lit);
	}

	lsStats.numberFlips = 0;
	m_descentsCount = 0;

	tass_init_outer_restart_interval(myyals);
	LOGDEBUG1("After Outer Loop Init");
	while (!this->terminateSolver) {
		tass_init_one_outer_iteration(myyals);
		LOGDEBUG1("After One Outer Loop Iteration Init");
		if (tass_need_to_run_max_tries(myyals)) {
			PABORT(-1, "Not Implemented, yet!");
		} else {
			if (this->simpleInnerLoop() != SatResult::UNKNOWN)
				break;
		}
		tass_restart_outer(myyals);
	}

	int res = tass_get_res(myyals);

	this->lsStats.numberUnsatClauses = tass_nunsat_external(myyals);

	LOGSTAT("[TaSSAT %d] Number of remaining unsats %d / %d, Number of Flips %d.",
			this->getSolverId(),
			this->lsStats.numberUnsatClauses,
			this->clausesCount,
			this->lsStats.numberFlips);

	return static_cast<SatResult>(res);
}

void
TaSSAT::setPhase(const unsigned int var, const bool phase)
{
	tass_setphase(this->myyals, (phase) ? var : -var);
}

void
TaSSAT::addClause(ClauseExchangePtr clause)
{
	for (int lit : clause->lits) {
		tass_add(this->myyals, lit);
	}
	tass_add(this->myyals, 0);
}

void
TaSSAT::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto clause : clauses) {
		addClause(clause);
	}
}

void
TaSSAT::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	if (clauses.size() > 33 * MILLION) {
		LOGERROR("The number of clauses %u is too high for TaSSAT!", clauses.size());
		exit(PERR_NOT_SUPPORTED);
	}
	for (auto clause : clauses) {
		for (int lit : clause) {
			tass_add(this->myyals, lit);
		}
		tass_add(this->myyals, 0);
	}

	this->setInitialized(true);

	this->clausesCount = clauses.size();
	LOG2("TaSSAT %d loaded all the %d clauses with %u variables", this->getSolverId(), this->clausesCount, nbVars);
}

void
TaSSAT::addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars)
{
	this->clausesCount = 0;
	int lit;
	for (lit = *literals; this->clausesCount < clsCount; literals++, lit=*literals) {
		tass_add(this->myyals, lit);
		if(!lit)
			this->clausesCount++;
	}

	this->setInitialized(true);

	LOG2("TaSSAT %d loaded all the %d clauses with %u variables", this->getSolverId(), this->clausesCount, nbVars);
}

void
TaSSAT::loadFormula(const char* filename)
{
	unsigned int parsedVarCount;
	std::vector<std::vector<int>> clauses;
	if (!Parsers::parseCNF(filename, clauses, &parsedVarCount)) {
		PABORT(PERR_PARSING, "Error at parsing!");
	}
	this->addInitialClauses(clauses, parsedVarCount);
}

std::vector<int>
TaSSAT::getModel()
{
	std::vector<int> model;
	unsigned int varCount = this->getVariablesCount();

	for (unsigned int i = 1; i <= varCount; i++) {
		/* tass_deref returns the best value */
		model.emplace_back(tass_deref(this->myyals, i) > 0 ? i : -i);
	}

	return model;
}

void
TaSSAT::printStatistics()
{
	tass_stats(this->myyals);
}

void
TaSSAT::printParameters()
{
	// void tass_usage (Yals *); to be used in printHelp
	tass_showopts(this->myyals);
	LOG0("MultiplePicks (%s, %u), InitialWeight: %lf, Initcpt: %lf, Basecpt: %lf, Currcpt: %lf, RandomPick: %lf",
		 m_enableMultiplePicks ? "Enabled" : "Disabled",
		 m_multiplePicksUnsatThreshold,
		 m_initialweight,
		 m_initpct,
		 m_basepct,
		 m_currpct,
		 m_randomPick);
}

void
TaSSAT::diversify(const SeedGenerator& getSeed)
{
	if (!getVariablesCount()) {
		LOGERROR("Please call diversify after initializing the myyals and adding the problem's clauses");
		exit(PERR_NOT_SUPPORTED);
	}

	tass_setflipslimit(this->myyals, m_flipsLimit);

	// Seed the random number generator for the myyals
	tass_srand(this->myyals, getSeed(this));

	std::mt19937 rng_engine(std::random_device{}());
	std::uniform_int_distribution<int> uniform_dist(1, m_maxNoise);

	// Todo use c++ weight compute for better management
	// The initial weight is defined as a macro #define BASE_WEIGHT 100.0

	tass_setopt(this->myyals, "currpmille", 75);                                            
    tass_setopt(this->myyals, "basepmille", 175);                                              
    tass_setopt(this->myyals, "initpmille", 1000);
	
	/* Restarts */
	tass_setopt(this->myyals, "cached", 0);
	int noise = uniform_dist(rng_engine);
	if (noise < m_maxNoise / 3) {
		tass_setopt(this->myyals, "best", 1);
	} else if (noise > m_maxNoise > 2 * (m_maxNoise / 3)) {
		tass_setopt(this->myyals, "cacheduni", 1);
	} else {
		tass_setopt(this->myyals, "cached", 1);
	}

	LOG2("Diversification of TaSSAT(%d,%u) done", this->getSolverId(), this->getSolverTypeId());
}

SatResult
TaSSAT::simpleInnerLoop()
{
	int res = 0;
	tass_init_inner_restart_interval(myyals);
	LOGDEBUG1("Entering yals inner loop");

	while (!(res = tass_done(myyals)) && !tass_need_to_restart_outer(myyals) && !this->terminateSolver) {
		if (tass_need_to_restart_inner(myyals)) {
			tass_restart_inner(myyals);
			if (!tass_getopt(myyals, "liwetonly"))
				tass_disable_liwet(myyals);
		} else {
			if (!tass_is_liwet_active(myyals) && tass_needs_liwet(myyals))
				tass_enable_liwet(myyals);
			// if (tass_is_liwet_active(myyals) && tass_needs_probsat (myyals))
			//   tass_disable_liwet(myyals);
			if (tass_is_liwet_active(myyals)) {

				// save_stats_lm (myyals);

				// At maximum we take the number of unsat clauses
				int maxToPick = tass_nunsat_external(myyals);

				// if we do not reach the threshold (or is disabled) we will make a simple flip
				if (maxToPick < m_multiplePicksUnsatThreshold || !m_enableMultiplePicks) {
					tass_liwet_compute_uwrvs(myyals);
					maxToPick = tass_liwet_get_uwrvs_size(myyals);

					// If a positif was found the best will be flipped (default TaSSAT behavior)
					if (maxToPick) {
						int lit = tass_pick_literal_liwet(myyals);
						tass_flip_liwet(myyals, lit);
						LOGDEBUG2("1Flip Mode: %d", lit);
						lsStats.numberFlips++;
					} // Else the maxToPick is zero thus the transfer weight condition will be enabled

				}
				// In case we need to do the multiple pick since we have a great number of unsat clauses
				else if (maxToPick >= m_multiplePicksUnsatThreshold) {

					// TaSSAT manages arrays for all the literals having a positif gain if flipped. In this vector we
					// store the indices to the N best literals in these arrays
					picksIndices.resize(maxToPick);
					// Custom liwe_compute that just updates our vector using a c lower bound algorithm to sort at
					tass_liwet_compute_uwrvs_top_n(myyals, picksIndices.data(), maxToPick);

					// Count only none -1 indices
					auto newEnd = std::lower_bound(picksIndices.begin(), picksIndices.end(), -1, std::greater<int>());
					picksIndices.resize(newEnd - picksIndices.begin());
					maxToPick = picksIndices.size();

#ifndef NDEBUG
					printf("Multiple Picks Mode: ");
					for (int i : picksIndices) {
						int currLit = 0;
						double currLitScore = tass_liwet_get_lit_gain(myyals, i, &currLit);
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
							int lit = tass_liwet_get_positive_lit(myyals, picksIndices[i]);
							flippedAtLeastOne++;
							LOGDEBUG2("Flipping literal %d", lit);
							tass_flip_liwet(myyals, lit);
						}
					}

					if (!flippedAtLeastOne) {
						// If no flip is to be made, force the system to update the weights to move on
						maxToPick = 0;
					} else {
						// Number of flips is incremented by the number of flipped vars
						lsStats.numberFlips += flippedAtLeastOne;
					}
				}

				if (0 == maxToPick) {
					LOGDEBUG2("Weight Update");
					tass_liwet_transfer_weights(myyals);
					continue; // No flip
				} else
					m_descentsCount++;
			} else // Default TaSSAT (inherited from the inner loop of tassat)
				tass_flip(myyals);
		}
	}
	return static_cast<SatResult>(res);
}