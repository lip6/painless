#ifndef _application_h_INCLUDED
#define _application_h_INCLUDED

struct kissat;

int kissat_application (struct kissat *, int argc, char **argv);
void kissat_mab_init(struct kissat *);
void print_options(struct kissat *solver);
#endif
