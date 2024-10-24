#include "colors.h"

#include <unistd.h>

int kissat_mab_is_terminal[3] = { 0, -1, -1 };

int
kissat_mab_initialize_terminal (int fd)
{
  assert (fd == 1 || fd == 2);
  assert (kissat_mab_is_terminal[fd] < 0);
  return kissat_mab_is_terminal[fd] = isatty (fd);
}

void
kissat_mab_force_colors (void)
{
  kissat_mab_is_terminal[1] = kissat_mab_is_terminal[2] = 1;
}

void
kissat_mab_force_no_colors (void)
{
  kissat_mab_is_terminal[1] = kissat_mab_is_terminal[2] = 0;
}
