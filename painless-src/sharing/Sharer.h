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

#include "sharing/SharingStrategy.h"
#include "sharing/SharingEntity.hpp"
#include "utils/Threading.h"
#include "utils/Logger.h"

static void *mainThrSharing(void *arg);

/// \defgroup sharing Sharing Related Classes
/// \ingroup sharing

/// @brief A sharer is a thread responsible to share clauses between solvers.
class Sharer : public Entity
{
public:
   /// Constructors.
   Sharer(int id_, std::vector<std::shared_ptr<SharingStrategy>> &sharingStrategies);
   Sharer(int id_, std::shared_ptr<SharingStrategy> sharingStrategy);

   /// Destructor.
   virtual ~Sharer();

   /// Print sharing statistics.
   virtual void printStats();

   /// @brief To join the thread of this sharer object
   inline void join()
   {
      if(sharer == nullptr) return ;
      sharer->join();
      delete sharer;
      sharer = nullptr;
       LOGDEBUG1("Sharer %d joined", id);
   }

   inline void setThreadAffinity(int coreId)
   {
      this->sharer->setThreadAffinity(coreId);
   }

protected:
   /// Pointer to the thread in charge of sharing.
   Thread *sharer;

   /// @brief Heuristic for strategy implementation comparaison (TODO: ifndef NSTAT for such probes)
   double totalSharingTime = 0;

   /// Strategy/Strategies used to shared clauses.
   std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;

   /// @brief Working function that will call sharingStrategy doSharing()
   /// @param  sharer the sharer object
   /// @return NULL if well ended
   friend void *mainThrSharing(void *);
};
