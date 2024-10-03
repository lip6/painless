#pragma once

#include "file.h"
#include "saga.h"

enum strictness
{
  RELAXED_PARSING = 0,
  NORMAL_PARSING = 1,
  PEDANTIC_PARSING = 2,
};

typedef enum strictness strictness;
namespace gaspi
{
  const char *kissat_parse_dimacs(struct kissat *, strictness, file *,
                                  uint64_t *linenoptr, int *max_var_ptr);
}
