#include "config/Parameters.hpp"
#include "utils/ErrorCodes.hpp"
#include <algorithm>
#include <boost/json/src.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <type_traits>

// Forward declaration of the primary template
template<typename T>
T
getValue(const std::string& value);

// Specialization for int
template<>
int
getValue<int>(const std::string& value)
{
  return std::stoi(value);
}

// Specialization for unsigned
template<>
unsigned
getValue<unsigned>(const std::string& value)
{
  return std::stoul(value);
}

// Specialization for float
template<>
float
getValue<float>(const std::string& value)
{
  return std::stof(value);
}

// Specialization for bool
template<>
bool
getValue<bool>(const std::string& value)
{
  std::string cleanValue;
  for (auto c : value) {
    if (std::isalnum(c))
      cleanValue.push_back(std::tolower(c));
  }

  return (cleanValue == "true" || cleanValue == "1");
}

// Specialization for std::string
template<>
std::string
getValue<std::string>(const std::string& value)
{
  return value;
}

// TODO: Compare the ifs and the map find (readability, code size)
void
Parameters::initFromCLI(const int argc, char const* const* const argv)
{
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg[0] != '-' && this->filename.empty()) {
      this->filename = arg;
      continue;
    }
    // accept both -param and --param
    size_t name_pos = arg.find_first_not_of('-');
    size_t eq_pos = arg.find('=');
    // If no = char found, it is a boolean flag
    std::string key = (eq_pos == std::string::npos)
                        ? arg.substr(name_pos)
                        : arg.substr(name_pos, eq_pos - name_pos);
    std::string value =
      (eq_pos == std::string::npos) ? "true" : arg.substr(eq_pos + 1);

#define PARAM(name, type, parsed_name, default_value, description)             \
  if (key == parsed_name) {                                                    \
    try {                                                                      \
      this->name = getValue<detail::storage_type_t<type>>(value);              \
    } catch (const std::exception& e) {                                        \
      LOGERROR("Error parsing parameter '%s': %s", parsed_name, e.what());     \
      exit(PERR_ARGS);                                                   \
    }                                                                          \
    continue;                                                                  \
  }
#define CATEGORY(name)
#define SUBCATEGORY(name)
#define ENDCATEGORY(name)
#define ENDSUBCATEGORY(name)
    PARAMETERS
    PABORT(PERR_ARGS, "Unknown Option: %s", key.c_str());
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
#undef ENDCATEGORY
#undef ENDSUBCATEGORY
  }
  Logger::getInstance().setVerbosityLevel(this->verbosity);
  if(this->color == "always") Logger::getInstance().forceColors(true);

  if (!this->details.empty()) {
    if (this->help) {
      LOGWARN("Both -help and -details are specified. Showing detailed help.");
    }
    std::cout << Parameters::helpDetailsToString(this->details);
    exit(0);
  }

  if (this->help) {
    std::cout << Parameters::helpToString();
    exit(0);
  }

  if (!this->enableDistributed && !this->filename.empty() &&
      !std::filesystem::exists(this->filename)) {
    LOGERROR("Error: File '%s' not found", this->filename.c_str());
    exit(PERR_ARGS);
  }

  if (this->filename.empty()) {
    LOGERROR("Error: no input file found");
    // printHelp();
    exit(PERR_ARGS);
  }

  if (!this->cpus) {
    this->cpus = std::thread::hardware_concurrency();
  }
}

void
Parameters::initFromJSON(const std::string& path)
{
  /* Read full file into a string */
  std::ifstream sjsonFile(path);
  PABORTIF(!sjsonFile.is_open(),
           PERR_ARGS,
           "Couldn't open file %s",
           path.c_str());

  std::string line;
  std::string wholefile;
  while (std::getline(sjsonFile, line)) {
    wholefile.append(line);
  }

  sjsonFile.close();

  LOGD2("Read from file: %s", wholefile.c_str());

  try {
    boost::json::value jsonValue = boost::json::parse(wholefile);
    LOGD2("JsonValue is %s an object", ((jsonValue.is_object()) ? "" : "not"));
    boost::json::object jsonObject = jsonValue.as_object();
    LOGD2("Parsed Json: ");
    for (boost::json::key_value_pair jsonAtt : jsonObject) {
      LOGD2("%s : %s",
            jsonAtt.key().data(),
            boost::json::serialize(jsonAtt.value()).data());
      PABORTIF(jsonAtt.value().is_object() || jsonAtt.value().is_array(),
               PERR_ARGS,
               "Cannot use json objects yet !");
      std::string key = jsonAtt.key();
      std::string value;
      if (jsonAtt.value().is_number())
        value = boost::json::serialize(jsonAtt.value());
      else if (jsonAtt.value()
                 .is_string()) { // To not read the double quotes ""
        value = jsonAtt.value().as_string();
      } else if (jsonAtt.value().is_bool()) {
        value = (jsonAtt.value().as_bool()) ? "true" : "false";
      }

#define PARAM(name, type, parsed_name, default_value, description)             \
  if (key == parsed_name) {                                                    \
    try {                                                                      \
      this->name = getValue<detail::storage_type_t<type>>(value);              \
    } catch (const std::exception& e) {                                        \
      LOGERROR("Error parsing parameter '%s': %s", parsed_name, e.what());     \
      exit(PERR_ARGS);                                                   \
    }                                                                          \
    continue;                                                                  \
  }
#define CATEGORY(name)
#define SUBCATEGORY(name)
#define ENDCATEGORY(name)
#define ENDSUBCATEGORY(name)
      PARAMETERS
      PABORT(PERR_ARGS, "Unknown Option: %s", key.c_str());
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
#undef ENDCATEGORY
#undef ENDSUBCATEGORY
    }
  } catch (const std::runtime_error& e) {
    PABORT(
      PERR_UNHANDLED_EXCEPTION, "Exception during json parsing %s", e.what());
  }
  Logger::getInstance().setVerbosityLevel(this->verbosity);
  if(this->color == "always") Logger::getInstance().forceColors(true);

  if (!this->cpus) {
    this->cpus = std::thread::hardware_concurrency();
  }
}

