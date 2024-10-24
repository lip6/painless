#pragma once

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TESTRUN(cmd, msg)                                                                                              \
	int res = cmd;                                                                                                     \
	if (res != 0) {                                                                                                    \
		printf(msg, res);                                                                                              \
		exit(res);                                                                                                     \
	}

/// Mutex class.
class Mutex
{
  public:
	/// Constructor.
	Mutex() { TESTRUN(pthread_mutex_init(&mtx, NULL), "Mutex init failed with msg %d") }

	/// Destructor.
	virtual ~Mutex() { TESTRUN(pthread_mutex_destroy(&mtx), "Mutex destroy failed with msg %d") }

	/// Lock the mutex.
	void lock() { TESTRUN(pthread_mutex_lock(&mtx), "Mutex lock failed with msg %d") }

	/// Unlock the mutex.
	void unlock() { TESTRUN(pthread_mutex_unlock(&mtx), "Mutex unlock failed with msg %d") }

	/// Try to lock the mutex, return true if succes else false.
	bool tryLock()
	{
		// return true if lock acquired
		return pthread_mutex_trylock(&mtx) == 0;
	}

  protected:
	/// A pthread mutex.
	pthread_mutex_t mtx;
};

/// Thread class
class Thread
{
  public:
	/// Constructor.
	Thread(void* (*main)(void*), void* arg) { pthread_create(&myTid, NULL, main, arg); }

	/// Join the thread.
	void join() { pthread_join(myTid, NULL); }

	void setThreadAffinity(int coreId)
	{
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(coreId, &cpuset);

		pthread_setaffinity_np(this->myTid, sizeof(cpu_set_t), &cpuset);
	}

  protected:
	/// The id of the pthread.
	pthread_t myTid;
};
