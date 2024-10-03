#include "ccnr.h"
#include "terminate.h"
#define pop(stack) stack[--stack ## _fill_pointer]
#define push(item, stack) stack[stack ## _fill_pointer++] = item
#define ccanr_Mersenne_N 624
#define ccanr_Mersenne_M 397
#define ccanr_Mersenne_MATRIX_A 0x9908b0dfUL
#define ccanr_Mersenne_UPPER_MASK 0x80000000UL
#define ccanr_Mersenne_LOWER_MASK 0x7fffffffUL
void    ccanr_Mersenne_init_with_seed(ccanr_randgen* randgen, int seed); 
int     ccanr_Mersenne_next(ccanr_randgen* randgen, int bound);  
int     ccanr_rand(CCAnr*, int);


int	 pick_var(CCAnr*);
void init(CCAnr*,char*);
void unit_propagation(CCAnr*);
void preprocess(CCAnr*);
void flip(CCAnr*,int);
void smooth_clause_weights(CCAnr*);
void update_clause_weights(CCAnr*);
void set_clause_weighting(CCAnr*);

void default_initialize(CCAnr* lssolver){
	lssolver->formula_len=0;
	lssolver->unitclause_queue_beg_pointer=0;
    lssolver->unitclause_queue_end_pointer=0;
	lssolver->max_tries = 10000;
	lssolver->max_flips = 200000000;
	lssolver->ave_weight=1;
	lssolver->delta_total_weight=0;
	lssolver->q_scale=0;
	lssolver->q_init=0;
	lssolver->mems_left=50000000;
}

void alloc_memory(CCAnr *lssolver){
	int var_mem = lssolver->num_vars+2;
	int cls_mem = lssolver->num_clauses+2;
	lssolver->score_inc_vars 	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->score_inc_flag 	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->var_lit 			= (lit**)malloc(sizeof(lit*)*var_mem);
	lssolver->var_lit_count 	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->clause_lit  		= (lit**)malloc(sizeof(lit*)*cls_mem);
	lssolver->clause_lit_count	= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->score				= (int*)malloc(sizeof(int)*var_mem);
	lssolver->time_stamp		= (int*)malloc(sizeof(int)*var_mem);
	lssolver->fix				= (int*)malloc(sizeof(int)*var_mem);
	lssolver->cscc 				= (int*)malloc(sizeof(int)*var_mem);
	lssolver->clause_weight		= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->sat_count			= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->sat_var			= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->unsat_stack		= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->index_in_unsat_stack=(int*)malloc(sizeof(int)*cls_mem);
	lssolver->unsatvar_stack  	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->in_unit_queue		= (int*)malloc(sizeof(int)*var_mem);
	lssolver->index_in_unsatvar_stack = (int*)malloc(sizeof(int)*var_mem);
	lssolver->unsat_app_count 	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->goodvar_stack 	= (int*)malloc(sizeof(int)*var_mem);
	lssolver->already_in_goodvar_stack = (int*)malloc(sizeof(int)*var_mem);
	lssolver->unitclause_queue 	= (lit*)malloc(sizeof(lit)*var_mem);
	lssolver->clause_delete 	= (int*)malloc(sizeof(int)*cls_mem);
	lssolver->cur_soln			= (char*)malloc(sizeof(int)*var_mem);
	lssolver->best_soln			= (char*)malloc(sizeof(int)*var_mem);
	lssolver->conflict_ct		= (int*)malloc(sizeof(int)*var_mem);
	// lssolver->in_conflict		= (int*)malloc(sizeof(int)*var_mem);
	lssolver->flip_stack		= (int*)malloc(sizeof(int)*var_mem);
	lssolver->flipped		    = (char*)malloc(sizeof(char)*var_mem);
	lssolver->idx_in_flip_stack = (int*)malloc(sizeof(int)*var_mem);
	for(int i=1;i<=lssolver->num_vars;++i) lssolver->conflict_ct[i]= lssolver->flipped[i] = 0;
	
}

