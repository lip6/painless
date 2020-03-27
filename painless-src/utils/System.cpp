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
#include <sys/time.h>

#include "../utils/System.h"

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

double getMemoryUsed()
{
   struct rusage r_usage;
   getrusage(RUSAGE_SELF,&r_usage);
   return r_usage.ru_maxrss;
}
