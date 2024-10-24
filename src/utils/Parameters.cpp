#include "Parameters.hpp"
#include "ErrorCodes.hpp"
#include "Logger.hpp"
#include <cstring>
#include <filesystem>
#include <type_traits>

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

// You can add more specializations for other types as needed

// In your Parameters::init function:
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

		PARAMETERS
#undef PARAM
#undef CATEGORY
	}

	if (!__globalParameters__.enableDistributed && !__globalParameters__.filename.empty() &&
		!std::filesystem::exists(__globalParameters__.filename)) {
		LOGERROR("Error: File '%s' not found", __globalParameters__.filename.c_str());
		exit(PERR_ARGS_ERROR); // Or handle the error appropriately
	}

	if (__globalParameters__.filename.empty()) {
		LOGERROR("Error: no input file found");
		printHelp();
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
		std::cout << "  " << GREEN << std::left << std::setw(30) << ("-" + std::string(parsed_name)) << RESET;                       \
		if (!std::is_same_v<type, bool>) {                                                                             \
			std::cout << std::left << std::setw(15) << ("<" #type ">");                                                \
		} else {                                                                                                       \
			std::cout << std::left << std::setw(15) << " ";                                                            \
		}                                                                                                              \
		std::cout << description << std::endl;                                                                         \
		std::cout << std::setw(47) << " " << "(default: " << GREEN << default_value << RESET <<")" << std::endl;                        \
	}

#define CATEGORY(description)                                                                                          \
	{                                                                                                                  \
		std::cout << YELLOW << description << RESET << std::endl;                                                      \
	}

	PARAMETERS
#undef PARAM
#undef CATEGORY

	exit(0);
}

void
Parameters::printParams()
{
	std::cout << "Parameters: ";

#define PARAM(name, type, parsed_name, default_value, description)                                                     \
	std::cout << parsed_name << ": " << __globalParameters__.name << "; ";
#define CATEGORY(description)
	std::cout << std::endl;
	PARAMETERS
#undef PARAM
#undef CATEGORY
}