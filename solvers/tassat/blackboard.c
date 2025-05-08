/*-------------------------------------------------------------------------*/
/* TaSSAT is a SLS SAT solver that implements a weight transferring algorithm. 
It is based on Yalsat (by Armin Biere)
Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA, USA. */
/*-------------------------------------------------------------------------*/

#include "blackboard.h"

void set_primary_worker (Yals *yals)
{
    primaryworker = yals;
    preprocessedtrail_size = -1;
}

void set_preprocessed_trail ()
{
    preprocessedtrail_size = preprocessed_trail_size (primaryworker);
    preprocessedtrail = malloc (preprocessedtrail_size* sizeof (int));
    int * arr = preprocessed_trail (primaryworker);
    memcpy (preprocessedtrail, arr, preprocessedtrail_size* sizeof (int));
    free (arr);
}

int * get_cdb_start () { return cdb_start (primaryworker);}

int * get_cdb_top () { return cdb_top (primaryworker);}

int * get_cdb_end () { return cdb_end (primaryworker);}

int * get_occs () { return occs (primaryworker);}

int get_noccs () { return noccs (primaryworker);}

int * get_refs () { return refs (primaryworker);}

int * get_lits () { return lits (primaryworker); }

int get_numvars () {return num_vars (primaryworker);}

int * get_preprocessed_trail () { return preprocessedtrail;}

int get_preprocessed_trail_size () { return preprocessedtrail_size;}

//void delete_temp_shared_structures () {free (preprocessed_trail);}

