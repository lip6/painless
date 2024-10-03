#include "colors.h"

#include <unistd.h>

int gkissat_is_terminal[3] = { 0, -1, -1 };

int
kissat_initialize_terminal (int fd)
{
  assert (fd == 1 || fd == 2);
  assert (gkissat_is_terminal[fd] < 0);
  return gkissat_is_terminal[fd] = isatty (fd);
}

void
kissat_force_colors (void)
{
  gkissat_is_terminal[1] = gkissat_is_terminal[2] = 1;
}

void
kissat_force_no_colors (void)
{
  gkissat_is_terminal[1] = gkissat_is_terminal[2] = 0;
}
