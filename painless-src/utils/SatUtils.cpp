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

#include "../utils/SatUtils.h"
#include "../clauses/ClauseManager.h"

#include <ctype.h>
#include <stdio.h>
#include <math.h>

static unsigned intWidth(int i)
{
   if (i == 0)
      return 1;
   
   return (i < 0) + 1 + (unsigned) log10(fabs(i));
}


void printModel(vector<int> & model)
{
  model.push_back(0);
  
  unsigned usedWidth = 0;
  
  for (unsigned i = 0; i < model.size(); i++) {
     if (usedWidth + 1 + intWidth(model[i]) > 80) {
        printf("\n");
        usedWidth = 0;
     }
  
     if (usedWidth == 0) {
        usedWidth += printf("v");
     }
  
     usedWidth += printf(" %d", model[i]);
  }

  printf("\n");
}


bool loadFormulaToSolvers(vector<SolverInterface*> solvers,
                          const char* filename)
{
	FILE* f = fopen(filename, "r");

	if (f == NULL)
		return false;
	
	int c    = 0;
	bool neg = false;

	vector<ClauseExchange *> clauses;

	vector<int> cls;

	while (c != EOF) {
		c = fgetc(f);

		// comment or problem definition line
		if (c == 'c' || c == 'p') {
			// skip this line
			while(c != '\n') {
				c = fgetc(f);
			}

			continue;
		}
		// whitespace
		if (isspace(c))
			continue;
		
		// negative
		if (c == '-') {
			neg = true;
			continue;
		}

		// number
		if (isdigit(c)) {
			int num = c - '0';

			c = fgetc(f);

			while (isdigit(c)) {
				num = num*10 + (c-'0');
				c = fgetc(f);
			}

			if (neg) {
				num *= -1;
			}

			neg = false;

			if (num != 0) {
				cls.push_back(num);
			} else {
				ClauseExchange * ncls = ClauseManager::allocClause(cls.size());

				for (size_t i=0; i<cls.size(); i++) {
				   ncls->lits[i] = cls[i];
				}

				ClauseManager::increaseClause(ncls, solvers.size());

				clauses.push_back(ncls);

				cls.clear();
			}
		}
	}

	fclose(f);

	for (size_t i = 0; i < solvers.size(); i++) {
		solvers[i]->addInitialClauses(clauses);
	}

	return true;
}