std::string
Parameters::helpToString()
{
  std::ostringstream oss;

  // Column width configuration
  const int OPTION_WIDTH = 30;
  const int TYPE_WIDTH = 15;
  const int INDENT = 2; // For the "  " prefix
  const int DEFAULT_INDENT = INDENT + OPTION_WIDTH + TYPE_WIDTH;
  const int SEPARATOR_WIDTH =
    Logger::getTerminalWidth(); // Total width for category separator

  oss << BOLD << "USAGE: solver [options] input.cnf" << RESET << std::endl;
  oss << "Options:" << std::endl << std::endl;

  std::string currentCategory = "";

#define PARAM(name, type, parsed_name, default_value, description)             \
  {                                                                            \
    oss << "  " << GREEN << std::left << std::setw(OPTION_WIDTH)               \
        << ("-" + std::string(parsed_name)) << RESET;                          \
    if (!std::is_same_v<type, bool>) {                                         \
      oss << std::left << std::setw(TYPE_WIDTH) << ("<" #type ">");            \
    } else {                                                                   \
      oss << std::left << std::setw(TYPE_WIDTH) << " ";                        \
    }                                                                          \
    oss << description << std::endl;                                           \
    oss << std::setw(DEFAULT_INDENT) << " "                                    \
        << "(default: " << GREEN << default_value << RESET << ")"              \
        << std::endl;                                                          \
  }

#define CATEGORY(name)                                                         \
  {                                                                            \
    std::string categoryName = "---------[ " #name " ]";                       \
    int dashCount = SEPARATOR_WIDTH - categoryName.length();                   \
    std::string separator(std::max(0, dashCount), '-');                        \
    oss << BLUE << categoryName << separator << RESET << std::endl;            \
  }

#define SUBCATEGORY(name)                                                      \
  {                                                                            \
    oss << CYAN << " [ " << #name << " ] " << std::endl;                       \
  }

#define ENDCATEGORY(name) oss << std::endl;
#define ENDSUBCATEGORY(name)

  PARAMETERS

#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
#undef ENDCATEGORY
#undef ENDSUBCATEGORY

  return oss.str();
}

std::string
Parameters::helpDetailsToString(std::string category)
{
  // Extract all unique categories from PARAMETERS macro
  std::vector<std::string> categories;
  std::ostringstream oss;
  std::string currentCategory = "";

#define PARAM(name, type, parsed_name, default_value, description)
#define CATEGORY(name)                                                         \
  currentCategory = #name;                                                     \
  categories.push_back(currentCategory);
#define SUBCATEGORY(name)
#define ENDCATEGORY(name)
#define ENDSUBCATEGORY(name)

  PARAMETERS

#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
#undef ENDCATEGORY
#undef ENDSUBCATEGORY

  category[0] = std::toupper(category[0]);
  std::transform(category.begin() + 1,
                 category.end(),
                 category.begin() + 1,
                 [](unsigned char c) { return std::tolower(c); });

  // Print header for the category

  oss << BOLD << YELLOW << "\n"
      << category << " Details:" << RESET << std::endl;
  oss << YELLOW << std::string(category.length() + 9, '-') << RESET
      << std::endl;

  if (category == "Portfolio") {
    oss << DETAILED_HELP_PORTFOLIO << std::endl;
  } else if (category == "Solving") {
    oss << DETAILED_HELP_SOLVING << std::endl;
  } else if (category == "Preprocessing") {
    oss << DETAILED_HELP_PREPROCESSING << std::endl;
  } else if (category == "Sharing") {
    oss << DETAILED_HELP_SHARING << std::endl;
  } else if (category == "Global") {
    oss << DETAILED_HELP_GLOBAL << std::endl;
  } else if (category == "*") {
    oss << DETAILED_HELP_PORTFOLIO << std::endl;
    oss << DETAILED_HELP_GLOBAL << std::endl;
    oss << DETAILED_HELP_SOLVING << std::endl;
    oss << DETAILED_HELP_PREPROCESSING << std::endl;
    oss << DETAILED_HELP_SHARING << std::endl;
  } else {
    oss << RED << "Unknown category: " << category << RESET << std::endl;
    oss << "Available categories: ";
    for (size_t i = 0; i < categories.size(); ++i) {
      oss << categories[i] << ", ";
    }
    oss << "* (for all)." << std::endl;
  }

  return oss.str();
}

std::string
Parameters::toString() const
{
  std::ostringstream oss;
  oss << "Parameters: ";

#define PARAM(name, type, parsed_name, default_value, description)             \
  oss << parsed_name << ": " << this->name << "; ";
#define CATEGORY(name)                                                         \
  oss << std::endl << "----" << #name << "----" << std::endl;
#define SUBCATEGORY(name)                                                      \
  oss << std::endl << "--" << #name << "--" << std::endl;
#define ENDCATEGORY(name) oss << std::endl << "--------" << std::endl;
#define ENDSUBCATEGORY(name)
  PARAMETERS
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
#undef ENDCATEGORY
#undef ENDSUBCATEGORY
  oss << std::endl;

  return oss.str();
}