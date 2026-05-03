#include "config/WorkingStrategyRegistry.hpp"

#include "utils/ErrorCodes.hpp"
// #include "working/DivideAndConquer.hpp"
#include "working/PortfolioSimple.hpp"
#include "utils/StringUtils.hpp"

namespace WorkingStrategyRegistry {

std::shared_ptr<WorkingStrategy>
create(const std::string& name,
       PainlessImpl& painless,
       std::vector<std::shared_ptr<SolverInterface>>& solvers)
{
  std::string key = pl::str::toLower(name);

  if (key == "portfoliosimple")
    return std::make_shared<PortfolioSimple>(painless, solvers);
  if (key == "divideandconquer")
    PABORT(PERR_NOT_SUPPORTED,"Divide and conquer is removed temporarly");
    //return std::make_shared<DivideAndConquer>(painless, solvers);

  PABORT(PERR_NOT_SUPPORTED, "Unknown working strategy: %s", name.c_str());
  return nullptr; // unreachable
}

} // namespace WorkingStrategyRegistry
