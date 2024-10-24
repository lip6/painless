#include "stack.h"

typedef struct painless_clause painless_clause;

struct painless_clause
{
	ints lits;	   // 4 members of 8 bytes in 64-bits arch
	unsigned glue; // : 30; // glue value is unlikely to reach values >= 2^30
				   // bool contains_eliminated : 1; // eliminated variable (autarky, bve , ...) look for
	// kissat_mab_mark_eliminated_variable() bool satisfied : 1; bool shrink : 1;
};