void free_memory(CCAnr *lssolver)
{
	int i;
	for (i = 0; i < lssolver->num_clauses; i++) 
	{
		free(lssolver->clause_lit[i]);
	}
	
	for(i=1; i<=lssolver->num_vars; ++i)
	{
		free(lssolver->var_lit[i]);
	}
	
	free(lssolver->score_inc_vars);
	free(lssolver->score_inc_flag);
	free(lssolver->var_lit);
	free(lssolver->var_lit_count); 
	free(lssolver->clause_lit); 
	free(lssolver->clause_lit_count); 
	free(lssolver->score);
	free(lssolver->time_stamp);
	free(lssolver->fix);	
	free(lssolver->cscc); 
	free(lssolver->clause_weight);	
	free(lssolver->sat_count);	
	free(lssolver->sat_var);
	free(lssolver->unsat_stack);	
	free(lssolver->index_in_unsat_stack);
	free(lssolver->unsatvar_stack); 
	free(lssolver->index_in_unsatvar_stack);
	free(lssolver->unsat_app_count);
	free(lssolver->goodvar_stack);
	free(lssolver->already_in_goodvar_stack);
	free(lssolver->unitclause_queue);
	free(lssolver->clause_delete);
	free(lssolver->cur_soln);
	free(lssolver->best_soln);
	free(lssolver->conflict_ct);
	free(lssolver->in_unit_queue);
	// free(lssolver->in_conflict);
	free(lssolver->flipped);
	free(lssolver->flip_stack);
	free(lssolver->idx_in_flip_stack);
}


//pick a var to be flip
int pick_var(CCAnr *lssolver)
{
	int         i,k,c,v;
	int         best_var;
	lit*		clause_c;
	
	/**Greedy Mode**/
	/*CCD (configuration changed decreasing) mode, the level with configuation chekcing*/
	if(lssolver->goodvar_stack_fill_pointer>0)
	{
		best_var = lssolver->goodvar_stack[0];
		
		if(lssolver->goodvar_stack_fill_pointer <= lssolver->bms*2){
			lssolver->mems_left -= lssolver->goodvar_stack_fill_pointer;
			for(i=1; i<lssolver->goodvar_stack_fill_pointer; ++i)
			{
				v=lssolver->goodvar_stack[i];
				if(lssolver->score[v]>lssolver->score[best_var]) best_var = v;
				else if(lssolver->score[v]==lssolver->score[best_var] && lssolver->time_stamp[v]<lssolver->time_stamp[best_var]) best_var = v;
			}
		}else{
			lssolver->mems_left -= lssolver->bms;
			for(int i=0;i<lssolver->bms;++i){
				int pos = ccanr_rand(lssolver,lssolver->goodvar_stack_fill_pointer-1)+1;
				v=lssolver->goodvar_stack[pos];
				if(lssolver->score[v]>lssolver->score[best_var]) best_var = v;
				else if(lssolver->score[v]==lssolver->score[best_var] && lssolver->time_stamp[v]<lssolver->time_stamp[best_var]) best_var = v;
			}
		}

		
		
		return best_var;
	}
	
	/*SD (significant decreasing) mode, the level with aspiration*/
	best_var = 0;
	if(lssolver->unsatvar_stack_fill_pointer < lssolver->bms*2){
		for(i=0; i<lssolver->unsatvar_stack_fill_pointer; ++i)
		{
			if(lssolver->score[lssolver->unsatvar_stack[i]]>lssolver->ave_weight) 
			{
				best_var = lssolver->unsatvar_stack[i];
				break;
			}
		}

		for(++i; i<lssolver->unsatvar_stack_fill_pointer; ++i)
		{
			v=lssolver->unsatvar_stack[i];
			if(lssolver->score[v]>lssolver->score[best_var]) best_var = v;
			else if(lssolver->score[v]==lssolver->score[best_var] && lssolver->time_stamp[v]<lssolver->time_stamp[best_var]) best_var = v;
		}
	}else{
		for(i=0; i<lssolver->bms; ++i)
		{
			int pos = ccanr_rand(lssolver,lssolver->bms);
			if(lssolver->score[lssolver->unsatvar_stack[pos]]>lssolver->ave_weight) 
			{
				best_var = lssolver->unsatvar_stack[pos];
				break;
			}
		}

		for(++i; i<lssolver->bms; ++i)
		{
			int pos = ccanr_rand(lssolver,lssolver->bms);
			v=lssolver->unsatvar_stack[pos];
			if(lssolver->score[v]>lssolver->score[best_var]) best_var = v;
			else if(lssolver->score[v]==lssolver->score[best_var] && lssolver->time_stamp[v]<lssolver->time_stamp[best_var]) best_var = v;
		}
	}
	if(best_var!=0) return best_var;
		
	/**Diversification Mode**/
	update_clause_weights(lssolver);
	
	/*focused random walk*/
	c = lssolver->unsat_stack[ccanr_rand(lssolver,lssolver->unsat_stack_fill_pointer)];
	clause_c = lssolver->clause_lit[c];
	best_var = clause_c[0].var_num;
	for(k=1; k<lssolver->clause_lit_count[c]; ++k)
	{
		v=clause_c[k].var_num;
		//if(time_stamp[v]<time_stamp[best_var]) best_var = v;
		if(lssolver->score[v]>lssolver->score[best_var]) best_var = v;
		else if(lssolver->score[v]==lssolver->score[best_var]&&lssolver->time_stamp[v]<lssolver->time_stamp[best_var]) best_var = v;
	}
	
	return best_var;
}

