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

#include "../solvers/SolverInterface.hpp"

#include <vector>

extern std::atomic<bool> globalEnding;

extern SatResult finalResult;

extern std::vector<int> finalModel;

class WorkingStrategy
{
public:
   WorkingStrategy()
   {
      parent = NULL;
   }

   virtual void solve(const std::vector<int> &cube) = 0;

   virtual void join(WorkingStrategy *winner, SatResult res,
                     const std::vector<int> &model) = 0;

   virtual void setInterrupt() = 0;

   virtual void unsetInterrupt() = 0;

   virtual void waitInterrupt() = 0;

   virtual void addSlave(WorkingStrategy *slave)
   {
      slaves.push_back(slave);
      slave->parent = this;
   }
   
   virtual ~WorkingStrategy() {}

protected:
   WorkingStrategy *parent;

   std::vector<WorkingStrategy *> slaves;
};
