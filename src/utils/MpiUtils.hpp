#pragma once

#include "utils/Logger.hpp"

using mpirank_t = int;

#ifndef NDIST

#define MY_MPI_END 2012
#define MYMPI_CLAUSES 1
#define MYMPI_BITSET 1
#define MYMPI_OK 2
#define MYMPI_NOTOK 3
#define MYMPI_MODEL 4

#define COLOR_YES 10

#define TESTRUNMPI(func)                                                       \
  do {                                                                         \
    int result = (func);                                                       \
    if (result != MPI_SUCCESS) {                                               \
      LOGERROR("MPI ERROR: %d in function %s", result, #func);                 \
      exit(PERR_MPI);                                                          \
    }                                                                          \
  } while (0)

#endif