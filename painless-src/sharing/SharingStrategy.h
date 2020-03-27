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

/// Sharing statistics.
struct SharingStatistics
{
   /// Constructor.
   SharingStatistics()
   {
      sharedClauses   = 0;
      receivedClauses = 0;
   }

   /// Number of shared clauses that have been shared.
   unsigned long sharedClauses;

   /// Number of shared clauses produced.
   unsigned long receivedClauses;
};

/// Strategy to shared clauses.
class SharingStrategy
{
public:
   /// Destructor.
   virtual ~SharingStrategy() {};

   /// This method shared clauses from the producers to the consumers.
   virtual void doSharing(int idSharer,
                          const vector<SolverInterface *> & producers,
                          const vector<SolverInterface *> & consumers) = 0;

   /// Return the sharing statistics of this sharng strategy.
   virtual SharingStatistics getStatistics() = 0;
};
