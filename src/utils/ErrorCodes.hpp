/**
 * @file errors.h
 * @brief Error and warning codes for the SAT solver
 */

#pragma once

/** @brief Error code for parsing failures */
#define PERR_PARSING -1

/** @brief Error code for MPI-related failures */
#define PERR_MPI -2

/** @brief Error code for distributed compilation failures */
#define PERR_DIST_COMPILE -3

/** @brief Error code for unsupported operations or features */
#define PERR_NOT_SUPPORTED -4

/** @brief Error code for unknown solver types */
#define PERR_UNKNOWN_SOLVER -5

/** @brief Error code for command-line argument parsing failures */
#define PERR_ARGS_ERROR -6

/** @brief Warning code for CPU count mismatch in load strategy */
#define PWARN_LSTRAT_CPU_COUNT 1
