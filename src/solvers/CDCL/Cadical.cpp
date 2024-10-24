#include "painless.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include <cassert>
#include <iomanip>

#include "Cadical.hpp"

/*------------------------Learner-------------------------*/

bool
Cadical::learning(int size, int glue)
{
	if (size > 0) {
		LOGDEBUG3("Cadical %d will export clause of size %d, glue %d", this->getSolverId(), size, glue);
		tempClause.reserve(size);
		this->lbd = glue;
		return true;
	} else {
		return false;
	}
}

void
Cadical::learn(int lit)
{
	if (lit)
		tempClause.push_back(lit);
	else {
		assert(tempClause.size() > 0 && this->lbd >= 0);
		auto exportedClause = ClauseExchange::create(tempClause, this->lbd, this->getSharingId());

		assert(tempClause.size() == exportedClause->size);
		assert((exportedClause->size > 1 && exportedClause->lbd > 0) ||
			   (exportedClause->size == 1 && exportedClause->lbd >= 0));

		/* filtering defined by a sharing strategy, done here in case it checks its literals */
		if (this->exportClause(exportedClause)) {
			LOGCLAUSE2(exportedClause->lits,
					   exportedClause->size,
					   "Cadical %d exported Clause %p for sharing",
					   this->getSolverId(),
					   exportedClause.get());
		}
		tempClause.clear();
	}
}

bool
Cadical::hasClauseToImport()
{
	if (this->m_clausesToImport->getOneClause(tempClauseToImport)) {
		LOGDEBUG2("Cadical %u will import clause %s", this->getSharingId(), tempClauseToImport->toString().c_str());
		return true;
	} else {
		m_clausesToImport->shrinkDatabase();
		return false;
	}
}

void
Cadical::getClauseToImport(std::vector<int>& clause, int& glue)
{
	assert((tempClauseToImport->size > 1 && tempClauseToImport->lbd > 0) ||
		   (tempClauseToImport->size == 1 && tempClauseToImport->lbd >= 0));

	assert(clause.empty());
	clause.insert(clause.end(), tempClauseToImport->begin(), tempClauseToImport->end());
	assert(clause.size() && clause.size() == tempClauseToImport->size && clause[0] == tempClauseToImport->lits[0]);

	glue = tempClauseToImport->lbd;
	LOGCLAUSE2(clause.data(), clause.size(), "Cadical %d will import Clause (lbd:%u)", this->getSolverId(), glue);
}

/*----------------------Main Class------------------------*/

Cadical::Cadical(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::CADICAL)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
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

/* Execution */

SatResult
Cadical::solve(const std::vector<int>& cube)
{
	if (!this->isInitialized()) {
		LOGWARN("Cadical %d was not initialized to be launched!", this->getSolverId());
		return SatResult::UNKNOWN;
	}
	unsetSolverInterrupt();

	/* use add to add unit clauses for permanent assumption */
	for (int lit : cube)
		solver->assume(lit);

	int res = solver->solve();

	if (res == 10) {
		LOG2("Cadical %d responded with SAT", this->getSolverId());
		return SatResult::SAT;
	}
	if (res == 20) {
		LOG2("Cadical %d responded with UNSAT", this->getSolverId());
		return SatResult::UNSAT;
	}
	LOGDEBUG2("Cadical %d responded with %d (UNKNOWN)", this->getSolverId(), res);
	return SatResult::UNKNOWN;
}

