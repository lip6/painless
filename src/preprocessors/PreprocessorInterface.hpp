#pragma once

#include "solvers/SolverInterface.hpp"

/**
 * @defgroup preproc_solving Preprocessing Techniques
 * @ingroup solving
 * @brief Different Classes for SAT Formula preprocessing
 * @{
 */

/// Preperocessing technique statistics
struct PreprocessorStats
{
	unsigned int newFormulaSize;
	unsigned int deletedClauses;
	unsigned int shrinkedClauses;
	unsigned int addedVariables;
	unsigned int eliminatedVariables;
};

/// Preprocessing technique type
enum PreprocessorAlgorithm
{
	MIX = 0,
	BVA = 1,
	BVE = 2,
	GAUSS = 3
	/* To fill */
};

/**
 * @brief Interface for preprocessor algorithms in SAT solving.
 *
 * This abstract class defines the interface for preprocessor algorithms
 * used in SAT solving. It inherits from SolverInterface and provides
 * additional methods specific to preprocessors.
 */
class PreprocessorInterface : public SolverInterface
{
  public:
	/**
	 * @brief Constructor for PreprocessorInterface.
	 * @param algo The preprocessor algorithm type.
	 * @param id The unique identifier for the preprocessor.
	 */
	PreprocessorInterface(PreprocessorAlgorithm algo, int id)
		: SolverInterface(SolverAlgorithmType::OTHER, id)
		, preProcType(algo)
	{
	}

	/**
	 * @brief Get the simplified formula after preprocessing.
	 * @return A vector of simplified clauses.
	 */
	virtual std::vector<simpleClause> getSimplifiedFormula() = 0;

	/**
	 * @brief Restore the original model from the preprocessed one.
	 * @param model The model to be restored.
	 */
	virtual void restoreModel(std::vector<int>& model) = 0;

	/**
	 * @brief Get statistics about the preprocessing.
	 * @return A PreprocessorStats object containing statistics.
	 */
	virtual PreprocessorStats getPreprocessorStatistics() = 0;

	/**
	 * @brief Release memory used by the preprocessor.
	 */
	virtual void releaseMemory() = 0;

  private:
	PreprocessorAlgorithm preProcType; ///< The type of preprocessor algorithm used.
};

/**
 * @} // end of preproc group
 */