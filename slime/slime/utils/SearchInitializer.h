#ifndef _INITIALIZER_H_
#define _INITIALIZER_H_

#include <vector>
#include <map>
#include "slime/core/SolverTypes.h"
#include "slime/utils/Ls.h"

using namespace std;

struct BetaDist {
    double a, b;
};

#define BayesianWeight(v) (max(parameters[v].a, parameters[v].b) / (parameters[v].a + parameters[v].b))


/** Polarity and Activity Initializer for MiniSat based SAT solvers
 * Current methods:
 * 1. Default (All-zero / All-False)
 * 2. Bayesian Moment Matching
 * 3. Jeroslow-Wang
 * 4. Random
 */

namespace SLIME {

class Solver;

class SearchInitializer {
    public:

        enum InitMethod {
            DEFAULT,
            BMM,
            JW,
            RANDOM,
        };

        SearchInitializer();
        SearchInitializer(Solver* s, int polarity, int activity, int initEpoch, int updateEpoch, char* solution = NULL);
        virtual ~SearchInitializer();
        void bayesian(char* solution = NULL);
        void update(vector<Lit>& clause, char* solution = NULL);
        void setEpochs(int epochs);
    private:
        template<typename T>
        void bayesian_update(T& c);
        void init_bayesian();

        void jeroslow_wang();
        
        Solver* solver;

        InitMethod polarity_init_method, activity_init_method;

        // BMM
        vector<BetaDist> parameters;
        int init_epochs;
        int update_epochs;
};

}
#endif
