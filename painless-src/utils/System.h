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


#ifndef UTILS_SYSTEM_H
#define UTILS_SYSTEM_H

/// Get the relative in seconds time.
double getRelativeTime();

/// Get the absolute time in seconds since the beginning.
double getAbsoluteTime();

/// Get the current memory used in Ko.
double getMemoryUsed();

#endif // UTILS_SYSTEM_H
