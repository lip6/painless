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

#include "clauses/ClauseExchange.h"
#include "utils/Threading.h"

#include <vector>
#include <memory>
#include <sstream>


/// Clause database interface for all sharing strategies (local & global)
class ClauseDatabase
{
public:
	/// Default Constructor.
	ClauseDatabase();

	/// @brief Constructor setting the maxClauseSize
	/// @param maxClauseSize the parameter defining the maximum clause size accepted in this ClauseDatabase.
	ClauseDatabase(int maxClauseSize);

	/// Destructor
	virtual ~ClauseDatabase();

	/// Add a shared clause to the database.
	virtual bool addClause(std::shared_ptr<ClauseExchange> clause) = 0;

	/// Fill the given buffer with shared clauses.
	/// @param totalSize Represents the total size in literals
	/// @return the number of selected clauses in literal count.
	virtual int giveSelection(std::vector<std::shared_ptr<ClauseExchange>> &selectedCls, unsigned totalSize) = 0;

	/// @brief Fill a vector with all its clauses
	/// @param v_cls the vector to fill
	virtual void getClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls) = 0;

	/// @brief To select the best clause in the database
	/// @param cls a double pointer, since i seek a pointer value
	/// @return true if found at least on clause to select, otherwise false (database is empty)
	virtual bool giveOneClause(std::shared_ptr<ClauseExchange> &cls) = 0;

	/// @brief Get the number of clauses present in the database per size
	/// @param nbClsPerSize a vector that will store the numbers
	virtual void getSizes(std::vector<int> &nbClsPerSize) = 0;

	/// @brief get the actual size of the database
	/// @return the actual size as an unsigned
	virtual unsigned getSize() = 0;

	/// @brief Prints the number of clauses seen per clause size.
    void getTotalSizes(std::stringstream &strStream);

	/// @brief Deletes all the clauses that have a size equal or greater than `size`
	/// @param size The size from which clauses will be deleted, for example to delete all the clauses give size = 1 (delete from the clauses of size 1)
	virtual void deleteClauses(int size = 1) = 0;

protected:
	/// @brief The maximum clause size accepted in this clauseDatabase (if <= 0: no limit).
	int maxClauseSize;

	/// @brief Vector containing the cumulative size of the database for statistics purpose.
    std::vector<int> totalSizes; // empty at start
};
