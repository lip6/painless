#ifndef _handle_h_INCLUDED
#define _handle_h_INCLUDED

void kissat_inc_init_signal_handler (void (*handler) (int));
void kissat_inc_reset_signal_handler (void);

void kissat_inc_init_alarm (void (*handler) (void));
void kissat_inc_reset_alarm (void);

const char *kissat_inc_signal_name (int sig);

#endif
