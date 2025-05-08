/*-------------------------------------------------------------------------*/
/* TaSSAT is a SLS SAT solver that implements a weight transferring algorithm.
It is based on Yalsat (by Armin Biere)
Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA, USA. */
/*-------------------------------------------------------------------------*/

#include "config.h"
#include "cflags.h"

#define YALSINTERNAL
#include "yils.h"

#include <stdio.h>

#define MSG(STR) printf ("%s%s\n", prefix, (STR))

void tass_banner (const char * prefix) {
  //MSG ("Version " YALS_VERSION " " YALS_ID);
  MSG ("Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA, USA.");
  //MSG ("Released " YALS_RELEASED);
  MSG ("Compiled " YALS_COMPILED);
  MSG (YALS_OS);
  MSG ("CC " YALS_CC);
  MSG ("CFLAGS " YALS_CFLAGS);
}

const char * tass_version () { return YALS_VERSION " " YALS_ID; }
