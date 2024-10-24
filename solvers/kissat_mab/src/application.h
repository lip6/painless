#ifndef _application_h_INCLUDED
#define _application_h_INCLUDED

struct kissat;

int
kissat_mab_application(struct kissat*, int argc, char** argv);
void
print_options(struct kissat* solver);
#endif
