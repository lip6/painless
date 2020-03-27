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

#include "../clauses/ClauseExchange.h"
#include "../utils/Threading.h"

#include <vector>

using namespace std;

/// Clause database used for Hordesat
class ClauseDatabase
{
public:
   /// Constructor.
	ClauseDatabase();

   /// Destructor
	~ClauseDatabase();

	/// Add a shared clause to the database.
	void addClause(ClauseExchange * clause);
	 
   /// Fill the given buffer with shared clauses.
	/// @return the number of used literals.
	int giveSelection(vector<ClauseExchange *> & selectedCls, unsigned totalSize,
                     int * selectCount);

protected:
   /// Vector of vector of shared clauses, on vector per size.
	vector<vector<ClauseExchange *> > clauses;
};
