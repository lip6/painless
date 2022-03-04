#include "slime/utils/Ls.h"

using namespace LS;

int mersenne_rand(ls*, int);


ls::ls() {
    additional_len = 10;
    max_steps = 900 * 1000 * 1000;
    max_mems = 100 * 1000 * 1000;
    swt_threshold = 50;
    swt_p = 0.3;
    swt_q = 0.7;
    aspiration_active = true;
    mems = 0;
}

bool ls::make_space() {
    if (0 == num_vars || 0 == num_clauses) {
        cout << "The formula size is zero. You may forgot to read the formula." << endl;
        return false;
    }
    vars.resize(num_vars + additional_len);
    clauses.resize(num_clauses + additional_len);
    solution.resize(num_vars + additional_len);
    best_solution.resize(num_vars + additional_len);
    index_in_unsat_clauses.resize(num_clauses + additional_len);
    index_in_unsat_vars.resize(num_vars + additional_len);

    return true;
}

void ls::build_neighborhood() {
    int j;
    int v, c;
    vector<char> neighbor_flag(num_vars + additional_len);
    for (j = 0; j < neighbor_flag.size(); ++j) { neighbor_flag[j] = 0; }
    for (v = 1; v <= num_vars; ++v) {
        variable *vp = &(vars[v]);
        for (lit lv : vp->literals) {
            c = lv.clause_num;
            for (lit lc : clauses[c].literals) {
                if (!neighbor_flag[lc.var_num] && lc.var_num != v) {
                    neighbor_flag[lc.var_num] = 1;
                    vp->neighbor_var_nums.push_back(lc.var_num);
                }
            }
        }
        for (j = 0; j < vp->neighbor_var_nums.size(); ++j) { neighbor_flag[vp->neighbor_var_nums[j]] = 0; }
    }
}

bool ls::local_search(const vector<char> *init_solution) {
    bool result = false;
    int flipv;
    best_found_cost = num_clauses;
    conflict_ct = vector<int>(num_vars + 10, 0);
    initialize(init_solution);
    unsat_clauses.size();
    if (0 == unsat_clauses.size()) { return true; }
    for (step = 0; step < max_steps; step++) {
        if (mems > max_mems) {
            return result;
        }
        flipv = pick_var();
        flip(flipv);
        for (int var_idx:unsat_vars) ++conflict_ct[var_idx];
        if (unsat_clauses.size() < best_found_cost) {
            best_found_cost = unsat_clauses.size();
            for (int k = 0; k < num_vars + 1; ++k) {
                best_solution[k] = solution[k];
            }
        }
        if (unsat_clauses.empty()) {
            result = true;
            break;
        }
    }
    return result;
}

void ls::clear_prev_data() {
    unsat_clauses.clear();
    vector<int>().swap(unsat_clauses);
    ccd_vars.clear();
    vector<int>().swap(ccd_vars);
    unsat_vars.clear();
    vector<int>().swap(unsat_vars);
    for (int &item : index_in_unsat_clauses) item = 0;
    for (int &item : index_in_unsat_vars) item = 0;
}

void ls::initialize(const vector<char> *init_solution) {
    int v, c;
    clear_prev_data();
    if (!init_solution) {
        for (v = 1; v <= num_vars; v++) {
            // if(mersenne_rand(this,2)==1) solution[v] = 1;
            // else solution[v] = 0;
            solution[v] = 0;
        }
    } else {
        if (init_solution->size() != num_vars) {
            cout << "c Error: the init solution's size is not equal to the number of variables." << endl;
            exit(0);
        }
        for (v = 1; v <= num_vars; v++) {
            solution[v] = init_solution->at(v - 1);
            best_solution[v] = solution[v];
        }
    }
    for (v = 1; v <= num_vars; v++) { vars[v].unsat_appear = 0; }
    for (c = 0; c < num_clauses; c++) {
        clauses[c].sat_count = 0;
        clauses[c].sat_var = -1;
        clauses[c].weight = 1;

        for (lit l : clauses[c].literals) {
            if (solution[l.var_num] == l.sense) {
                clauses[c].sat_count++;
                clauses[c].sat_var = l.var_num;
            }
        }
        if (0 == clauses[c].sat_count) { unsat_a_clause(c); }
    }
    avg_clause_weight = 1;
    delta_total_clause_weight = 0;
    initialize_variable_datas();
}

