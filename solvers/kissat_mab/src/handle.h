#ifndef _handle_h_INCLUDED
#define _handle_h_INCLUDED

void
kissat_mab_init_signal_handler(void (*handler)(int));
void
kissat_mab_reset_signal_handler(void);

void
kissat_mab_init_alarm(void (*handler)(void));
void
kissat_mab_reset_alarm(void);

const char*
kissat_mab_signal_name(int sig);

#endif