//set functions in the algorithm
void settings(CCAnr *lssolver,char *soln)
{
	set_clause_weighting(lssolver);
	init(lssolver,soln);
}

bool local_search(kissat *solver, CCAnr *lssolver)
{
	int flipvar;
	lssolver->flip_ct = 0;
	lssolver->confl_num_this_turn = 0;
	for(int i=1;i<=lssolver->num_vars;++i) {
		lssolver->best_soln[i]=lssolver->cur_soln[i];
		lssolver->flipped[i] = 0;
	}
	ccanr_merseene_init(lssolver, lssolver->num_vars+lssolver->num_clauses);
	lssolver->best_cost = lssolver->num_clauses;
	for (lssolver->step = 0; lssolver->step<lssolver->max_flips; lssolver->step++)
	{
		//find a solution
    	if (TERMINATED(11)) return false;
		if(lssolver->unsat_stack_fill_pointer < lssolver->best_cost){
			lssolver->best_cost = lssolver->unsat_stack_fill_pointer;
			for(int i=0;i<lssolver->flip_ct;++i){
				int v = lssolver->flip_stack[i];
				lssolver->best_soln[v] = 1-lssolver->best_soln[v];
				lssolver->flipped[v] = 0;
			}
			lssolver->flip_ct = 0;
			// for(int i=1;i<=lssolver->num_vars;++i) lssolver->best_soln[i]=lssolver->cur_soln[i];
		}
		if(lssolver->unsat_stack_fill_pointer==0) return true;
		if(lssolver->mems_left<0 && lssolver->step>1000) return false;
		flipvar = pick_var(lssolver);
		flip(lssolver,flipvar);
		lssolver->time_stamp[flipvar] = lssolver->step;

		//update conflicts information 
		lssolver->confl_num_this_turn++;
		for(int i=0;i<lssolver->unsatvar_stack_fill_pointer;++i)
			++lssolver->conflict_ct[lssolver->unsatvar_stack[i]];

	}
	return false;
}

static inline void unsat(CCAnr *lssolver, int clause)
{
	lssolver->index_in_unsat_stack[clause] = lssolver->unsat_stack_fill_pointer;
	push(clause,lssolver->unsat_stack);
	
	//update appreance count of each var in unsat clause and update stack of vars in unsat clauses
	int v;
	for(lit* p=lssolver->clause_lit[clause]; (v=p->var_num)!=0; p++)
	{	
		lssolver->unsat_app_count[v]++;
		if(lssolver->unsat_app_count[v]==1)
		{
			lssolver->index_in_unsatvar_stack[v] = lssolver->unsatvar_stack_fill_pointer;
			push(v,lssolver->unsatvar_stack);	
		}
	}
}

