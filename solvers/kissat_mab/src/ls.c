#include "ls.h"
#include "ccnr.h"
#include "trail.h"
#include "terminate.h"
#include <time.h>

bool kissat_mab_ccanring (kissat *solver) {
    if (GET_OPTION(ccanr) && solver->freeze_ls_restart_num < 1) return true;
    return false;
}

inline double cpuTime(void) { return (double)clock() / CLOCKS_PER_SEC; }

int toFormul(int ilit) {
    if (ilit & 1) return -(ilit >> 1) - 1;
    else return (ilit >> 1) + 1;
}

void load_ls_data(kissat *solver, CCAnr *lssolver) {
    int ls_var = VARS, ls_cls = 0;
    
    const word *arena = BEGIN_STACK(solver->arena);
    clause *start = (clause *)(arena);
    const clause *end = (clause *)END_STACK(solver->arena);
    for (clause *c = start; c != end; c = kissat_mab_next_clause(c)) {
        if (c->garbage) continue;
        if (c->redundant && c->glue > GET_OPTION(tier2)) continue;
        ls_cls++;
    }
    if (TERMINATED(11)) return;
    litpairs binary_clause;
    INIT_STACK(binary_clause); 
    watches *all_watches = solver->watches;
    for (all_literals(lit)) {
        watches *watches = all_watches + lit;
        watch *p = BEGIN_WATCHES(*watches);
        const watch *end_watches = END_WATCHES(*watches);
        while (p != end_watches) {
            const watch watch = *p++;
            if (watch.type.binary) {
                const unsigned other = watch.binary.lit;
                if (lit < other) {
                    const litpair litpair = {.lits = {lit, other}};
                    PUSH_STACK(binary_clause, litpair);
                }
            }
        }
    }
    if (TERMINATED(11)) return;
    int binary_size = SIZE_STACK(binary_clause); 
    ls_cls += binary_size;

    lssolver->num_vars = ls_var;
    lssolver->num_clauses = ls_cls;
    lssolver->mems_left = (long long) solver->ls_mems_num;
    lssolver->ratio = (lssolver->num_clauses+0.0)/lssolver->num_vars;
    alloc_memory(lssolver);
    for (int c = 0; c < lssolver->num_clauses; c++) 
		lssolver->clause_lit_count[c] = lssolver->clause_delete[c] = 0;
	for (int v=1; v<=lssolver->num_vars; ++v)
		lssolver->var_lit_count[v] = lssolver->fix[v] = lssolver->in_unit_queue[v] = solver->ls_active[v - 1] = 0;
	lssolver->max_clause_len = 0;
	lssolver->min_clause_len = lssolver->num_vars;
    if (TERMINATED(11)) return;
    int cls_ct = 0, mx_var = 0;
    for (clause *c = start; c != end; c = kissat_mab_next_clause(c)) {
        if (TERMINATED(11)) return;
        if (c->garbage) continue;
        if (c->redundant && c->glue > GET_OPTION(tier2)) continue;
        int size = c->size;
        lssolver->clause_lit_count[cls_ct] = size;
        lssolver->clause_lit[cls_ct] = (lit*)malloc(sizeof(lit)*(lssolver->clause_lit_count[cls_ct]+1));    
        int i = 0;
        for (; i < size; i++) {
            int idx = IDX (c->lits[i]);
            solver->ls_active[idx] = 1;
            flags *f = FLAGS (idx);
            int cur_lit = toFormul(c->lits[i]);
            if (abs(cur_lit) > mx_var)
                mx_var = abs(cur_lit);
            lssolver->clause_lit[cls_ct][i].clause_num = cls_ct;
            lssolver->clause_lit[cls_ct][i].var_num = abs(cur_lit);
            if (cur_lit > 0) lssolver->clause_lit[cls_ct][i].sense = 1;
            else    lssolver->clause_lit[cls_ct][i].sense = 0;
            if (f->fixed && !lssolver->in_unit_queue[abs(cur_lit)]) {
                lssolver->in_unit_queue[abs(cur_lit)] = 1;
                lit fix;
                fix.var_num = abs(cur_lit);
                fix.sense = solver->values[LIT(idx)] == 1 ? 1 : 0;
                lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = fix;
            }
            
            lssolver->var_lit_count[lssolver->clause_lit[cls_ct][i].var_num]++;
        }
        lssolver->clause_lit[cls_ct][i].var_num=0; 
        lssolver->clause_lit[cls_ct][i].clause_num = -1;
        if(lssolver->clause_lit_count[cls_ct]==1){
            if (!lssolver->in_unit_queue[lssolver->clause_lit[cls_ct][0].var_num]) {
                lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = lssolver->clause_lit[cls_ct][0];
                lssolver->in_unit_queue[lssolver->clause_lit[cls_ct][0].var_num] = 1;
            }
            lssolver->clause_delete[cls_ct]=1;
        }
        if(lssolver->clause_lit_count[cls_ct] > lssolver->max_clause_len)
            lssolver->max_clause_len = lssolver->clause_lit_count[cls_ct];
        else if(lssolver->clause_lit_count[cls_ct] < lssolver->min_clause_len)
            lssolver->min_clause_len = lssolver->clause_lit_count[cls_ct];
        lssolver->formula_len += lssolver->clause_lit_count[cls_ct];

        cls_ct++;
    }

    litpair *binaries = BEGIN_STACK(binary_clause);
    for (int binary_ref = 0; binary_ref < binary_size; binary_ref++) {
        if (TERMINATED(11)) return;
        const litpair *litpair = binaries + binary_ref;
        lssolver->clause_lit_count[cls_ct] = 2;
        lssolver->clause_lit[cls_ct] = (lit*)malloc(sizeof(lit)*(lssolver->clause_lit_count[cls_ct]+1));    
        int i = 0;
        for (; i < 2; i++) {
            int idx = IDX (litpair->lits[i]);
            solver->ls_active[idx] = 1;
            flags *f = FLAGS (idx);
            int cur_lit = toFormul(litpair->lits[i]);
            if (abs(cur_lit) > mx_var)
                mx_var = abs(cur_lit);
            lssolver->clause_lit[cls_ct][i].clause_num = cls_ct;
            lssolver->clause_lit[cls_ct][i].var_num = abs(cur_lit);
            if (cur_lit > 0) lssolver->clause_lit[cls_ct][i].sense = 1;
            else    lssolver->clause_lit[cls_ct][i].sense = 0;
            lssolver->var_lit_count[lssolver->clause_lit[cls_ct][i].var_num]++;
            if (f->fixed && !lssolver->in_unit_queue[abs(cur_lit)]) {
                lssolver->in_unit_queue[abs(cur_lit)] = 1;
                lit fix;
                fix.var_num = abs(cur_lit);
                fix.sense = solver->values[LIT(idx)] == 1 ? 1 : 0;
                lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = fix;
            }
        }
        lssolver->clause_lit[cls_ct][i].var_num=0; 
        lssolver->clause_lit[cls_ct][i].clause_num = -1;
        if(lssolver->clause_lit_count[cls_ct]==1){
            if (!lssolver->in_unit_queue[lssolver->clause_lit[cls_ct][0].var_num]) {
                lssolver->unitclause_queue[lssolver->unitclause_queue_end_pointer++] = lssolver->clause_lit[cls_ct][0];
                lssolver->in_unit_queue[lssolver->clause_lit[cls_ct][0].var_num] = 1;
            }
            lssolver->clause_delete[cls_ct]=1;
        }
        if(lssolver->clause_lit_count[cls_ct] > lssolver->max_clause_len)
            lssolver->max_clause_len = lssolver->clause_lit_count[cls_ct];
        else if(lssolver->clause_lit_count[cls_ct] < lssolver->min_clause_len)
            lssolver->min_clause_len = lssolver->clause_lit_count[cls_ct];
        lssolver->formula_len += lssolver->clause_lit_count[cls_ct];

        cls_ct++;
    }
    if (TERMINATED(11)) return;
    update_after_build(lssolver);
    RELEASE_STACK (binary_clause);
}

