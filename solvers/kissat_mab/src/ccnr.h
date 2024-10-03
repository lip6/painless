#ifndef _ccnr_h_INCLUDED
#define _ccnr_h_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/times.h> //these two h files are for linux
#include <unistd.h>

// mersenne twist
typedef struct ccanr_randgen ccanr_randgen;
struct ccanr_randgen
{
	unsigned mt[624];
	int 	 mti;
};


// Define a data structure for a literal in the SAT problem.
typedef struct lit lit;

struct lit {
    int clause_num;		//clause num, begin with 0
    int var_num;		//variable num, begin with 1
    int sense;			//is 1 for true literals, 0 for false literals.
};

typedef struct CCAnr CCAnr;

struct CCAnr{
    /*parameters of the instance*/
    int     bms;
    int     num_vars;		//var index from 1 to num_vars 
    int     num_clauses;	//clause index from 0 to num_clauses-1
    int		max_clause_len;
    int		min_clause_len;
    int		formula_len;
    double	avg_clause_len;
    double 	ratio;
    int		ave_weight;   //significant score(sigscore) needed for aspiration
    int		delta_total_weight;
    int		threshold;
    float	p_scale;//w=w*p+ave_w*q
    float	q_scale;
    int		scale_ave;//scale_ave==ave_weight*q_scale
    int 	q_init;
    long long  mems_left;
    //cutoff
    int		max_tries;
    int		tries;
    int		max_flips;
    int		step;
    int* 	score_inc_vars;
    int* 	score_inc_flag;
    int 	score_inc_count;
    /* literal arrays */				
    lit**	var_lit;				//var_lit[i][j] means the j'th literal of var i.
    int*	var_lit_count;          //amount of literals of each var
    lit**	clause_lit;		        //clause_lit[i][j] means the j'th literal of clause i.
    int*	clause_lit_count; 	    // amount of literals in each clause		

    
    /* Information about the variables. */
    int*    score;		
    int*	time_stamp;
    int*	fix;
    int*	cscc;

    /* Information about the clauses */			
    int*     clause_weight;	
    int*     sat_count;		
    int*	 sat_var;
    //unsat clauses stack
    int*	unsat_stack;		 //store the unsat clause number
    int		unsat_stack_fill_pointer;
    int*	index_in_unsat_stack;//which position is a clause in the unsat_stack

    //variables in unsat clauses
    int*	unsatvar_stack;		
    int		unsatvar_stack_fill_pointer;
    int*	index_in_unsatvar_stack;
    int*	unsat_app_count;		//a varible appears in how many unsat clauses

    //configuration changed decreasing variables (score>0 and confchange=1)
    int*	goodvar_stack;		
    int		goodvar_stack_fill_pointer;
    int*	already_in_goodvar_stack;

    //unit clauses preprocess
    int*    in_unit_queue;
    lit*	unitclause_queue;		
    int		unitclause_queue_beg_pointer;
    int     unitclause_queue_end_pointer;
    int*    clause_delete;

    /* Information about solution */
    char*    cur_soln;	//the current solution, with 1's for True variables, and 0's for False variables
    ccanr_randgen randgen;
	char*	best_soln;
	int 	best_cost;

	// confl_trans
    int    confl_num_this_turn;
	int*   conflict_ct;

    // preprocess
    int     fix_var_ct;
    int     del_cls_ct;

    // for soln
    char*   flipped;
    int     flip_ct;
    int*    flip_stack;
    int*    idx_in_flip_stack;
};


struct kissat;
void 	init_CCAnr(CCAnr*);
void 	reinit_CCAnr(CCAnr*);
// void 	confl_trans(CCAnr*);


bool    local_search(struct kissat* solver, CCAnr* lssolver);
int     build_instance(CCAnr*,char *filename);
void    print_solution(CCAnr*);
int     verify_sol(CCAnr*);
void 	alloc_memory(CCAnr*);
void    free_memory(CCAnr*);
void    settings(CCAnr*, char*);
void    ccanr_merseene_init(CCAnr*, int);
void 	update_after_build(CCAnr*);


#endif