static inline void sat(CCAnr *lssolver, int clause)
{
	int index,last_unsat_clause;

	//since the clause is satisfied, its position can be reused to store the last_unsat_clause
	last_unsat_clause = pop(lssolver->unsat_stack);
	index = lssolver->index_in_unsat_stack[clause];
	lssolver->unsat_stack[index] = last_unsat_clause;
	lssolver->index_in_unsat_stack[last_unsat_clause] = index;
	
	//update appreance count of each var in unsat clause and update stack of vars in unsat clauses
	int v,last_unsat_var;
	for(lit* p=lssolver->clause_lit[clause]; (v=p->var_num)!=0; p++)
	{	
		lssolver->unsat_app_count[v]--;
		if(lssolver->unsat_app_count[v]==0)
		{
			last_unsat_var = pop(lssolver->unsatvar_stack);
			index = lssolver->index_in_unsatvar_stack[v];
			lssolver->unsatvar_stack[index] = last_unsat_var;
			lssolver->index_in_unsatvar_stack[last_unsat_var] = index;
		}
	}
}

//initiation of the algorithm
void init(CCAnr *lssolver,char *soln)
{
	int 		v,c;
	int			i,j;
	
	//Initialize edge weights
	for (c = 0; c<lssolver->num_clauses; c++)
		lssolver->clause_weight[c] = 1;

	//init unsat_stack
	lssolver->unsat_stack_fill_pointer = 0;
	lssolver->unsatvar_stack_fill_pointer = 0;

	//init solution
	for (v = 1; v <= lssolver->num_vars; v++) {
        
        if(lssolver->fix[v]==0){
			if(soln==NULL){
				if(ccanr_rand(lssolver,2)==1) lssolver->cur_soln[v] = 1;
				else lssolver->cur_soln[v] = 0;
			}else{
				lssolver->cur_soln[v] = soln[v-1];
			}

			lssolver->time_stamp[v] = 0;
			lssolver->unsat_app_count[v] = 0;
			lssolver->cscc[v] = 1;
			lssolver->score_inc_flag[v]=0;
		}
		
		
	}

	/* figure out sat_count, and init unsat_stack */
	for (c=0; c<lssolver->num_clauses; ++c) 
	{
		if(lssolver->clause_delete[c]==1) continue;
		
		lssolver->sat_count[c] = 0;
		
		for(j=0; j<lssolver->clause_lit_count[c]; ++j)
		{
			if (lssolver->cur_soln[lssolver->clause_lit[c][j].var_num] == lssolver->clause_lit[c][j].sense)
			{
				lssolver->sat_count[c]++;
				lssolver->sat_var[c] = lssolver->clause_lit[c][j].var_num;	
			}
		}

		if (lssolver->sat_count[c] == 0) 
			unsat(lssolver,c);
	}

	/*figure out var score*/
	int lit_count;
	for (v=1; v<=lssolver->num_vars; v++) 
	{
		if(lssolver->fix[v]==1) 
		{
			lssolver->score[v] = -10000000;
			continue;
		}
		
		lssolver->score[v] = 0;

		lit_count = lssolver->var_lit_count[v];
		
		for(i=0; i<lit_count; ++i)
		{
			c = lssolver->var_lit[v][i].clause_num;
			if (lssolver->sat_count[c]==0) lssolver->score[v]++;
			else if (lssolver->sat_count[c]==1 && lssolver->var_lit[v][i].sense==lssolver->cur_soln[v]) lssolver->score[v]--;
		}
	}
	
		
	//init goodvars stack
	lssolver->goodvar_stack_fill_pointer = 0;
	for (v=1; v<=lssolver->num_vars; v++) 
	{
		if(lssolver->fix[v]==1)  continue;
		if(lssolver->score[v]>0)// && conf_change[v]==1)
		{
			lssolver->already_in_goodvar_stack[v] = 1;
			push(v,lssolver->goodvar_stack);
			
		}
		else lssolver->already_in_goodvar_stack[v] = 0;
	}
	
	//setting for the virtual var 0
	lssolver->time_stamp[0]=0;
	//pscore[0]=0;
}

