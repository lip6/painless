#ifndef _error_h_INCLUDED
#define _error_h_INCLUDED

void kissat_inc_error (const char *fmt, ...);
void kissat_inc_fatal (const char *fmt, ...);

void kissat_inc_fatal_message_start (void);

void kissat_inc_call_function_instead_of_abort (void (*)(void));
void kissat_inc_abort ();

#endif
