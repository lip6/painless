#pragma once

#include <vector>

// Clause size
typedef unsigned int csize_t;
// Literal type
typedef int lit_t;
// Simple Clause as standard vector
typedef std::vector<lit_t> simpleClause;
// Type for object ids
typedef int plid_it;
// Type for lbd value (glue)
typedef unsigned int lbd_t;
// Type for reference counting (intrusive pointers, ...)
typedef unsigned int rcount_t;
// Type for hash of clause
typedef long long hash_t;

/// Struct for pointer, size pair (mainly used to easily hash clauses from C written solvers)
struct ClikeClause
{
	csize_t size;
	lit_t* lits;

	lit_t* begin() { return lits; }
	lit_t* end() { return lits + size; }

	const lit_t* begin() const { return lits; }
	const lit_t* end() const { return lits + size; }
};

// Bounds

/// Maximum acceptable literal, and also variable
#define MAX_LIT ((1U << (sizeof(lit_t) * 8 - 1)) - 1)
/// Minimum acceptable literal
#define MIN_LIT (-MAX_LIT)
/// Maximum clause size supported by the type
#define MAX_CSIZE ((csize_t)-1)