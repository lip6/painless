#ifndef _resources_h_INCLUDED
#define _resources_h_INCLUDED

double kissat_inc_wall_clock_time (void);

#ifndef QUIET

#ifndef _resources_h_INLCUDED
#define _resources_h_INLCUDED

#include <stdint.h>

struct kissat;

double kissat_inc_process_time (void);
uint64_t kissat_inc_current_resident_set_size (void);
uint64_t kissat_inc_maximum_resident_set_size (void);
void kissat_inc_print_resources (struct kissat *);

#endif

#endif
#endif
