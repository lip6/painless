#pragma once

/** @file in this file different ways to quickly configure a painless instance
 * are defined. Instead of adding solvers one by one, the Parameters offer a
 * quick way to fire some instances via cli or json file.
 */

#include <string>
#include "core/painless.hpp"

namespace PainlessConfigurator
{
    bool configurePainlessFromJson(const std::string& jsonPath,
                             PainlessImpl& painless);
    
    bool configurePainlessFromCLI(const int argc,
                            char const* const* const argv,
                            PainlessImpl& painless);
}

