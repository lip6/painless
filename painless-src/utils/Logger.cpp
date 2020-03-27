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

#include "../utils/Logger.h"
#include "../utils/System.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

static int verbosityLevelSetting = 0;

void setVerbosityLevel(int level)
{
   verbosityLevelSetting = level;
}

void log(int verbosityLevel, const char * fmt ...)
{
   if (verbosityLevel <= verbosityLevelSetting) {
      va_list args;

      va_start(args, fmt);

      printf("c [%.2f] ", getRelativeTime());

      vprintf(fmt, args);

      va_end(args);

      fflush(stdout);
   }
}
