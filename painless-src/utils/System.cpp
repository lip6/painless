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

#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include "utils/System.h"
#include "Logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <sys/resource.h>

// Memory
// ======

void MemInfo::parseMemInfo()
{
    std::ifstream meminfo_file("/proc/meminfo");
    if (!meminfo_file.is_open())
    {
        this->MemTotal = std::numeric_limits<long>::max();
        this->MemFree = std::numeric_limits<long>::max();
        this->MemAvailable = std::numeric_limits<long>::max();
        return;
    }

    std::string line;
    while (std::getline(meminfo_file, line))
    {
        std::istringstream iss(line);
        std::string key;
        long value;

        if (iss >> key >> value)
        {
            if (key == "MemTotal:")
            {
                this->MemTotal = value;
                LOGDEBUG1("Parsed MemTotal: %ld", this->MemTotal);
            }
            else if (key == "MemFree:")
            {
                this->MemFree = value;
                LOGDEBUG1("Parsed MemFree: %ld", this->MemFree);
            }
            else if (key == "MemAvailable:")
            {
                this->MemAvailable = value;
                LOGDEBUG1("Parsed MemAvailable: %ld", this->MemAvailable);
            }
        }
    }

    meminfo_file.close();
}

MemInfo* MemInfo::singleton_ = nullptr;

MemInfo *MemInfo::getInstance()
{
    if(singleton_==nullptr){
        singleton_ = new MemInfo();
    }
    return singleton_;
}

double MemInfo::getUsedMemory()
{
	struct rusage r_usage;
	getrusage(RUSAGE_SELF, &r_usage);
	return r_usage.ru_maxrss;
}

double MemInfo::getNotAvailableMemory()
{
	MemInfo *meminfo = MemInfo::getInstance();
	meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
	return meminfo->MemTotal - meminfo->MemAvailable;
}

double MemInfo::getTotalMemory()
{
	MemInfo *meminfo = MemInfo::getInstance();
	return meminfo->MemTotal;
}

double MemInfo::getFreeMemory()
{
	MemInfo *meminfo = MemInfo::getInstance();
	meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
	return meminfo->MemFree;
}

double MemInfo::getAvailableMemory()
{
	MemInfo *meminfo = MemInfo::getInstance();
	meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
	return meminfo->MemAvailable;
}

// double getMemoryMax()
// {
// 	return sysconf(_SC_PHYS_PAGES) * (sysconf(_SC_PAGE_SIZE) / 1024);
// }

// double getMemoryFree()
// {
// 	return sysconf(_SC_AVPHYS_PAGES) * (sysconf(_SC_PAGE_SIZE) / 1024);
// }

// Time
// ====

static double timeStart = getAbsoluteTime();

double getAbsoluteTime()
{
	timeval time;

	gettimeofday(&time, NULL);

	return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

double getRelativeTime()
{
	return getAbsoluteTime() - timeStart;
}

double getRelativeTime(double start)
{
	return getAbsoluteTime() - start;
}

void getTimeToWait(timespec *relativeTimeToWait, timespec *timeToWaitForCond)
{
	timeval now;
	gettimeofday(&now, NULL);
	timeToWaitForCond->tv_nsec = now.tv_usec * 1000 + relativeTimeToWait->tv_nsec;
	timeToWaitForCond->tv_sec = now.tv_sec + relativeTimeToWait->tv_sec;
	if (timeToWaitForCond->tv_nsec > BILLION)
	{
		timeToWaitForCond->tv_nsec = timeToWaitForCond->tv_nsec - BILLION;
		timeToWaitForCond->tv_sec++;
	}
	LOGDEBUG2("Now : %ld,%ld, relativeTimeTowait: %ld,%ld, result: %ld,%ld", now.tv_sec, now.tv_usec * 1000, relativeTimeToWait->tv_sec, relativeTimeToWait->tv_nsec, timeToWaitForCond->tv_sec, timeToWaitForCond->tv_nsec);
}

void getTimeSpecMicro(long microSeconds, timespec *result)
{
	result->tv_sec = microSeconds / MILLION;
	microSeconds = microSeconds - result->tv_sec * MILLION;
	result->tv_nsec = microSeconds * 1000;
}