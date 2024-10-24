#ifndef _print_h_INCLUDED
#define _print_h_INCLUDED

#ifndef QUIET

#include <stdint.h>

struct kissat;

int
kissat_mab_verbosity(struct kissat*);

void
kissat_mab_signal(struct kissat*, const char* type, int sig);
void
kissat_mab_message(struct kissat*, const char* fmt, ...);
void
kissat_mab_section(struct kissat*, const char* name);
void
kissat_mab_verbose(struct kissat*, const char* fmt, ...);
void
kissat_mab_very_verbose(struct kissat*, const char* fmt, ...);
void
kissat_mab_extremely_verbose(struct kissat*, const char* fmt, ...);
void
kissat_mab_warning(struct kissat*, const char* fmt, ...);

void
kissat_mab_phase(struct kissat*, const char* name, uint64_t, const char* fmt, ...);
#else

#define kissat_mab_message(...)                                                                                        \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_phase(...)                                                                                          \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_section(...)                                                                                        \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_signal(...)                                                                                         \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_verbose(...)                                                                                        \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_very_verbose(...)                                                                                   \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_extremely_verbose(...)                                                                              \
	do {                                                                                                               \
	} while (0)
#define kissat_mab_warning(...)                                                                                        \
	do {                                                                                                               \
	} while (0)

#endif

#endif
