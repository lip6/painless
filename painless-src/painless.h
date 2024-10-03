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

#include "sharing/Sharer.h"
#include "solvers/SolverCdclInterface.hpp"
#include "working/WorkingStrategy.h"

#include <atomic>
#include <vector>
#include <unistd.h>

/// Is it the end of the search
extern std::atomic<bool> globalEnding;
/// @brief  Mutex for timeout cond
extern pthread_mutex_t mutexGlobalEnd;

/// @brief Cond to wait on timeout or wakeup on globalEnding
extern pthread_cond_t condGlobalEnd;

/// Working strategy
extern WorkingStrategy *working;

/// Array of sharers
extern std::vector<Sharer *> sharers;

/// Size of the array of sharers
extern int nSharers;

/// Final result
extern SatResult finalResult;

/// Model for SAT instances
extern std::vector<int> finalModel;

/// @brief Number of sharing groups
extern int nb_groups;

/// Number of solvers for sat solving to launch
extern int cpus;

/// To check if painless is using distributed mode
extern std::atomic<bool> dist;