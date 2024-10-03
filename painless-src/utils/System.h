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

#pragma once
#include <sys/time.h>

#define BILLION 1000000000
#define MILLION 1000000

/// @brief Singleton class for MemInfo
class MemInfo
{
protected:
    MemInfo()
    {
    }

public:
    MemInfo(MemInfo &other) = delete;

    void operator=(const MemInfo &) = delete;

    static MemInfo *getInstance();

    /// Parse the /proc/meminfo file to get some data
    void parseMemInfo();

    /// Get the current memory used of this process in Ko.
    static double getUsedMemory();

    /// @brief Get the current memory used in all the OS.
    static double getNotAvailableMemory();

    /// @brief Get the maximum memory in Ko.
    static double getTotalMemory();

    /// @brief Get the free memory (cache not counted) in Ko.
    static double getFreeMemory();

    /// @brief Get the available memory (cache included) in Ko.
    static double getAvailableMemory();

protected:
    static MemInfo *singleton_;

public:
    long MemTotal;
    long MemFree;
    long MemAvailable;
};

/// Get the relative in seconds time.
double getRelativeTime();

/// Get the relative time according to a certain start
double getRelativeTime(double start);

/// Get the absolute time in seconds since the beginning.
double getAbsoluteTime();

/// @brief Update the timespec to have the absolute time needed to wait for a certain time. It is mainly used for pthread_cond_timedwait
/// @param relativeTimeToWait the time period to wait
/// @param timeToWaitForCond a pointer to a timespec that will be given to the pthread_cond_timedwait
void getTimeToWait(timespec *relativeTimeToWait, timespec *timeToWaitForCond);

/// @brief A function to convert a long of microseconds into timespec
/// @param microSeconds the number of microseconds
/// @param result the timespec
void getTimeSpecMicro(long microSeconds, timespec *result);
