#include "SolverCDCLInterface.hpp"
#include <iomanip>

void
SolverInterface::printStats(
  const std::vector<std::shared_ptr<SolverInterface>>& solvers)
{
  std::ostringstream oss;

  oss << SolverCDCLInterface::Statistics::getHeader();

  for (auto s : solvers) {
    if (s->getAlgoType() == SolverInterface::Type::CDCL)
      oss << "c " << s->statisticsToString();
  }

  LOG0("\n%s", oss.str().c_str());
}

void
SolverCDCLInterface::printCDCLStats(
  const std::vector<std::shared_ptr<SolverCDCLInterface>>& solvers)
{
  std::ostringstream oss;

  oss << SolverCDCLInterface::Statistics::getHeader();

  for (auto s : solvers) {
    oss << "c " << s->statisticsToString();
  }

  LOG0("\n%s", oss.str().c_str());
}

/// Get the header row for the statistics table
std::string
SolverCDCLInterface::Statistics::getHeader()
{
  return "c Solver,"
         "Conflicts,"
         "Propagations,"
         "Restarts,"
         "Decisions,"
         "MemPeak(KB),"
         "ImportedUnits,"
         "ImportedBinaries,"
         "ImportedLarges,"
         "ExportedClauses,"
         "FilteredExportedClauses\n";
}

/// Get the footer row for the statistics table
std::string
SolverCDCLInterface::Statistics::getFooter()
{
  // No footer needed for CSV format
  return "";
}

/// Convert statistics to a formatted CSV row
std::string
SolverCDCLInterface::Statistics::toString(int solverTypeId,
                                          const std::string& solverName) const
{
  std::ostringstream oss;
  oss << solverName << " " << solverTypeId << ","
      << conflicts << ","
      << propagations << ","
      << restarts << ","
      << decisions << ","
      << std::fixed << std::setprecision(2) << memPeak << ","
      << importedUnits << ","
      << importedBinaries << ","
      << importedLarges << ","
      << exportedClauses << ","
      << filteredExportedClauses << "\n";
  return oss.str();
}