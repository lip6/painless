// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TESTRUN(cmd, msg) int res = cmd; if (res != 0) { printf(msg,res); \
                                                         exit(res); }

/// Mutex class.
class Mutex
{
public:
   /// Constructor.
   Mutex()
   {
      TESTRUN(pthread_mutex_init(&mtx, NULL), "Mutex init failed with msg %d\n")
   }

   /// Destructor.
   virtual ~Mutex()
   {
      TESTRUN(pthread_mutex_destroy(&mtx), "Mutex destroy failed with msg %d\n")
   }

   /// Lock the mutex.
   void lock()
   {
      TESTRUN(pthread_mutex_lock(&mtx), "Mutex lock failed with msg %d\n")
   }

   /// Unlock the mutex.
   void unlock()
   {
      TESTRUN(pthread_mutex_unlock(&mtx), "Mutex unlock failed with msg %d\n")
   }

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
   Thread(void * (* main)(void *), void * arg)
   {
      pthread_create(&myTid, NULL, main, arg);
   }

   /// Join the thread.
   void join()
   {
      pthread_join(myTid, NULL);
   }

protected:
   /// The id of the pthread.
   pthread_t myTid;
};
