#include "Formula.hpp"
#include "utils/Logger.hpp"

bool
Formula::insert_unit(int lit)
{
	if (units.count(-lit) > 0) {
		LOG0("UNSAT: Trying to insert unit %d, while %d is also a unit", lit, -lit);
		return false;
	}
	units.insert(lit);
	return true;
}

bool
Formula::push_clause(const std::vector<int>& clause)
{
	if (clause.size() == 1) {
		int unit = *clause.begin();
		if (!this->insert_unit(unit))
			return false;
	} else {
		nonUnits.push_row(clause);
		for (int lit : clause) {
			unsigned int index = LIT_IDX(lit);
			if (index >= occurenceLists.size())
				occurenceLists.resize(index + 1);
			/* index of last pushed clause is size - 1 */
			occurenceLists.at(index).push_back(nonUnits.getRowsCount() - 1);
		}
	}
	return true;
}

bool
Formula::emplace_clause(std::initializer_list<int> clause)
{
	if (clause.size() == 1) {
		int unit = *clause.begin();
		if (!this->insert_unit(unit))
			return false;
	} else {
		nonUnits.emplace_row(clause);
		nonUnits.push_row(clause);
		for (int lit : clause) {
			unsigned int index = LIT_IDX(lit);
			if (index > occurenceLists.size())
				occurenceLists.resize(index);
			occurenceLists.at(index).push_back(nonUnits.getRowsCount() - 1);
		}
	}
	return true;
}

bool
Formula::delete_lit_nonUnit(unsigned int index, int dlit)
{
	// Remove Lit from clause
	auto clause = nonUnits[index];
	auto lit_it = std::find(clause.begin(), clause.end(), dlit);

	assert(lit_it != clause.end());

	size_t lit_index = lit_it - clause.begin();

	LOGCLAUSE1(clause.data(), clause.size(), "Literal %d is in index %u in clause %u", dlit, lit_index, index);
	if (!nonUnits.delete_element(index, lit_index)) {
		LOGDEBUG1("The formula is UNSAT at unit propagation, clause %u is empty", index);
		return false;
	}

	// Check if clause becomes unit
	if (this->getNonUnitEfficientSize(index) == 1) {
		int newunit = 0;

		for (int clit : clause) {
			newunit = clit;
			break;
		}
		LOGDEBUG1("New unit clause to be added: %d", newunit);
		if (!this->insert_unit(newunit))
			return false;
	}

	// Remove clause from occurences of lit
	return delete_nonUnit_occurence(dlit, index);
}

bool
Formula::delete_nonUnit_occurence(int dlit, unsigned int index)
{
	// Remove clause from lit occurence lit by making it zero
	auto& tempRef = this->occurenceLists.at(LIT_IDX(dlit));
	std::find(tempRef.begin(), tempRef.end(), index).operator*() =
		0; /* need to find a better contiguous data structure for quick search*/

	/* Detect pure literals if occurenceLists is empty */
	if (std::all_of(tempRef.begin(), tempRef.end(), [](unsigned int index) { return index == 0; })) {
		LOGDEBUG1("Detected Pure Literal: %d", -dlit);
		/* Check for non occuring variables (both lits are pure) */
	}

	LOGCLAUSE1((int*)tempRef.data(), tempRef.size(), "Removed %d for literal %d: ", index, dlit);

	// if new real size == 0 => -lit is a pure literal, need to iterate to check if all zeros !! Add a deleted count in
	// back or front (abstract to ignore it needed) or a separate sizes as in twoDvector
	return true;
}

void
Formula::delete_nonUnit(unsigned int index)
{
	auto clauseToDelete = nonUnits[index];
	for (auto lit : clauseToDelete) {
		delete_nonUnit_occurence(lit, index);
	}
	nonUnits.delete_row(index);
	deletedClausesCount++;
}

void
Formula::shrink_structures()
{
	// Capture initial statistics
	unsigned int initialClausesCount = nonUnits.getRowsCount();
	unsigned int initialNonUnitsSize = nonUnits.end(nonUnits.getRowsCount() - 1) - nonUnits.begin(0);
	unsigned int initialOccurenceListsSize = this->occurenceLists.size();
	unsigned int initialUnitsCount = this->units.size();

	// Cleanup the non-unit clauses
	nonUnits.cleanup();

	unsigned int pureLiteralsCount = 0;
	// Remove zero elements from occurrence lists and shrink the vectors
	for (size_t i = 0; i < occurenceLists.size(); ++i) {
		auto& occurenceList = occurenceLists[i];
		// Remove all zero elements
		occurenceList.erase(std::remove(occurenceList.begin(), occurenceList.end(), 0), occurenceList.end());
		// Shrink the vector to release unused memory
		occurenceList.shrink_to_fit();

		// Detect pure literals (TODO, update units, and manage deleted vars)
		if (occurenceList.empty()) {
			LOGDEBUG1("PureLiteral: %d", IDX_LIT(i));
			++pureLiteralsCount;
		}
	}

	// Reduce the number of buckets in the hash table, freeing unused memory
	units.rehash(0);

	// Capture final statistics
	unsigned int finalClausesCount = nonUnits.getRowsCount();
	unsigned int finalNonUnitsSize = nonUnits.end(nonUnits.getRowsCount() - 1) - nonUnits.begin(0);
	unsigned int finalOccurenceListsSize = this->occurenceLists.size();
	unsigned int finalUnitsCount = this->units.size();

	assert(finalClausesCount == initialClausesCount - deletedClausesCount);

	deletedClausesCount = 0;

	LOGSTAT("Formula Shrink Results (before -> after): nonUnits: %u -> %u (clauses: %u->%u), occurenceLists: %u -> %u "
			"(pureLiterals: %u), units: %u -> %u",
			initialNonUnitsSize,
			finalNonUnitsSize,
			initialClausesCount,
			finalClausesCount,
			initialOccurenceListsSize,
			finalOccurenceListsSize,
			pureLiteralsCount,
			initialUnitsCount,
			finalUnitsCount);
}