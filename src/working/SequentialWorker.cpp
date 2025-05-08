#include "working/SequentialWorker.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"

#include <unistd.h>

;

// Main executed by worker threads
void*
mainWorker(void* arg)
{
	SequentialWorker* sq = (SequentialWorker*)arg;

	SatResult res = SatResult::UNKNOWN;

	std::vector<int> model;

	while (globalEnding == false && sq->force == false) {
		pthread_mutex_lock(&sq->mutexStart);

		while (sq->waitJob == true) {
			pthread_cond_wait(&sq->mutexCondStart, &sq->mutexStart);
		}

		pthread_mutex_unlock(&sq->mutexStart);

		LOGDEBUG1("Sequential Worker for solver of %s %d before solve",
				  typeid(*(sq->solver)).name(),
				  sq->solver->getSolverId());

		sq->waitInterruptLock.lock();

		res = sq->solver->solve(sq->actualCube); // why pass by param what the solver already has ?

		sq->waitInterruptLock.unlock();

		LOGDEBUG3("Sequential Worker for solver of %s %d after solve",
				  typeid(*(sq->solver)).name(),
				  sq->solver->getSolverId());

		if (res == SatResult::SAT) {
			model = sq->solver->getModel();
		}

		sq->join(NULL,
				 res,
				 model); // assign true to force ! No need for sq->force==false in while loop (see original painless)

		model.clear();

		sq->waitJob = true;
	}

	return NULL;
}

// Constructor
SequentialWorker::SequentialWorker(std::shared_ptr<SolverInterface> solver_)
{
	solver = solver_;
	force = false;
	waitJob = true;

	pthread_mutex_init(&mutexStart, NULL);
	pthread_cond_init(&mutexCondStart, NULL);

	worker = new Thread(mainWorker, this);
}

// Destructor
SequentialWorker::~SequentialWorker()
{
	if (!force)
		setSolverInterrupt();

	worker->join();
	delete worker;

	pthread_mutex_destroy(&mutexStart);
	pthread_cond_destroy(&mutexCondStart);
}

void
SequentialWorker::solve(const std::vector<int>& cube)
{
	actualCube = cube;

	unsetSolverInterrupt();

	waitJob = false;

	pthread_mutex_lock(&mutexStart);
	pthread_cond_signal(&mutexCondStart);
	pthread_mutex_unlock(&mutexStart);
}

void
SequentialWorker::join(WorkingStrategy* winner, SatResult res, const std::vector<int>& model)
{
	force = true;
	LOGDEBUG1("SequentialWorker %p of solver %s is joining with res = %d.", this, typeid(*solver).name(), res);

	if (globalEnding)
		return;

	if (parent == NULL) {
		worker->join();

		globalEnding = true;
		finalResult = res;

		if (res == SatResult::SAT) {
			finalModel = model;
		}
		mutexGlobalEnd.lock();
		condGlobalEnd.notify_all();
		mutexGlobalEnd.unlock();
	} else {
		LOGDEBUG1("SequentialWorker %p calls its parent", this);
		parent->join(this, res, model);
	}
}

void
SequentialWorker::waitInterrupt()
{
	waitInterruptLock.lock();
	waitInterruptLock.unlock();
}

void
SequentialWorker::setSolverInterrupt()
{
	force = true;
	solver->setSolverInterrupt();
}

void
SequentialWorker::unsetSolverInterrupt()
{
	force = false;
	solver->unsetSolverInterrupt();
}