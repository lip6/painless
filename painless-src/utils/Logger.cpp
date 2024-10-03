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

#include "utils/Logger.h"
#include "utils/System.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils/Parameters.h"
#include "MpiUtils.h"

#include <mutex>
#include <cmath>
#include <atomic>

static std::mutex logMutex;

std::atomic<bool> quiet = false;

static int verbosityLevelSetting = 0;

void setVerbosityLevel(int level)
{
   verbosityLevelSetting = level;
}

/**
 * Logging function
 * @param verbosityLevel to filter out some logs using program arguments
 * @param color in certain cases (debug and error) use different color for better readability
 * @param issuer the function name that logged this message (for debug and error)
 * @param fmt the message logged
 */
void log(int verbosityLevel, const char *color, const char *issuer, const char *fmt...)
{
   if (verbosityLevel <= verbosityLevelSetting)
   {
      std::lock_guard<std::mutex> lockLog(logMutex);

      if (quiet)
         return; /* placed after lock for real effect */

      va_list args;

      va_start(args, fmt);

      printf("c%s", color);

      printf("[%.2f] ", getRelativeTime());

      if (Parameters::getBoolParam("dist"))
         printf("[mpi:%d] ", mpi_rank);

      if (issuer[0] != '-') /* only for debug and error */
         printf("[%s] ", issuer);

      vprintf(fmt, args);

      va_end(args);

      printf(".%s", RESET);

      printf("\n");

      fflush(stdout);
   }
}

void logClause(int verbosityLevel, const char *color, const int *lits, unsigned size, const char *fmt...)
{
   if (verbosityLevel <= verbosityLevelSetting)
   {
      std::lock_guard<std::mutex> lockLog(logMutex);

      if (quiet)
         return; /* placed after lock for real effect */

      va_list args;

      va_start(args, fmt);

      printf("cc%s", color);

      vprintf(fmt, args);

      printf(" [%u] ", size);

      for (unsigned i = 0; i < size; i++)
      {
         printf("%d ", lits[i]);
      }

      va_end(args);

      printf("%s", RESET);

      printf("\n");

      fflush(stdout);
   }
}

void logSolution(const char *string)
{
   std::lock_guard<std::mutex> lockLog(logMutex);
   printf("s %s\n", string);
}

static unsigned intWidth(int i)
{
   if (i == 0)
      return 1;

   return (i < 0) + 1 + (unsigned)log10(fabs(i));
}

void logModel(const std::vector<int> &model)
{
   std::lock_guard<std::mutex> lockLog(logMutex);
   unsigned usedWidth = 0;

   for (unsigned i = 0; i < model.size(); i++)
   {
      if (usedWidth + 1 + intWidth(model[i]) > 80)
      {
         printf("\n");
         usedWidth = 0;
      }

      if (usedWidth == 0)
      {
         usedWidth += printf("v");
      }

      usedWidth += printf(" %d", model[i]);
   }

   printf(" 0");

   printf("\n");
}