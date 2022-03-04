#ifndef SLIME_LS_H
#define SLIME_LS_H

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "slime/utils/Mersenne.h"


using namespace std;
//---------------------


namespace LS {

//--------------------------
//functions in basis.h & basis.cpp
    struct lit {
        unsigned char sense: 1;    //is 1 for true literals, 0 for false literals.
        int clause_num: 31;        //clause num, begin with 0
        int var_num;            //variable num, begin with 1
        lit(int the_lit, int the_clause) {
            var_num = abs(the_lit);
            clause_num = the_clause;
            sense = the_lit > 0 ? 1 : 0;
        }

        struct lit &operator^=(const struct lit &l) {
            sense ^= l.sense;
            clause_num ^= l.clause_num;
            var_num ^= l.var_num;
            return *this;
        }

        bool operator==(const struct lit &l) const {
            return sense == l.sense && clause_num == l.clause_num && var_num == l.var_num;
        }

        bool operator!=(const struct lit &l) const {
            return !(*this == l);
        }
    };

    struct variable {
        vector<lit> literals;
        vector<int> neighbor_var_nums;
        long long score;
        long long last_flip_step;
        int unsat_appear;
        bool cc_value;
        bool is_in_ccd_vars;
    };
    struct clause {
        vector<lit> literals;
        int sat_count;
        int sat_var;
        long long weight;
    };

    class ls {
    public:
        ls();

        bool local_search(const vector<char> *init_solution = 0);
        int     seed;
        SLIME::mersenne_randgen randgen;

        vector<variable> vars;
        vector<clause> clauses;
        int num_vars;
        int num_clauses;
        int additional_len;
        //data structure used
        vector<int> unsat_clauses;
        vector<int> index_in_unsat_clauses;
        vector<int> unsat_vars;
        vector<int> index_in_unsat_vars;
        vector<int> ccd_vars;
        //solution information
        vector<char> solution;
        vector<char> best_solution;
        int best_found_cost;
        long long step;
        long long mems;
        long long max_mems;
        long long max_steps;
        //algorithmic parameters
        bool aspiration_active;
        int aspiration_score;
        //clause weighting
        int swt_threshold;
        float swt_p;//w=w*p+ave_w*q
        float swt_q;
        int avg_clause_weight;
        long long delta_total_clause_weight;

        //main functions
        void initialize(const vector<char> *init_solution = 0);

        void initialize_variable_datas();

        void clear_prev_data();

        int pick_var();

        void flip(int flipv);

        void update_cc_after_flip(int flipv);

        void update_clause_weights();

        void smooth_clause_weights();

        //funcitons for basic operations
        void sat_a_clause(int the_clause);

        void unsat_a_clause(int the_clause);

        //functions for buiding data structure
        bool make_space();

        void build_neighborhood();

        vector<int> conflict_ct;
    };

}
#endif
