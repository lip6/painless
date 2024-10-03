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

#include "clauses/ClauseDatabase.h"
#include "clauses/ClauseBufferAlpha.h"

#include <vector>

/// Clause database used for global sharer, cannot have an undefined maxClauseSize, by default it is set to 50.
class ClauseDatabaseLockFree : public ClauseDatabase
{
public:
	/// Constructor with default maxClauseSize = 50
	ClauseDatabaseLockFree();

	/// Constructor with custom maxClauseSize
	ClauseDatabaseLockFree(int maxClauseSize);

	/// Destructor
	~ClauseDatabaseLockFree();

	/// Add a shared clause to the database.
	bool addClause(std::shared_ptr<ClauseExchange> clause) override;

	/// Fill the given buffer with shared clauses.
	/// @return the number of selected clauses in literals.
	int giveSelection(std::vector<std::shared_ptr<ClauseExchange>> &selectedCls, unsigned totalSize) override;

	/// @brief Fill a vector with all its clauses
	/// @param v_cls the vector to fill
	void getClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls) override;

	/// @brief To select the best clause in the database
	/// @param cls a double pointer, since i seek a pointer value
	/// @return true if found at least on clause to select, otherwise false (database is empty)
	bool giveOneClause(std::shared_ptr<ClauseExchange> &cls) override;

	/// @brief Get the number of clauses present in the database per size
	/// @param nbClsPerSize a vector that will store the numbers
	void getSizes(std::vector<int> &nbClsPerSize) override;

	/// @brief get the actual size of the database
	/// @return the actual size as an unsigned
	unsigned getSize() override;

	/// @brief Deletes all the clauses that have a size equal or greater than `size`
	/// @param size The size from which clauses will be deleted, for example to delete all the clauses give size = 1 (delete from the clauses of size 1)
	void deleteClauses(int size = 1) override;

protected:
	/// Vector of buffer of shared clauses, one buffer per size till maxClauseSize.
	std::vector<ClauseBufferAlpha *> clauses;
};