void flip(CCAnr *lssolver, int flipvar)
{
	if (lssolver->flipped[flipvar]==1)
	{
		lssolver->flipped[flipvar] = 0;
		int last_v = lssolver->flip_stack[--lssolver->flip_ct];
		int idx = lssolver->idx_in_flip_stack[flipvar];
		lssolver->idx_in_flip_stack[last_v] = idx;
		lssolver->flip_stack[idx] = last_v;
	}
	else
	{
		lssolver->flipped[flipvar] = 1;
		lssolver->idx_in_flip_stack[flipvar] = lssolver->flip_ct;
		lssolver->flip_stack[lssolver->flip_ct++] = flipvar;
	}

	lssolver->cur_soln[flipvar] = 1 - lssolver->cur_soln[flipvar];
	
	lssolver->score_inc_count=0;
	
	int i;
	int v,c;

	lit* clause_c;
	
	int org_flipvar_score = lssolver->score[flipvar];
	
	//update related clauses and neighbor vars
	lssolver->mems_left-=lssolver->var_lit_count[flipvar];
	for(lit *q = lssolver->var_lit[flipvar]; (c=q->clause_num)>=0; q++)
	{
		clause_c = lssolver->clause_lit[c];
		if(lssolver->cur_soln[flipvar] == q->sense)
		{
			++lssolver->sat_count[c];
			
			if (lssolver->sat_count[c] == 2) //sat_count from 1 to 2
			{
				lssolver->score[lssolver->sat_var[c]] += lssolver->clause_weight[c];
				
				if(lssolver->score_inc_flag[lssolver->sat_var[c]]!=1)
				{
					lssolver->score_inc_vars[lssolver->score_inc_count++]=lssolver->sat_var[c];
					lssolver->score_inc_flag[lssolver->sat_var[c]]=1;
				}
			}
			else if (lssolver->sat_count[c] == 1) // sat_count from 0 to 1
			{
				lssolver->sat_var[c] = flipvar;//record the only true lit's var
				for(lit* p=clause_c; (v=p->var_num)!=0; p++) 
				{
					lssolver->score[v] -= lssolver->clause_weight[c];
					lssolver->cscc[v] = 1;
				}
                
				sat(lssolver,c);
			}
		}
		else // cur_soln[flipvar] != cur_lit.sense
		{
			--lssolver->sat_count[c];
			if (lssolver->sat_count[c] == 1) //sat_count from 2 to 1
			{
				for(lit* p=clause_c; (v=p->var_num)!=0; p++) 
				{
					if(p->sense == lssolver->cur_soln[v] )
					{
						lssolver->score[v] -= lssolver->clause_weight[c];
						lssolver->sat_var[c] = v;
						break;
					}
				}
			}
			else if (lssolver->sat_count[c] == 0) //sat_count from 1 to 0
			{
				for(lit* p=clause_c; (v=p->var_num)!=0; p++) 
				{
					lssolver->score[v] += lssolver->clause_weight[c];
					lssolver->cscc[v] = 1;
					
					if(lssolver->score_inc_flag[v]!=1)
					{
					lssolver->score_inc_vars[lssolver->score_inc_count++]=v;
					lssolver->score_inc_flag[v]=1;
					}
				}
				unsat(lssolver,c);
			}//end else if
			
		}//end else
	}

	lssolver->score[flipvar] = -org_flipvar_score;
	
	lssolver->cscc[flipvar] = 0;
	
	/*update CCD */
	int index;
	
	for(index=lssolver->goodvar_stack_fill_pointer-1; index>=0; index--)
	{
		v = lssolver->goodvar_stack[index];
		if(lssolver->score[v]<=0)
		{
			lssolver->goodvar_stack[index] = pop(lssolver->goodvar_stack);
			lssolver->already_in_goodvar_stack[v] = 0;
		}	
	}

	
	for (i=0; i<lssolver->score_inc_count; ++i)
	{
		v = lssolver->score_inc_vars[i];
		if(lssolver->score[v]>0 && lssolver->cscc[v]==1 && lssolver->already_in_goodvar_stack[v] ==0)
		{
			push(v,lssolver->goodvar_stack);
			lssolver->already_in_goodvar_stack[v] = 1;
		}
		lssolver->score_inc_flag[v]=0;
	}
}

