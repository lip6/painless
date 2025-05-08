/*-------------------------------------------------------------------------*/
/* TaSSAT is a SLS SAT solver that implements a weight transferring algorithm.
It is based on Yalsat (by Armin Biere)
Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA, USA. */
/*-------------------------------------------------------------------------*/

#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include "yals.h"


Yals * primaryworker; 
int * preprocessedtrail;
int preprocessedtrail_size;
void set_primary_worker (Yals *yals);
void set_preprocessed_trail ();
int * get_cdb_start ();
int * get_cdb_end ();
int * get_cdb_top ();
int * get_occs (); 
int get_noccs ();
int * get_refs (); 
int * get_lits ();
int get_numvars ();
int * get_preprocessed_trail ();
int get_preprocessed_trail_size ();
void delete_temp_shared_structures ();
