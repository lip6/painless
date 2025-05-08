#include "Parameters.hpp"
#include "ErrorCodes.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <type_traits>
#include <thread>

Parameters __globalParameters__;

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
	return (value == "true" || value == "1");
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
Parameters::init(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg[0] != '-' && __globalParameters__.filename.empty()) {
			__globalParameters__.filename = arg;
			continue;
		}
		size_t eq_pos = arg.find('=');
		std::string key = (eq_pos == std::string::npos) ? arg.substr(1) : arg.substr(1, eq_pos - 1);
		std::string value = (eq_pos == std::string::npos) ? "true" : arg.substr(eq_pos + 1);

#define PARAM(name, type, parsed_name, default_value, description)                                                     \
	if (key == parsed_name) {                                                                                          \
		try {                                                                                                          \
			__globalParameters__.name = getValue<type>(value);                                                         \
		} catch (const std::exception& e) {                                                                            \
			LOGERROR("Error parsing parameter '%s': %s", parsed_name, e.what());                                       \
			exit(PERR_ARGS_ERROR);                                                                                     \
		}                                                                                                              \
		continue;                                                                                                      \
	}
#define CATEGORY(description)
#define SUBCATEGORY(description)
		PARAMETERS
		PABORT(PERR_ARGS_ERROR,"Unknown Option: %s", key.c_str());
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
	}

	setVerbosityLevel(__globalParameters__.verbosity);

	if (!__globalParameters__.details.empty()) {
		if (__globalParameters__.help) {
			LOGWARN("Both -help and -details are specified. Showing detailed help.");
		}
		Parameters::printDetailedHelp(__globalParameters__.details);
		exit(0);
	}

	if (__globalParameters__.help) {
		Parameters::printHelp();
		exit(0);
	}

	if (!__globalParameters__.enableDistributed && !__globalParameters__.filename.empty() &&
		!std::filesystem::exists(__globalParameters__.filename)) {
		LOGERROR("Error: File '%s' not found", __globalParameters__.filename.c_str());
		exit(PERR_ARGS_ERROR);
	}

	if (__globalParameters__.filename.empty()) {
		LOGERROR("Error: no input file found");
		// printHelp();
		exit(PERR_ARGS_ERROR);
	}

	if(!__globalParameters__.cpus)
	{
		__globalParameters__.cpus = std::thread::hardware_concurrency();
	}
}

void
Parameters::printHelp()
{
	std::cout << BOLD << "USAGE: solver [options] input.cnf" << RESET << std::endl;
	std::cout << "Options:" << std::endl << std::endl;
	std::string currentCategory = "";

#define PARAM(name, type, parsed_name, default_value, description)                                                     \
	{                                                                                                                  \
		std::cout << "  " << GREEN << std::left << std::setw(30) << ("-" + std::string(parsed_name)) << RESET;         \
		if (!std::is_same_v<type, bool>) {                                                                             \
			std::cout << std::left << std::setw(15) << ("<" #type ">");                                                \
		} else {                                                                                                       \
			std::cout << std::left << std::setw(15) << " ";                                                            \
		}                                                                                                              \
		std::cout << description << std::endl;                                                                         \
		std::cout << std::setw(47) << " " << "(default: " << GREEN << default_value << RESET << ")" << std::endl;      \
	}

#define CATEGORY(description)                                                                                          \
	{                                                                                                                  \
		std::cout << BLUE << description << RESET << std::endl;                                                      \
	}
#define SUBCATEGORY(description)                                                                                       \
	{                                                                                                                  \
		std::cout << CYAN << ">> " << description << std::endl;                                                                \
	}

	PARAMETERS
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
	exit(0);
}

void
Parameters::printDetailedHelp(std::string& category)
{
	// Extract all unique categories from PARAMETERS macro
	std::vector<std::string> categories;
	std::string currentCategory = "";

#define PARAM(name, type, parsed_name, default_value, description)
#define CATEGORY(description)                                                                                          \
	currentCategory = description;                                                                                     \
	categories.push_back(currentCategory);
#define SUBCATEGORY(description)

	PARAMETERS

#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY

	category[0] = std::toupper(category[0]);
	std::transform(
		category.begin() + 1, category.end(), category.begin() + 1, [](unsigned char c) { return std::tolower(c); });

	// Print header for the category

	std::cout << BOLD << YELLOW << "\n" << category << " Details:" << RESET << std::endl;
	std::cout << YELLOW << std::string(category.length() + 9, '-') << RESET << std::endl;

	if (category == "Portfolio") {
		std::cout << DETAILED_HELP_PORTFOLIO << std::endl;
	} else if (category == "Solving") {
		std::cout << DETAILED_HELP_SOLVING << std::endl;
	} else if (category == "Preprocessing") {
		std::cout << DETAILED_HELP_PREPROCESSING << std::endl;
	} else if (category == "Sharing") {
		std::cout << DETAILED_HELP_SHARING << std::endl;
	} else if (category == "Global") {
		std::cout << DETAILED_HELP_GLOBAL << std::endl;
	} else if (category == "*") {
		std::cout << DETAILED_HELP_PORTFOLIO << std::endl;
		std::cout << DETAILED_HELP_GLOBAL << std::endl;
		std::cout << DETAILED_HELP_SOLVING << std::endl;
		std::cout << DETAILED_HELP_PREPROCESSING << std::endl;
		std::cout << DETAILED_HELP_SHARING << std::endl;
	} else {
		std::cout << RED << "Unknown category: " << category << RESET << std::endl;
		std::cout << "Available categories: ";
		for (size_t i = 0; i < categories.size(); ++i) {
			std::cout << categories[i] << ", ";
		}
		std::cout << "* (for all)."<< std::endl;
	}
}

void
Parameters::printParams()
{
	std::cout << "Parameters: ";

#define PARAM(name, type, parsed_name, default_value, description)                                                     \
	std::cout << parsed_name << ": " << __globalParameters__.name << "; ";
#define CATEGORY(description)
#define SUBCATEGORY(description) std::cout << std::endl;
	PARAMETERS
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY
	std::cout << std::endl;
}