#include "Parameters.h"
#include "ErrorCodes.h"
#include <filesystem>

char *Parameters::binName = nullptr;
std::map<std::string, std::string> Parameters::params;
char *Parameters::filename = nullptr;

// Implement the methods here...
void Parameters::init(int argc, char **argv)
{
   binName = argv[0];
   for (int i = 1; i < argc; i++)
   {
      char *arg = argv[i];

      if (arg[0] != '-' && filename == NULL)
      {
         filename = arg;

         continue;
      }

      char *eq = strchr(arg, '=');

      if (eq == NULL)
      {
         params[arg + 1];
      }
      else
      {
         *eq = 0;

         char *left = arg + 1;
         char *right = eq + 1;

         params[left] = right;
      }
   }

   // Check if the file exists only in non dist for now
   if (!Parameters::getBoolParam("dist") && !std::filesystem::exists(filename))
   {
      LOGERROR("Error: File '%s' not found", filename);
      exit(PERR_ARGS_ERROR); // Or handle the error appropriately
   }

   if (getFilename() == NULL || getBoolParam("h"))
      printHelp();

   // TODO : detect number of cores, sockets, and if hyperthreading to chose default for "c" and warn user if overbooking
}

char *Parameters::getFilename()
{
   static char *ret = filename;

   return ret;
}

bool Parameters::getBoolParam(const std::string &name)
{
   return params.find(name) != params.end();
}

const std::string Parameters::getParam(const std::string &name, const std::string &defaultValue)
{
   if (getBoolParam(name))
      return params[name];

   return defaultValue;
}

const std::string Parameters::getParam(const std::string &name)
{
   return getParam(name, "");
}

int Parameters::getIntParam(const std::string &name, int defaultValue)
{
   if (getBoolParam(name))
      return atoi(params[name].c_str());

   return defaultValue;
}

void Parameters::printParams()
{
   LOG("p filename %s", filename);

   std::cout << "p ";

   for (std::map<std::string, std::string>::iterator it = params.begin();
        it != params.end(); it++)
   {
      if (it->second.empty())
      {
         std::cout << it->first << ", ";
      }
      else
      {
         std::cout << it->first << "=" << it->second << ", ";
      }
   }

   std::cout << std::endl;
}

void Parameters::printHelp()
{
   std::cout << "USAGE: " << binName << " [options] input.cnf" << std::endl;
   std::cout << "Options:" << std::endl;
   std::cout << "General:" << std::endl;
   std::cout << "\t-c=<INT>\t\t number of cpus, default is 24" << std::endl;
   // std::cout << "\t-max-memory=<INT>\t memory limit in GB, default is 200" << std::endl;
   std::cout << "\t-t=<INT>\t\t timeout in seconds, default is no limit" << std::endl;
   std::cout << "\t-v=<INT>\t\t verbosity level, default is 0" << std::endl;
   std::cout << "\t-solver=<STR>\t\t the portfolio of solvers, approx equal number of threads per letter: k(default): kissat, a: maple, y: yalsat" << std::endl;
   std::cout << "Local Sharing:" << std::endl;
   std::cout << "\t-dup\t\t enable duplicates detection if supported by the local sharing strategy choosen" << std::endl;
   std::cout << "\t-shr-strat=<INT>\t\t strategy for local sharing value in [1,5], 0 by default" << std::endl;
   std::cout << "\t-shr-sleep=<INT>\t time in useconds a sharing strategy makes a sharer sleep each round, default is 500000 (0.5s)" << std::endl;
   std::cout << "\t-shr-lit=<INT>\t\t number of literals shared per round, default is 1500" << std::endl;
   std::cout << "\t-shr-initial-lbd=<UNSIGNED>\t\t the initial value of lbd limit for the producers in sharing strategies (constant for SimpleSharing), default 2." << std::endl;
   std::cout << "\t-shr-horde-init-round=<UNSIGNED>\t\t the number of rounds before the HordesatSharingAlt strategy starts to increase/decrease the lbdlimit, default is 1." << std::endl;
   std::cout << "Global Sharing:" << std::endl;
   std::cout << "\t-dist\t\t enable distributed(global) sharing, disabled by default" << std::endl;
   std::cout << "\t-gshr-lit=<INT>\t\t number of literals (+metadata) shared per round in the global sharing strategy, default is 1500*c" << std::endl;
   std::cout << "\t-gshr-strat=<INT>\t\t global sharing strategy, by default 1 (allgather)" << std::endl;
   std::cout << "\t-nb-bloom-gstrat=<INT>\t\t the number if bloom filters to use in the global sharing strategy, by default (1), possible values (0,1,2)" << std::endl;
   std::cout << "\t-max-cls-size=<INT>\t\t the maximum size of clauses to be added to the limited databases, eg: ClauseDatabaseLmited used in GlobalDatabase" << std::endl;
   std::cout << "Preprocessing and diversification:" << std::endl;
   std::cout << "\t-initshuffle\t\t initshuffle" << std::endl;
   std::cout << "\t-max-div-noise\t\t the maximum noise used by the random engine" << std::endl;
   exit(0);
}