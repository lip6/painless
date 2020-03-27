// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>
#include <string.h>

using namespace std;

extern map<string, string> params;

extern char * filename;

/// Class to manage the parameters
class Parameters
{
public:
   /// Init the parameters.
   static void init(int argc, char ** argv)
   {
      for (int i = 1; i < argc; i++) {
         char * arg = argv[i];

         if (arg[0] != '-' && filename == NULL) {
            filename = arg;
            continue;
         }

         char * eq = strchr(arg, '=');

         if (eq == NULL) {
            params[arg+1];
         } else {
            *eq = 0;

            char * left  = arg+1;
            char * right = eq+1;

            params[left] = right;
         }
      }
   }

   /// Get the input cnf filename.
   static char * getFilename()
   {
      static char * ret = filename;

      return ret;
   }

   /// Print all parameters.
   static void printParams()
   {
      printf("c filename %s\n", filename);

      cout << "c ";

      for (map<string,string>::iterator it = params.begin();
           it != params.end(); it++)
      {
         if (it->second.empty()) {
            cout << it->first << ", ";
         } else {
            cout << it->first << "=" << it->second << ", ";
         }
      }

      cout << "\n";
   }

   /// Return true if the parameters is set otherwise false.
   static bool getBoolParam(const string & name)
   {
      return params.find(name) != params.end();
   }

   /// Get the string value of a parameters with a default value.
   static const string getParam(const string & name,
                                const string & defaultValue)
   {
      if (getBoolParam(name))
         return params[name];

      return defaultValue;
   }

   /// Get the string value of a parameter.
   static const string getParam(const string & name)
   {
      return getParam(name, "");
   }

   /// Get the int value of a parameter with a default value.
   static int getIntParam(const string & name, int defaultValue)
   {
      if (getBoolParam(name))
         return atoi(params[name].c_str());

      return defaultValue;
   }
};
