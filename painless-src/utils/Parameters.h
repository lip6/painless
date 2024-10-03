#pragma once

#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "Logger.h"

extern std::map<std::string, std::string> params;

extern char *filename;

/// \class Parameters
/// \brief Class to manage the parameters
/// \defgroup utils
/// \ingroup utils
class Parameters
{
public:

   Parameters() = delete;

   /// \brief Initializes the parameters using the command line arguments.
   /// \param argc The number of command line arguments.
   /// \param argv The array of command line arguments.
   static void init(int argc, char **argv);

   /// \brief Gets the filename parameter.
   /// \return The filename parameter as a C-style string.
   static char *getFilename();

   /// \brief Check if flag exists
   /// \param name The name of the flag
   /// \return True if the flag exists, otherwise false
   static bool getBoolParam(const std::string &name);

   /// \brief Gets a string parameter by name with a default value.
   /// \param name The name of the string parameter.
   /// \param defaultValue The default value to return if the parameter is not found.
   /// \return The value of the parameter as a string.
   static const std::string getParam(const std::string &name, const std::string &defaultValue);

   /// \brief Gets a string parameter by name.
   /// \param name The name of the string parameter.
   /// \return The value of the parameter as a string.
   static const std::string getParam(const std::string &name);

   /// \brief Gets an integer parameter by name with a default value.
   /// \param name The name of the integer parameter.
   /// \param defaultValue The default value to return if the parameter is not found.
   /// \return The value of the parameter as an integer.
   static int getIntParam(const std::string &name, int defaultValue);

   /// \brief Prints all the parameters to the standard output.
   static void printParams();

   /// \brief Prints the help message for the parameters.
   static void printHelp();

private:
   static char *binName;
   static std::map<std::string, std::string> params;
   static char * filename;
};
