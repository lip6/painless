#ifndef _require_h_INCLUDED
#define _require_h_INCLUDED

#define kissat_mab_require(COND, ...)                                                                                  \
	do {                                                                                                               \
		if ((COND))                                                                                                    \
			break;                                                                                                     \
		kissat_mab_fatal_message_start();                                                                              \
		fprintf(stderr, "calling '%s': ", __func__);                                                                   \
		fprintf(stderr, __VA_ARGS__);                                                                                  \
		fputc('\n', stderr);                                                                                           \
		fflush(stderr);                                                                                                \
		kissat_mab_abort();                                                                                            \
	} while (0)

#define kissat_mab_require_initialized(SOLVER) kissat_mab_require(SOLVER, "uninitialized")

#define kissat_mab_require_valid_external_internal(LIT)                                                                \
	do {                                                                                                               \
		kissat_mab_require((LIT) != INT_MIN, "invalid literal '%d' (INT_MIN)", (LIT));                                 \
		const int TMP_IDX = ABS(LIT);                                                                                  \
		kissat_mab_require(                                                                                            \
			TMP_IDX <= EXTERNAL_MAX_VAR, "invalid literal '%d' (variable larger than %d)", (LIT), EXTERNAL_MAX_VAR);   \
		assert(VALID_EXTERNAL_LITERAL(LIT));                                                                           \
	} while (0)

#endif
