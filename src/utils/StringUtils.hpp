#pragma once

#include <string>

namespace pl::str {
// Takes the string as a copy parameter to modify it
inline std::string
toLower(std::string s)
{
  for (auto& c : s)
    c = std::tolower(static_cast<unsigned char>(c));
  return s;
}
}