void update_after_build(CCAnr *lssolver){
	int i,c,v;
	lssolver->avg_clause_len = (double)(lssolver->formula_len+0.0)/lssolver->num_clauses;
	
	//creat var literal arrays
	for (v=1; v<=lssolver->num_vars; ++v)
	{
		lssolver->var_lit[v] = (lit*)malloc(sizeof(lit)*(lssolver->var_lit_count[v]+1));
		lssolver->var_lit_count[v] = 0;	//reset to 0, for build up the array
	}
	//scan all clauses to build up var literal arrays
	for (c = 0; c < lssolver->num_clauses; ++c) 
	{
		for(i=0; i<lssolver->clause_lit_count[c]; ++i)
		{
			v = lssolver->clause_lit[c][i].var_num;
			lssolver->var_lit[v][lssolver->var_lit_count[v]] = lssolver->clause_lit[c][i];
			++lssolver->var_lit_count[v];
		}
	}
	for (v=1; v<=lssolver->num_vars; ++v) //set boundary
		lssolver->var_lit[v][lssolver->var_lit_count[v]].clause_num=-1;
	
	if(lssolver->unitclause_queue_end_pointer>0) preprocess(lssolver);

}



void smooth_clause_weights(CCAnr *lssolver)
{
	int j,c,v;
	int new_total_weight=0;

	for (v=1; v<=lssolver->num_vars; ++v) 
		lssolver->score[v] = 0;
	
	//smooth clause score and update score of variables
	for (c = 0; c<lssolver->num_clauses; ++c)
	{
		if (lssolver->clause_delete[c]==1) continue;
		
		lssolver->clause_weight[c] = lssolver->clause_weight[c]*lssolver->p_scale+lssolver->scale_ave;
		if(lssolver->clause_weight[c]<1) lssolver->clause_weight[c] = 1;
		
		new_total_weight+=lssolver->clause_weight[c];
		
		//update score of variables in this clause 
		if (lssolver->sat_count[c]==0) 
		{
			for(j=0; j<lssolver->clause_lit_count[c]; ++j)
			{
				lssolver->score[lssolver->clause_lit[c][j].var_num] += lssolver->clause_weight[c];
			}
		}
		else  if(lssolver->sat_count[c]==1)
			lssolver->score[lssolver->sat_var[c]]-=lssolver->clause_weight[c];
	}
	
	lssolver->ave_weight=new_total_weight/lssolver->num_clauses;
}

void update_clause_weights(CCAnr *lssolver)
{
	int i,v;
	lssolver->mems_left -= lssolver->unsat_stack_fill_pointer;
	for(i=0; i < lssolver->unsat_stack_fill_pointer; ++i)
		lssolver->clause_weight[lssolver->unsat_stack[i]]++;
	
	for(i=0; i<lssolver->unsatvar_stack_fill_pointer; ++i)
	{
		v = lssolver->unsatvar_stack[i];
		lssolver->score[v] += lssolver->unsat_app_count[v];
		if(lssolver->score[v]>0 &&  lssolver->cscc[v]==1 && lssolver->already_in_goodvar_stack[v] ==0)
		{
			push(v,lssolver->goodvar_stack);
			lssolver->already_in_goodvar_stack[v] =1;
		}
	}
	
	lssolver->delta_total_weight+=lssolver->unsat_stack_fill_pointer;
	if(lssolver->delta_total_weight>=lssolver->num_clauses)
	{
		lssolver->ave_weight+=1;
		lssolver->delta_total_weight -= lssolver->num_clauses;
		
		//smooth weights
		if(lssolver->ave_weight>lssolver->threshold)
			smooth_clause_weights(lssolver);
	}
}

void set_clause_weighting(CCAnr *lssolver)
{	
	lssolver->threshold=300;
	lssolver->p_scale=0.3;
	if(lssolver->q_init==0)
	{
		if(lssolver->ratio<=15) lssolver->q_scale=0;
		else lssolver->q_scale=0.7;
	}
	else 
	{
		if(lssolver->q_scale<0.5)  //0
			lssolver->q_scale = 0.7;
		else
			lssolver->q_scale = 0;
	}
	
	lssolver->scale_ave=(lssolver->threshold+1)*lssolver->q_scale;
	lssolver->q_init = 1;
}

