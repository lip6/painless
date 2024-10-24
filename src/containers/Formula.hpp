
#include "vector2D.hpp"
#include <unordered_set>

using namespace painless;

// Macros for literal representation conversion
#define PLIT_IDX(LIT) (LIT * 2 - 2)
#define NLIT_IDX(LIT) (-LIT * 2 - 1)
#define LIT_IDX(LIT) (LIT > 0 ? PLIT_IDX(LIT) : NLIT_IDX(LIT))
#define IDX_LIT(IDX) (((IDX)&1) ? -(((IDX) + 1) >> 1) : (((IDX) >> 1) + 1))

/**
 * @class Formula
 * @brief Represents a SAT formula with unit and non-unit clauses.
 * @ingroup pl_containers
 * @warning still in development
 */
class Formula
{
  public:
	 /**
     * @brief Adds a new clause to the formula.
     * @param clause The clause to be added.
     * @return True if the clause was successfully added, false otherwise.
     */
    bool push_clause(const std::vector<int>& clause);

    /**
     * @brief Adds a new clause to the formula using an initializer list.
     * @param clause The clause to be added as an initializer list.
     * @return True if the clause was successfully added, false otherwise.
     */
    bool emplace_clause(std::initializer_list<int> clause);

    /**
     * @brief Inserts a unit clause into the formula.
     * @param lit The literal to be inserted as a unit clause.
     * @return True if the unit clause was successfully inserted, false otherwise.
     */
    bool insert_unit(int lit);

    /**
     * @brief Deletes a non-unit clause from the formula.
     * @param index The index of the clause to be deleted.
     */
    void delete_nonUnit(unsigned int index);

    /**
     * @brief Deletes a literal from a non-unit clause.
     * @param index The index of the clause.
     * @param lit The literal to be deleted.
     * @return False if the clause becomes empty after deletion, true otherwise.
     */
    bool delete_lit_nonUnit(unsigned int index, int lit);

    /**
     * @brief Deletes a clause occurrence from a literal's occurrence list.
     * @param lit The literal.
     * @param index The index of the clause to be removed from the occurrence list.
     * @return True if the operation was successful.
     */
    bool delete_nonUnit_occurence(int lit, unsigned int index);

	/**
     * @brief Deletes a unit clause from the formula.
     * @param lit The literal representing the unit clause to be deleted.
     */
	void delete_unit(int lit) { units.erase(lit); }

	/**
     * @brief Sets the number of variables in the formula.
     * @param varCount The number of variables.
     */
	void setVarCount(unsigned int varCount) { this->varCount = varCount; }

	/**
     * @brief Gets the number of variables in the formula.
     * @return The number of variables.
     */
	unsigned int getVarCount() { return this->varCount; }

	/**
     * @brief Gets a non-unit clause from the formula.
     * @param i The index of the non-unit clause.
     * @return A span of the clause literals, skipping zeros.
     */
	skipzero_span<const int> getNonUnit(unsigned int i) const { return nonUnits[i]; }

	/**
     * @brief Gets the number of non-zero literals in a non-unit clause.
     * @param i The index of the non-unit clause.
     * @return The number of non-zero literals in the clause.
     */
	unsigned int getNonUnitEfficientSize(unsigned int i) const { return nonUnits.getRowSize(i); }

	/**
     * @brief Gets the set of unit clauses.
     * @return A constant reference to the set of unit clauses.
     */
	const std::unordered_set<int>& getUnits() const { return this->units; }

	 /**
     * @brief Gets a modifiable reference to the set of unit clauses.
     * @return A reference to the set of unit clauses.
     */
	std::unordered_set<int>& getUnits() { return this->units; }

	/**
     * @brief Gets the occurrence list of a literal.
     * @param lit The literal.
     * @return A span of clause indices where the literal occurs.
     */
	skipzero_span<unsigned int> getOccurenceList(int lit)
	{
		return skipzero_span<unsigned int>(this->occurenceLists.at(LIT_IDX(lit)));
	}

	/**
     * @brief Gets the number of unit clauses in the formula.
     * @return The number of unit clauses.
     */
	unsigned int getUnitCount() { return this->units.size(); }

	 /**
     * @brief Gets the number of non-unit clauses in the formula.
     * @return The number of non-unit clauses.
     */
	unsigned int getNonUnitsCount() { return this->nonUnits.getRowsCount() - this->deletedClausesCount; }

	 /**
     * @brief Gets the total number of clauses (unit and non-unit) in the formula.
     * @return The total number of clauses.
     */
	unsigned int getAllClauseCount() { return this->getUnitCount() + this->getNonUnitsCount(); }

	/**
     * @brief Shrinks the internal data structures to save memory.
     */
	void shrink_structures();

  private:
	std::unordered_set<int> units;  ///< Set of unit clauses.
    vector2D<int> nonUnits;  ///< 2D vector storing non-unit clauses.
    std::vector<std::vector<unsigned int>> occurenceLists;  ///< Occurrence lists for each literal.
    unsigned int varCount = 0;  ///< Number of variables in the formula.
    unsigned int deletedClausesCount = 0;  ///< Count of deleted clauses for efficient non-unit clause counting
};
