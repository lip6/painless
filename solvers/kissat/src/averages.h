#ifndef _averages_h_INCLUDED
#define _averages_h_INCLUDED

#include "smooth.h"

#include <stdbool.h>

typedef struct averages averages;
typedef struct common_averages common_averages;

// Commong averages for all modes
struct common_averages {
    bool initialized;
    smooth units, learned_units;
    smooth med_glue;
};

struct averages {
  bool initialized;
  smooth fast_glue, slow_glue;
#ifndef QUIET
  smooth level, size, trail;
#endif
  smooth decision_rate;
  uint64_t saved_decisions;
};

struct kissat;

void kissat_init_averages (struct kissat *, averages *);
void kissat_init_caverages (struct kissat *, common_averages *);

#define AVERAGES (solver->averages[solver->stable])

#define EMA(NAME) (AVERAGES.NAME)

#define AVERAGE(NAME) (EMA (NAME).value)

#define UPDATE_AVERAGE(NAME, VALUE) \
  kissat_update_smooth (solver, &EMA (NAME), VALUE)

#define CAVERAGES (solver->common_averages)

#define CEMA(NAME) (CAVERAGES.NAME)

#define CAVERAGE(NAME) (CEMA (NAME).value)

#define UPDATE_CAVERAGE(NAME, VALUE) \
  kissat_update_smooth (solver, &CEMA (NAME), VALUE)

#endif