void ls::initialize_variable_datas() {
    int v, c;
    variable *vp;
    //scores
    for (v = 1; v <= num_vars; v++) {
        vp = &(vars[v]);
        vp->score = 0;
        for (lit l : vp->literals) {
            c = l.clause_num;
            if (0 == clauses[c].sat_count) { vp->score += clauses[c].weight; }
            else if (1 == clauses[c].sat_count && l.sense == solution[l.var_num]) { vp->score -= clauses[c].weight; }
        }
    }
    //last flip step
    for (v = 1; v <= num_vars; v++) { vars[v].last_flip_step = 0; }
    //cc datas
    for (v = 1; v <= num_vars; v++) {
        vp = &(vars[v]);
        vp->cc_value = 1;
        if (vp->score > 0) //&&vars[v].cc_value==1
        {
            ccd_vars.push_back(v);
            vp->is_in_ccd_vars = 1;
        } else { vp->is_in_ccd_vars = 0; }
    }
    //the virtual var 0
    vp = &(vars[0]);
    vp->score = vp->cc_value = vp->is_in_ccd_vars = vp->last_flip_step = 0;
}

int ls::pick_var() {
    int i, k, c, v;
    int best_var = 0;

    if (ccd_vars.size() > 0) {
        mems += ccd_vars.size();
        best_var = ccd_vars[0];
        for (int v : ccd_vars) {
            if (vars[v].score > vars[best_var].score) { best_var = v; }
            else if (vars[v].score == vars[best_var].score && vars[v].last_flip_step < vars[best_var].last_flip_step) { best_var = v; }
        }
        return best_var;
    }

    //Aspriation Mode

    //----------------------------------------
    if (aspiration_active) {
        aspiration_score = avg_clause_weight;
        for (i = 0; i < unsat_vars.size(); ++i) {
            v = unsat_vars[i];
            if (vars[v].score > aspiration_score) {
                best_var = v;
                break;
            }
        }
        for (++i; i < unsat_vars.size(); ++i) {
            v = unsat_vars[i];
            if (vars[v].score > vars[best_var].score) best_var = v;
            else if (vars[v].score == vars[best_var].score && vars[v].last_flip_step < vars[best_var].last_flip_step) best_var = v;
        }
        if (best_var != 0) return best_var;
    }
    update_clause_weights();
    return best_var;
}

void ls::flip(int flipv) {
    solution[flipv] = 1 - solution[flipv];
    int org_flipv_score = vars[flipv].score;
    mems += vars[flipv].literals.size();
    for (lit l : vars[flipv].literals) {
        clause *cp = &(clauses[l.clause_num]);
        if (solution[flipv] == l.sense) {
            cp->sat_count++;
            if (1 == cp->sat_count) {
                sat_a_clause(l.clause_num);
                cp->sat_var = flipv;
                for (lit lc : cp->literals) { vars[lc.var_num].score -= cp->weight; }
            } else if (2 == cp->sat_count) { vars[cp->sat_var].score += cp->weight; }
        } else {
            cp->sat_count--;
            if (0 == cp->sat_count) {
                unsat_a_clause(l.clause_num);
                for (lit lc : cp->literals) { vars[lc.var_num].score += cp->weight; }
            } else if (1 == cp->sat_count) {
                for (lit lc : cp->literals) {
                    if (solution[lc.var_num] == lc.sense) {
                        vars[lc.var_num].score -= cp->weight;
                        cp->sat_var = lc.var_num;
                        break;
                    }
                }
            }
        }
    }
    vars[flipv].score = -org_flipv_score;
    vars[flipv].last_flip_step = step;
    //update cc_values
    update_cc_after_flip(flipv);
}

