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

#include "../solvers/SolverInterface.h"

#include <vector>

using namespace std;

static atomic<int> currentIdSolver(0);

/// Factory to create solvers.
class SolverFactory
{
public:
   /// Instantiate and return a MapleCOMSPS solver.
   static SolverInterface * createMapleCOMSPSSolver();

   static SolverInterface * createReducerSolver(SolverInterface *solver);

   /// Instantiate and return a group of MapleCOMSPS solvers.
   static void createMapleCOMSPSSolvers(int groupSize,
                                        vector<SolverInterface *> & solvers);

   /// Clone and return a new solver.
   static SolverInterface * cloneSolver(SolverInterface * other);

   /// Print stats of a group of solvers.
   static void printStats(const vector<SolverInterface *> & solvers);

   /// Apply a sparse and random diversification on solvers.
   static void sparseRandomDiversification(const
                                           vector<SolverInterface *> & solvers);

   /// Apply a native diversification on solvers.
   static void nativeDiversification(const vector<SolverInterface *> & solvers);
};
