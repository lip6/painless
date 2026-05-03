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

/** @brief Error code for argument errors */
#define PERR_ARGS -6

/** @brief Error code for possible out of bounds accesses */
#define PERR_BOUND -7

#define PERR_UNHANDLED_EXCEPTION -8

#define PERR_BAD_DATA -9

#define PERR_BAD_BEHAVIOR -10

/** @brief Error code for topology error */
#define PERR_TOPOLOGY -11

/** @brief Error code for unknown solver types */
#define PERR_UNKNOWN_DATABASE -12

/** @brief Warning code for CPU count mismatch in load strategy */
#define PWARN_LSTRAT_CPU_COUNT 1