void build_soln(kissat *solver, CCAnr *lssolver) {
    const flags *flags = solver->flags;   
    const phase *phases = solver->phases;
    const value initial_phase = GET_OPTION(phase) ? 1 : -1;
    for (all_variables(idx)) {
        if (solver->ls_active[idx] && !flags[idx].active) {
            // continue;
        }
        const phase *p = phases + idx;
        value value = 0;
        value = p->target;
        if (!value)
            value = p->saved;
        if (!value)
            value = initial_phase;
        if (value == 1)
            solver->top_trail_soln[idx] = 1;
        else
            solver->top_trail_soln[idx] = 0;
    }
}

int kissat_mab_ccanr (kissat *solver) {
    if (solver->ccanr_has_constructed) 
        reinit_CCAnr(solver->lssolver);
    else {
        init_CCAnr(solver->lssolver);
        solver->ccanr_has_constructed = true;
    } 
    load_ls_data(solver, solver->lssolver);
    if (TERMINATED(11)) return 0;
    build_soln(solver, solver->lssolver);
    settings(solver->lssolver, solver->top_trail_soln);
    if (TERMINATED(11)) return 0;
    int init_unsat_ct = solver->lssolver->unsat_stack_fill_pointer;
    solver->lssolver->bms = solver->ls_bms < solver->vars ? solver->ls_bms : solver->vars;
    int res = local_search(solver, solver->lssolver);
    if (TERMINATED(11)) return 0;
    int gap_inc = GET_OPTION(ccanr_gap_inc);
    for (int i = 0; i < VARS; i++) 
        solver->ls_mediation_soln[i] = solver->lssolver->best_soln[i + 1];
    if (solver->lssolver->best_cost <= solver->ls_best_unsat_num) {
        solver->restarts_gap -= gap_inc;
        if (solver->restarts_gap < abs(gap_inc))
            solver->restarts_gap = abs(gap_inc);
        solver->ls_best_unsat_num = solver->lssolver->best_cost;
        for (int i = 0; i < VARS; i++)
            solver->ls_best_soln[i] = solver->ls_mediation_soln[i];
    }
    else {
        solver->restarts_gap += gap_inc;
        if (solver->restarts_gap < abs(gap_inc))
            solver->restarts_gap = abs(gap_inc);
        solver->ls_bms = 0.1 * solver->ls_bms * GET_OPTION(ccanr_dynamic_bms);
        if (solver->ls_bms > VARS) solver->ls_bms = VARS;
        if (solver->ls_bms < 5) solver->ls_bms = 5;
    }
    if (res) {
        value *values = solver->values;
        for (int i = 0; i < VARS; i++) {
            if (!solver->ls_active[i]) continue;
            int ilit = LIT(i);
            values[ilit] = solver->ls_mediation_soln[i] ? 1 : -1;
            values[NOT(ilit)] = -values[ilit];
        }
    }

    if (solver->ls_call_num == 1) { //first ls
        phase *phases = solver->phases;
        const flags *flags = solver->flags;   
        for (all_variables(idx)) {
            if (solver->ls_active[idx] && flags[idx].active) {
                phase *p = phases + idx;  
                p->target = solver->ls_mediation_soln[idx] ? 1 : -1;
            }
        }
    }

    kissat_mab_reset_target_assigned(solver);
    solver->freeze_ls_restart_num = solver->restarts_gap;
    solver->ls_mems_num *= solver->ls_mems_inc;
    if (solver->ls_mems_num < solver->ls_mems_num_min)
        solver->ls_mems_num = solver->ls_mems_num_min;
    return res ? 10 : 0;
}