//preprocess
void unit_propagation(CCAnr *lssolver)
{
    lit uc_lit;
    int uc_clause;
    int uc_var;
    bool uc_sense;
    
    int c;
    int i,j;
    lit cur;
    
    
    for(lssolver->unitclause_queue_beg_pointer=0; lssolver->unitclause_queue_beg_pointer < lssolver->unitclause_queue_end_pointer; lssolver->unitclause_queue_beg_pointer++)
    {
        uc_lit = lssolver->unitclause_queue[lssolver->unitclause_queue_beg_pointer];
        
        uc_var = uc_lit.var_num;
        uc_sense = uc_lit.sense;
        
        if(lssolver->fix[uc_var]==1) {if(uc_sense!=lssolver->cur_soln[uc_var])continue;}
     
        lssolver->cur_soln[uc_var] = uc_sense;//fix the variable in unit clause
        lssolver->fix[uc_var] = 1;
        for(i = 0; i<lssolver->var_lit_count[uc_var]; ++i)
        {
            cur = lssolver->var_lit[uc_var][i];
            c = cur.clause_num;
            
            if(lssolver->clause_delete[c]==1) continue;
            
            if(cur.sense == uc_sense)//then remove the clause from var's var_lit[] array
            {
                lssolver->clause_delete[c]=1;
            }
            else
            {
                if(lssolver->clause_lit_count[c]==2)
                {
                    if(lssolver->clause_lit[c][0].var_num == uc_var)
                    {
            			if (!lssolver->in_unit_queue[lssolver->clause_lit[c][1].var_num]) {
                        	lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = lssolver->clause_lit[c][1];
							lssolver->in_unit_queue[lssolver->clause_lit[c][1].var_num] = 1;
						}
                    }
                    else
                    {
						if (!lssolver->in_unit_queue[lssolver->clause_lit[c][0].var_num]) {
                        	lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = lssolver->clause_lit[c][0];
							lssolver->in_unit_queue[lssolver->clause_lit[c][0].var_num] = 1;
						}
                    }
                    
                    lssolver->clause_delete[c]=1;
                }
                else
                {
                    for(j=0; j<lssolver->clause_lit_count[c]; ++j)
                    {
                        if(lssolver->clause_lit[c][j].var_num == uc_var)
                        {
                            lssolver->clause_lit[c][j]=lssolver->clause_lit[c][lssolver->clause_lit_count[c]-1];
    
                            lssolver->clause_lit_count[c]--;
                            
                            break;
                        }
                    }
                }
            }   
        }   
    }
}

void preprocess(CCAnr *lssolver)
{
    int c,v,i;
    int delete_clause_count=0;
    int fix_var_count=0;
    
    unit_propagation(lssolver);
    
    //rescan all clauses to build up var literal arrays
    for (v=1; v<=lssolver->num_vars; ++v) 
        lssolver->var_lit_count[v] = 0;
    
    lssolver->max_clause_len = 0;
	lssolver->min_clause_len = lssolver->num_vars;
    int    formula_len=0;
    
    for (c = 0; c < lssolver->num_clauses; ++c) 
    {
        if(lssolver->clause_delete[c]==1) {
            delete_clause_count++;
            continue;
        }
        
        for(i=0; i<lssolver->clause_lit_count[c]; ++i)
        {
            v = lssolver->clause_lit[c][i].var_num;
            lssolver->var_lit[v][lssolver->var_lit_count[v]] = lssolver->clause_lit[c][i];
            ++lssolver->var_lit_count[v];
        }
        lssolver->clause_lit[c][i].var_num=0; //new clause boundary
        lssolver->clause_lit[c][i].clause_num = -1;
        
        //about clause length
        formula_len += lssolver->clause_lit_count[c];
        
        if(lssolver->clause_lit_count[c] > lssolver->max_clause_len)
            lssolver->max_clause_len = lssolver->clause_lit_count[c];
        else if(lssolver->clause_lit_count[c] < lssolver->min_clause_len)
            lssolver->min_clause_len = lssolver->clause_lit_count[c];
    }
    
    lssolver->avg_clause_len = (double)(formula_len+0.0)/lssolver->num_clauses;
    
    for (v=1; v<=lssolver->num_vars; ++v) 
    {
    	if(lssolver->fix[v]==1)
    	{
    		fix_var_count++;
    	}
        lssolver->var_lit[v][lssolver->var_lit_count[v]].clause_num=-1;//new var_lit boundary
    }
	lssolver->fix_var_ct = fix_var_count;
	lssolver->del_cls_ct = delete_clause_count;
    
}



