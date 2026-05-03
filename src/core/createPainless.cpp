#include "config/PainlessConfigurator.hpp"

Painless*
create_empty_painless()
{
  PainlessImpl* painless = new PainlessImpl();

  return painless;
}

Painless*
create_painless(const char* json, int isQuiet)
{
  // Logger should be quiet by default
  if (isQuiet)
    Logger::getInstance().setIsQuiet();
  PainlessImpl* painless = new PainlessImpl();
  // By default, it is quiet
  PainlessConfigurator::configurePainlessFromJson(json, *painless);

  return painless;
}

void
destroy_painless(Painless* solver)
{
  delete solver;
}
