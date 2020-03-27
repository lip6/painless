// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
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

#include "painless.h"

#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/SatUtils.h"

#include "solvers/SolverFactory.h"
#include "solvers/Reducer.h"

#include "clauses/ClauseManager.h"

#include "sharing/StrengtheningSharing.h"
#include "sharing/SimpleSharing.h"
#include "sharing/HordeSatSharing.h"
#include "sharing/Sharer.h"

#include "working/SequentialWorker.h"
#include "working/Portfolio.h"

#include <unistd.h>


using namespace std;


// -------------------------------------------
// Declaration of global variables
// -------------------------------------------
atomic<bool> globalEnding(false);

Sharer ** sharers = NULL;

int nSharers = 0;

WorkingStrategy * working = NULL;

SatResult finalResult = UNKNOWN;

vector<int> finalModel;

vector<SolverInterface *> solvers;

void redefine_sig_handler(int num, void (*handler)(int))
{
   struct sigaction signal;
   signal.sa_handler = handler;
   signal.sa_flags = 0;
   sigemptyset(&signal.sa_mask);
   sigaction(num, &signal, NULL);
}

// -------------------------------------------
// Main of the framework
// -------------------------------------------
int main(int argc, char ** argv)
{
   Parameters::init(argc, argv);

   if (Parameters::getFilename() == NULL ||
       Parameters::getBoolParam("h"))
   {
      cout << "USAGE: " << argv[0] << " [options] input.cnf" << endl;
      cout << "Options:" << endl;
      cout << "\t-c=<INT>\t\t number of cpus, default is 24" << endl;
      cout << "\t-max-memory=<INT>\t memory limit in GB, default is 51" << \
	      endl;
      cout << "\t-t=<INT>\t\t timeout in seconds, default is no limit" << endl;
      cout << "\t-lbd-limit=<INT>\t LBD limit of exported clauses, default is" \
	      " 2" << endl;
      cout << "\t-shr-sleep=<INT>\t time in useconds a sharer sleep each " \
         "round, default is 500000 (0.5s)" << endl;
      cout << "\t-shr-lit=<INT>\t\t number of literals shared per round, " \
         "default is 1500" << endl;
      cout << "\t-v=<INT>\t\t verbosity level, default is 0" << endl;
      return 0;
   }

   Parameters::printParams();

   int cpus = Parameters::getIntParam("c", 23);
   setVerbosityLevel(Parameters::getIntParam("v", 0));

   // Create and init solvers
   vector<SolverInterface *> solvers_LRB_recto;
   vector<SolverInterface *> solvers_LRB_verso;
   vector<SolverInterface *> solvers_VSIDS_recto;
   vector<SolverInterface *> solvers_VSIDS_verso;

   if (Parameters::getBoolParam("strength") && cpus >= 2) {
      SolverFactory::createMapleCOMSPSSolvers(cpus - 1, solvers);
      solvers.push_back(SolverFactory::createReducerSolver(SolverFactory::createMapleCOMSPSSolver()));
   } else {
      SolverFactory::createMapleCOMSPSSolvers(cpus, solvers);
   }

   int nSolvers = solvers.size();

   cout << "c " << nSolvers << " solvers are used, with IDs in [|0, "
        << nSolvers - 1 << "|]." << endl;
   cout << "c solvers with odd (even) IDs used LRB (VSIDS)." << endl;
   cout << "c solver " << ID_XOR
        << " uses Gaussian Elimination (GE) at preprocessing" << endl;

   SolverFactory::nativeDiversification(solvers);

   for (int id = 0; id < nSolvers; id++) {
      if (id % 2 == 0) {
         if (id % 4 == 0) {
            solvers_LRB_verso.push_back(solvers[id]);
         } else {
            solvers_LRB_recto.push_back(solvers[id]);
         }
      } else {
         if ((id - 1) % 4 == 0) {
            solvers_VSIDS_verso.push_back(solvers[id]);
         } else {
            solvers_VSIDS_recto.push_back(solvers[id]);
         }
      }
   }

   SolverFactory::sparseRandomDiversification(solvers_LRB_recto);
   SolverFactory::sparseRandomDiversification(solvers_LRB_verso);
   SolverFactory::sparseRandomDiversification(solvers_VSIDS_recto);
   SolverFactory::sparseRandomDiversification(solvers_VSIDS_verso);


   vector<SolverInterface *> from;
   switch(Parameters::getIntParam("shr-strat", 0)) {
      // Init Sharing
      case 1 :
         nSharers   = 1;
         sharers    = new Sharer*[nSharers];
         sharers[0] = new Sharer(0, new SimpleSharing(), solvers, solvers);
         break;
      case 2 :
         nSharers = cpus;
         sharers  = new Sharer*[nSharers];

         for (size_t i = 0; i < nSharers; i++) {
            from.clear();
            from.push_back(solvers[i]);
            sharers[i] = new Sharer(i, new HordeSatSharing(), from,
                                    solvers);
         }
         break;
   }

   // Init working
   working = new Portfolio();
   for (size_t i = 0; i < nSolvers; i++) {
      working->addSlave(new SequentialWorker(solvers[i]));
   }


   // Init the management of clauses
   ClauseManager::initClauseManager();


   // Launch working
   vector<int> cube;
   working->solve(cube);


   // Wait until end or timeout
   int timeout = Parameters::getIntParam("t", -1);

   while(globalEnding == false) {
      sleep(1);

      if (timeout > 0 && getRelativeTime() >= timeout) {
         globalEnding = true;
         working->setInterrupt();
      }
   }


  // Delete sharers
  for (int id = 0; id < nSharers; id++) {
     sharers[id]->printStats();
     delete sharers[id];
  }
  delete sharers;

  // Print solver stats
  SolverFactory::printStats(solvers);


  // Delete working strategy
  delete working;


  // Delete shared clauses
  ClauseManager::joinClauseManager();


   // Print the result and the model if SAT
   cout << "c Resolution time: " << getRelativeTime() << "s" << endl;

   if (finalResult == SAT) {
      cout << "s SATISFIABLE" << endl;

      if (Parameters::getBoolParam("no-model") == false) {
         printModel(finalModel);
      }
   } else if (finalResult == UNSAT) {
      cout << "s UNSATISFIABLE" << endl;
   } else {
      cout << "s UNKNOWN" << endl;
   }

   return finalResult;
}
