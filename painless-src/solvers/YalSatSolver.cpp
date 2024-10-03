#include "YalSatSolver.h"
#include "utils/SatUtils.h"
#include "utils/Parameters.h"
#include "utils/ErrorCodes.h"
#include "utils/System.h"

/* For termination callback check */
int yalsat_terminate(void *p_YalSat)
{
    YalSat *cpp_YalSat = (YalSat *)p_YalSat;
    if (cpp_YalSat->terminateSolver)
        return 1;
    else
        return 0;
}

YalSat::YalSat(int _id) : LocalSearchSolver(_id, LocalSearchType::YALSAT)
{
    // Yals * yals_new ();
    this->solver = yals_new();
    yals_seterm(this->solver, yalsat_terminate, this);
    this->clausesCount = 0;
}

YalSat::~YalSat()
{
    // void yals_del (Yals *);
    yals_del(this->solver);
    LOGDEBUG1("Yalsat %d deleted!", this->getId());
}

int YalSat::getVariablesCount()
{
    // -1 because of if (idx >= yals->nvars) yals->nvars = idx + 1; in yals_add
    return yals_getnvars(this->solver) - 1;
}

int YalSat::getDivisionVariable()
{
    // TODO: a better division according to the trail (read about divide and conquer)
    return (rand() % getVariablesCount()) + 1;
}

void YalSat::setSolverInterrupt()
{
    if (!this->terminateSolver)
    {
        LOG1("Asked Yalsat %d to terminate", this->id);
        this->terminateSolver = true;
    }
}

void YalSat::unsetSolverInterrupt()
{
    this->terminateSolver = false;
}

SatResult YalSat::solve(const std::vector<int> &cube)
{
    int res;
    // int yals_sat (Yals *); // do unit propagation as preprocessing
    // set phases using cube
    for (int lit : cube)
    {
        yals_setphase(this->solver, lit);
    }

    res = yals_sat(this->solver);

    this->lsStats.numberFlips = yals_flips(this->solver);
    this->lsStats.numberUnsatClauses = yals_minimum(this->solver);
    
    LOG1("[YalSat %d] Number of remaining unsats %d / %d, Number of Flips %d.", this->id, this->lsStats.numberUnsatClauses, this->clausesCount, this->lsStats.numberFlips);

    if (SatResult::SAT != res)
    {
        return SatResult::UNKNOWN;
    }

    return SatResult::SAT;
}

void YalSat::setPhase(const unsigned var, const bool phase)
{
    yals_setphase(this->solver, (phase) ? var : -var);
}

void YalSat::addClause(std::shared_ptr<ClauseExchange> clause)
{
    for (int lit : clause->lits)
    {
        yals_add(this->solver, lit);
    }
    yals_add(this->solver, 0);
}

void YalSat::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto clause : clauses)
    {
        addClause(clause);
    }
}

void YalSat::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
    if(clauses.size() > 33 * MILLION)
    {
        LOGERROR("The number of clauses %u is too high for yalsat!", clauses.size());
        exit(PERR_NOT_SUPPORTED);
    }
    for (auto clause : clauses)
    {
        for (int lit : clause)
        {
            yals_add(this->solver, lit);
        }
        yals_add(this->solver, 0);
    }

    this->initialized = true;

    this->clausesCount = clauses.size();
    LOG1("Yalsat %d loaded all the %d clauses with %u variables", this->id, this->clausesCount, nbVars);
}

void YalSat::loadFormula(const char *filename)
{
    unsigned parsedVarCount;
    std::vector<std::vector<int>> clauses;
    parseFormula(filename, clauses, &parsedVarCount);
    this->addInitialClauses(clauses, parsedVarCount);
}

std::vector<int> YalSat::getModel()
{
    std::vector<int> model;
    unsigned varCount = this->getVariablesCount();

    for (int i = 1; i <= varCount; i++)
    {
        /* yals_deref returns the best value */
        model.emplace_back(yals_deref(this->solver, i) > 0 ? i : -i);
    }

    return model;
}

void YalSat::printStatistics()
{
    yals_stats(this->solver);
}

void YalSat::printParameters()
{
    // void yals_usage (Yals *); to be used in printHelp
    yals_showopts(this->solver);
}

void YalSat::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
    if (!getVariablesCount())
    {
        LOGERROR("Please call diversify after initializing the solver and adding the problem's clauses");
        exit(PERR_NOT_SUPPORTED);
    }
    // void yals_srand (Yals *, unsigned long long seed);
    yals_srand(this->solver, this->id);
    // int yals_setopt (Yals *, const char * name, int val);
    // yals_setopt(this->solver, "walk", uniform_dist(rng_engine) % 2);
    // void yals_setflipslimit (Yals *, long long);
    yals_setflipslimit(this->solver, Parameters::getIntParam("ls-flips", -1));
    // void yals_setmemslimit (Yals *, long long);
}