void ls::update_cc_after_flip(int flipv) {
    int index, v, last_item;
    variable *vp = &(vars[flipv]);
    vp->cc_value = 0;
    for (index = ccd_vars.size() - 1; index >= 0; index--) {
        v = ccd_vars[index];
        if (vars[v].score <= 0) {
            last_item = ccd_vars.back();
            ccd_vars.pop_back();
            ccd_vars[index] = last_item;
            mems++;
            vars[v].is_in_ccd_vars = 0;
        }
    }
    //update all flipv's neighbor's cc to be 1
    for (int v : vp->neighbor_var_nums) {
        vars[v].cc_value = 1;
        if (vars[v].score > 0 && !(vars[v].is_in_ccd_vars)) {
            ccd_vars.push_back(v);
            mems++;
            vars[v].is_in_ccd_vars = 1;
        }
    }
}

void ls::sat_a_clause(int the_clause) {
    int index, last_item;
    //use the position of the clause to store the last unsat clause in stack
    last_item = unsat_clauses.back();
    unsat_clauses.pop_back();
    index = index_in_unsat_clauses[the_clause];
    unsat_clauses[index] = last_item;
    index_in_unsat_clauses[last_item] = index;
    //update unsat_appear and unsat_vars
    for (lit l : clauses[the_clause].literals) {
        vars[l.var_num].unsat_appear--;
        if (0 == vars[l.var_num].unsat_appear) {
            last_item = unsat_vars.back();
            unsat_vars.pop_back();
            index = index_in_unsat_vars[l.var_num];
            unsat_vars[index] = last_item;
            index_in_unsat_vars[last_item] = index;
        }
    }
}

void ls::unsat_a_clause(int the_clause) {
    index_in_unsat_clauses[the_clause] = unsat_clauses.size();
    unsat_clauses.push_back(the_clause);
    //update unsat_appear and unsat_vars
    for (lit l : clauses[the_clause].literals) {
        vars[l.var_num].unsat_appear++;
        if (1 == vars[l.var_num].unsat_appear) {
            index_in_unsat_vars[l.var_num] = unsat_vars.size();
            unsat_vars.push_back(l.var_num);
        }
    }
}

void ls::update_clause_weights() {
    for (int c : unsat_clauses) { clauses[c].weight++; }
    mems += unsat_vars.size();
    for (int v : unsat_vars) {
        vars[v].score += vars[v].unsat_appear;
        if (vars[v].score > 0 && 1 == vars[v].cc_value && !(vars[v].is_in_ccd_vars)) {
            ccd_vars.push_back(v);
            vars[v].is_in_ccd_vars = 1;
        }
    }
    delta_total_clause_weight += unsat_clauses.size();
    if (delta_total_clause_weight >= num_clauses) {
        avg_clause_weight += 1;
        delta_total_clause_weight -= num_clauses;
        if (avg_clause_weight > swt_threshold) { smooth_clause_weights(); }
    }
}

void ls::smooth_clause_weights() {
    int v, c;
    for (v = 1; v <= num_vars; v++) { vars[v].score = 0; }
    int scale_avg = avg_clause_weight * swt_q;
    avg_clause_weight = 0;
    delta_total_clause_weight = 0;
    clause *cp;
    for (c = 0; c < num_clauses; ++c) {
        cp = &(clauses[c]);
        cp->weight = cp->weight * swt_p + scale_avg;
        if (cp->weight < 1) cp->weight = 1;
        delta_total_clause_weight += cp->weight;
        if (delta_total_clause_weight >= num_clauses) {
            avg_clause_weight += 1;
            delta_total_clause_weight -= num_clauses;
        }
        if (0 == cp->sat_count) {
            for (lit l : cp->literals) { vars[l.var_num].score += cp->weight; }
        } else if (1 == cp->sat_count) { vars[cp->sat_var].score -= cp->weight; }

    }
    //reset ccd_vars
    ccd_vars.clear();
    vector<int>().swap(ccd_vars);
    variable *vp;
    for (v = 1; v <= num_vars; v++) {
        vp = &(vars[v]);
        if (vp->score > 0 && 1 == vp->cc_value) {
            ccd_vars.push_back(v);
            vp->is_in_ccd_vars = 1;
        } else { vp->is_in_ccd_vars = 0; }
    }
}


int mersenne_rand(ls* lssolver,int bound){
	return mersenne_next(&(lssolver->randgen),bound);
}