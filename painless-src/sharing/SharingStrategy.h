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

#include "sharing/SharingStatistics.h"
#include "sharing/SharingEntityVisitor.h"

// To include the real types
#include "../solvers/SolverCdclInterface.hpp"
#include "sharing/GlobalDatabase.h"
#include "sharing/SharingEntity.hpp"

#include <vector>

/**
 * \defgroup sharing_strat Sharing Strategies
 * \ingroup sharing_strat
 * \ingroup sharing
 * @brief The interface for all the different sharing strategies local or global.
 *
 */
class SharingStrategy
{
public:
   /// Constructors
   SharingStrategy() {}

   /// Destructor.
   virtual ~SharingStrategy() {}

   /// This method shared clauses from the producers to the consumers.
   /// @return true if sharer should end , false otherwise
   virtual bool doSharing() = 0;

   /// @brief  It permits the strategy to choose the sleeping time of the sharer
   /// @return number of micro seconds to sleep
   virtual ulong getSleepingTime();

   /// @brief Function called to print the stats of the strategy
   virtual void printStats() = 0;

   /// @brief To count the number of literals present in a vector of clauses
   /// @param clauses the vector of clauses
   /// @return the number of literals in the vector clauses
   static int getLiteralsCount(std::vector<std::shared_ptr<ClauseExchange>> clauses);
};
