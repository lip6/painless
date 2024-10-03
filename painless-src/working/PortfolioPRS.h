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

#include "utils/Parameters.h"
#include "working/WorkingStrategy.h"

#include "preprocessors/PRS-Preprocessors/preprocess.hpp"

#include "sharing/Sharer.h"
#include "sharing/GlobalDatabase.h"
#include "sharing/LocalStrategies/LocalSharingStrategy.h"
#include "sharing/GlobalStrategies/GlobalSharingStrategy.h"
#include "preprocessors/StructuredBva.hpp"

#include <mutex>
#include <condition_variable>

class PortfolioPRS : public WorkingStrategy
{
public:
   PortfolioPRS();

   ~PortfolioPRS();

   void solve(const std::vector<int> &cube);

   void join(WorkingStrategy *strat, SatResult res,
             const std::vector<int> &model);

   void restoreModelDist();

   void setInterrupt();

   void unsetInterrupt();

   void waitInterrupt();

protected:
   std::atomic<bool> strategyEnding;

   // Workers
   //--------
   preprocess prs{-1};
   StructuredBVA sbva{-1};
   std::mutex sbvaLock;
   std::condition_variable sbvaSignal;

   // Sharing
   //--------

   /* Use weak_ptr instead of shared_ptr ? */
   std::vector<std::shared_ptr<LocalSharingStrategy>> localStrategies;
   std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;
   std::shared_ptr<GlobalDatabase> globalDatabase;
   std::vector<std::unique_ptr<Sharer>> sharers;
};