void
Cadical::setSolverInterrupt()
{
	this->stopSolver = true;
	LOGDEBUG1("Asking Cadical (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());
}

void
Cadical::unsetSolverInterrupt()
{
	this->stopSolver = false;
}

void
Cadical::initCadicalOptions()
{
#ifndef NDEBUG
	cadicalOptions.insert({ "quiet", 0 });
#else
	cadicalOptions.insert({ "quiet", 1 });
#endif
	cadicalOptions.insert({ "stabilize", 1 });
	cadicalOptions.insert({ "stabilizeonly", 0 });
	/*
	 * target = 1: in stable phase only
	 * target = 2: always choose target
	 */
	cadicalOptions.insert({ "target", 1 });

	cadicalOptions.insert({ "elimreleff", 1e3 });
	cadicalOptions.insert({ "subsumereleff", 1e3 });

	// Shuffling
	cadicalOptions.insert({ "shuffle", 0 });	   /* shuffle variables */
	cadicalOptions.insert({ "shufflequeue", 0 });  /* shuffle variable queue */
	cadicalOptions.insert({ "shufflescores", 0 }); /* shuffle variable queue */
	cadicalOptions.emplace("shufflerandom",0);

	// Random Walks
	cadicalOptions.insert({ "walkredundant", 0 });
	cadicalOptions.insert({ "walknonstable", 1 });
	cadicalOptions.insert({ "walk", 1 });

	// Search Configuration
	cadicalOptions.insert({ "chrono", 1 });
	cadicalOptions.insert({ "chronoalways", 0 });
	cadicalOptions.insert({ "chronolevelim", 100 });

	// Restart Management
	cadicalOptions.insert({ "restart", 1 });
	cadicalOptions.insert({ "restartint", 1 });

	// Decision
	cadicalOptions.insert({ "score", 1 }); // 1: VSIDS, 0: no score computing

	// Phase
	cadicalOptions.insert({ "phase", 1 });
	cadicalOptions.insert({ "rephase", 1 });
	cadicalOptions.insert({ "rephaseint", 1e3 });
	cadicalOptions.insert({ "forcephase", 0 });

	// Simplification Techniques
	cadicalOptions.insert({ "block", 0 }); /* Blocked clause elimination */
	cadicalOptions.insert({ "elim", 1 });  /* Bounded Variable elimination */
	cadicalOptions.insert({ "otfs", 1 });  /* on the fly subsumption */
	cadicalOptions.emplace("condition", 0); /* globally blocked clause elimination */
	cadicalOptions.emplace("cover", 0); /* covered clause elimination */
	cadicalOptions.emplace("inprocessing" ,1); /* enable inprocessing (search is stopped, simplification is resumed)*/

	// Learnt Clauses
	cadicalOptions.insert({ "reducetier1glue", 2 });
	cadicalOptions.insert({ "reducetier2glue", 6 });

	// random seed
	cadicalOptions.insert({ "seed", 0 });
			

	// Setting the options
	for (auto& opt : cadicalOptions) {
		solver->set(opt.first.c_str(), opt.second);
	}
}

static std::mt19937 engine;
static std::uniform_int_distribution<unsigned> uniform(0, 100);

void
Cadical::diversify(const SeedGenerator& getSeed)
{
	unsigned int typeId = this->getSolverTypeId();
	unsigned int generalSeed = getSeed(this);

	unsigned int cadicalCount = this->getSolverTypeCount();

	cadicalOptions.at("seed") = generalSeed;
	cadicalOptions.at("phase") = generalSeed % 2;

	LOGDEBUG1("Diversification of Cadical (%d,%d)", this->getSolverId(), typeId);
	// From Mallob Native Diversification
	switch (typeId % 10) {
		case 0:
			cadicalOptions.at("phase") = 0;
			break;
		case 1:
			solver->configure("sat");
			// 25% chances
			if (uniform(engine) < 25) {
				cadicalOptions.at("walk") = 1;
				cadicalOptions.at("target") = 2;
				cadicalOptions.at("chrono") = 1;
				cadicalOptions.at("chronoalways") = 1;
				cadicalOptions.at("walkredundant") = 1;
				cadicalOptions.at("reducetier1glue") = 2;
				cadicalOptions.at("reducetier2glue") = 3 + generalSeed % 2;
				cadicalOptions.at("restartint") = 90 + (1 + typeId % 10);
			}
			break;
		case 2:
			cadicalOptions.at("elim") = 0;
			break;
		case 3:
			solver->configure("unsat");
			cadicalOptions.at("restartint") = 1;
			if (uniform(engine) < 25) {
				cadicalOptions.at("target") = 0;
				cadicalOptions.at("chrono") = 0;
			}
			break;
		case 4:
			cadicalOptions.at("condition") = 1;
			break;
		case 5:
			cadicalOptions.at("walk") = 0;
			break;
		case 6:
			cadicalOptions.at("restartint") = 100;
			break;
		case 7:
			cadicalOptions.at("cover") = 1;
			break;
		case 8:
			cadicalOptions.at("shuffle") = 1;
			cadicalOptions.at("shufflerandom") = 1;
			break;
		case 9:
			cadicalOptions.at("inprocessing") = 0;
			break;
	}

	// Setting the options
	for (auto& opt : cadicalOptions) {
		assert(solver->set(opt.first.c_str(), opt.second));
	}
}

/* Clause Management */

void
Cadical::loadFormula(const char* filename)
{
	int nbVars;
	int strict = 2;
	solver->read_dimacs(filename, nbVars, strict);
	this->setInitialized(true);
}

void
Cadical::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	solver->reserve(nbVars);

	for (auto& clause : clauses) {
		solver->clause(clause);
		// for (int lit : clause)
		//     solver->add(lit);
		// solver->add(0);
	}
	this->setInitialized(true);
	LOG2("The Cadical Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
Cadical::addClause(ClauseExchangePtr clause)
{
	unsigned int maxVar = solver->vars();
	for (int lit : clause->lits) {
		if (std::abs(lit) > maxVar) {
			LOGERROR("[Cadical %d] literal %d is out of bound, maxVar is %d", this->getSolverId(), lit, maxVar);
			return;
		}
	}

	solver->clause(clause->lits, clause->size);
	// for (int lit : clause->lits)
	//     solver->add(lit);
	// solver->add(0);
}

void
Cadical::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto clause : clauses)
		this->addClause(clause);
}

