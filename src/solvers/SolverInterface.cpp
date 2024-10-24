#include "SolverInterface.hpp"

//------------------------------------------------------------------------------
// Public Member Functions
//------------------------------------------------------------------------------

void
SolverInterface::printWinningLog()
{
	int algo = static_cast<int>(this->m_algoType);
	LOGSTAT("The winner is of type: %s", (algo) ? (algo) ? "LOCAL_SEARCH" : "PREPROCESSING" : "CDCL");
}

void
SolverInterface::printStatistics()
{
	LOGWARN("printStatistics is not implemented");
}

void
SolverInterface::printParameters()
{
	LOGWARN("printParameters is not implemented");
}

//------------------------------------------------------------------------------
// Constructor & Destructor
//------------------------------------------------------------------------------

SolverInterface::SolverInterface(SolverAlgorithmType algoType, int solverId)
	: m_algoType(algoType)
	, m_initialized(false)
	, m_solverId(solverId)
{
}

SolverInterface::~SolverInterface()
{
	auto it = s_instanceCounts.find(std::type_index(typeid(*this)));
	if (it != s_instanceCounts.end())
		it->second--;
}