void ccanr_merseene_init(CCAnr* lssolver, int seed){
	ccanr_Mersenne_init_with_seed(&(lssolver->randgen),seed);
}

int ccanr_rand(CCAnr* lssolver,int bound){
	return ccanr_Mersenne_next(&(lssolver->randgen),bound);
}


void ccanr_Mersenne_init_with_seed(ccanr_randgen* randgen, int seed){
	unsigned int s = ((unsigned int) (seed << 1)) + 1;
	randgen->mt[0] = s & 0xffffffffUL;
	for(randgen->mti = 1; randgen->mti < ccanr_Mersenne_N; randgen->mti++) {
		randgen->mt[randgen->mti] = (1812433253UL * (randgen->mt[randgen->mti - 1] ^ (randgen->mt[randgen->mti - 1] >> 30)) + randgen->mti);
		randgen->mt[randgen->mti] &= 0xffffffffUL;
	}
}

int ccanr_Mersenne_next31(ccanr_randgen* randgen){
	unsigned int y;
	static unsigned int mag01[2] = {0x0UL, ccanr_Mersenne_MATRIX_A};
	if(randgen->mti >= ccanr_Mersenne_N) {
		int kk;
		for(kk = 0; kk < ccanr_Mersenne_N - ccanr_Mersenne_M; kk++) {
		y = (randgen->mt[kk] & ccanr_Mersenne_UPPER_MASK) | (randgen->mt[kk + 1] & ccanr_Mersenne_LOWER_MASK);
		randgen->mt[kk] = randgen->mt[kk + ccanr_Mersenne_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for(; kk < ccanr_Mersenne_N - 1; kk++) {
		y = (randgen->mt[kk] & ccanr_Mersenne_UPPER_MASK) | (randgen->mt[kk + 1] & ccanr_Mersenne_LOWER_MASK);
		randgen->mt[kk] = randgen->mt[kk + (ccanr_Mersenne_M - ccanr_Mersenne_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		y = (randgen->mt[ccanr_Mersenne_N - 1] & ccanr_Mersenne_UPPER_MASK) | (randgen->mt[0] & ccanr_Mersenne_LOWER_MASK);
		randgen->mt[ccanr_Mersenne_N - 1] = randgen->mt[ccanr_Mersenne_M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
		randgen->mti = 0;
	}
	y = randgen->mt[randgen->mti++];
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);
	return  (int) (y>>1);
}

int ccanr_Mersenne_next(ccanr_randgen* randgen, int bound){
	unsigned int value;
	do {
		value = ccanr_Mersenne_next31(randgen);
	} while(value + (unsigned int) bound >= 0x80000000UL);
	return (int) (value % bound);
}


void init_CCAnr(CCAnr* lssolver){
	int seed = 1;
	ccanr_merseene_init(lssolver,seed);
	default_initialize(lssolver);
}

void reinit_CCAnr(CCAnr *lssolver){
	free_memory(lssolver);
	init_CCAnr(lssolver);
}

// void confl_trans(CCAnr* lssolver){
// 	lssolver->in_conflict_sz = 0;
// 	if(lssolver->step==0) return;
// 	for(int i=1;i<=lssolver->num_vars;++i){
// 		lssolver->conflict_ct[i] = lssolver->conflict_ct[i]*100/lssolver->step;
// 		if(lssolver->conflict_ct[i]>0){
// 			lssolver->in_conflict[lssolver->in_conflict_sz++] = i;
// 		}
// 	}
// }