/* Sharing */

bool
Cadical::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0);
	m_clausesToImport->addClause(clause);
	return true;
}

void
Cadical::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto cls : clauses) {
		importClause(cls);
	}
}

/* Variable Management */
unsigned int
Cadical::getVariablesCount()
{
	return solver->vars();
}

int
Cadical::getDivisionVariable()
{
	return (rand() % getVariablesCount()) + 1;
}

void
Cadical::setPhase(const unsigned int var, const bool phase)
{
	solver->phase((phase) ? var : -var);
}

void
Cadical::bumpVariableActivity(const int var, const int times)
{
	LOGERROR("Not Implemented, yet");
	exit(PERR_NOT_SUPPORTED);
}

/* Statistics And More */
#include "cadical/src/stats.hpp"
void
Cadical::printStatistics()
{
	/* TODO get with prefix instead of print */
	CaDiCaL::Stats* cstats = solver->getStatistics();

	std::cout << std::left << std::setw(15) << ("| C" + std::to_string(this->getSolverTypeId())) << std::setw(20)
			  << ("| " + std::to_string(cstats->conflicts)) << std::setw(20)
			  << ("| " + std::to_string(cstats->propagations.search)) << std::setw(17)
			  << ("| " + std::to_string(cstats->restarts)) << std::setw(20)
			  << ("| " + std::to_string(cstats->decisions)) << std::setw(20) << "|"
			  << "\n";
}

void
Cadical::printWinningLog()
{
	SolverCdclInterface::printWinningLog();
	LOGSTAT("The winner is Cadical(%d, %d), type: %s",
			this->getSolverId(),
			this->getSolverTypeId(),
			(this->getSolverTypeId() % 3 == 0)	 ? "UNSAT"
			: (this->getSolverTypeId() % 3 == 1) ? "SAT"
												 : "SWITCH");
}

std::vector<int>
Cadical::getFinalAnalysis()
{
	LOGERROR("Not Implemented, yet");
	exit(PERR_NOT_SUPPORTED);
}

std::vector<int>
Cadical::getSatAssumptions()
{
	LOGERROR("Not Implemented, yet");
	exit(PERR_NOT_SUPPORTED);
}

std::vector<int>
Cadical::getModel()
{
	std::vector<int> model;
	unsigned int maxVar = this->getVariablesCount();

	for (unsigned int i = 1; i <= maxVar; i++) {
		int tmp = solver->val(i);
		if (!tmp)
			tmp = i;
		model.push_back(tmp);
	}

	return model;
}