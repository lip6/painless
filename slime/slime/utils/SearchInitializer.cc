#include "slime/utils/SearchInitializer.h"
#include <ctime>
#include "slime/core/Solver.h"

using namespace SLIME;

SearchInitializer::SearchInitializer()
{
}

SearchInitializer::SearchInitializer(Solver* s, int polarity, int activity, int initEpoch, int updateEpoch, char* solution)
{

    solver = s;

    polarity_init_method = (InitMethod)polarity;
    activity_init_method = (InitMethod)activity;

    init_epochs = initEpoch;
    update_epochs = updateEpoch;

    if ( polarity_init_method == BMM || activity_init_method == BMM ) {
        init_bayesian();
        bayesian(solution);
    }

    if ( polarity_init_method == JW || activity_init_method == JW )
        jeroslow_wang();

    if ( polarity_init_method == RANDOM )
        for(int v=0; v<solver->nVars(); v++) {
            // solver->setPolarity(v, rand() % 2 ? false : true);
            solver->setPolarity(v, mersenne_next(&(solver->randgen),2) ? false : true);
        }

    if ( activity_init_method == RANDOM ) {
        int rnd_max = 1000000;
        for(int v=0; v<solver->nVars(); v++)
            // solver->setActivity(v, rand() / RAND_MAX * 0.00001);
            solver->setActivity(v, mersenne_next(&(solver->randgen),rnd_max) / rnd_max * 0.00001);
    }
}

SearchInitializer::~SearchInitializer()
{
}

void SearchInitializer::update(vector<Lit>& clause, char* solution)
{
    for(int i=0; i<update_epochs; i++)
    {
        if (polarity_init_method == BMM)
            bayesian_update(clause);
    }

    for(int i=0; i<clause.size(); i++)
    {
        int v = var(clause[i]);
        if (solution == NULL) {
            solver->setPolarity(v, (parameters[v].a > parameters[v].b) ? false : true);
            solver->setActivity(v, BayesianWeight(v));
        } else {
            // seen_vars_since_last_ls_call.insert(v);
            solution[v] = (parameters[v].a > parameters[v].b) ? true : false;
        }
    }
}

template <typename T>
void SearchInitializer::bayesian_update(T& c)
{
    double coeff_product = 1.0;
    for( int j=0; j<c.size(); j++ )
    {
        Lit l = c[j];
        if ( solver->value(l) == l_True ) return; // this clause is already satisfied
        if ( solver->value(l) == l_False ) continue; // this literal is already falsified
        int v = var(l);
        int sgn = !sign(l);

        double coeff = (sgn ? parameters[v].b : parameters[v].a) / (parameters[v].a + parameters[v].b);
        coeff_product *= coeff;
    }
    double normalization_constant = 1 - coeff_product;
    for( int j=0; j<c.size(); j++ )
    {
        Lit l = c[j];
        if ( solver->value(l) == l_False ) continue; // this literal is already falsified
        int v = var(l);
        int sgn = !sign(l);

        double *p[2], up[2];
        p[0] = &parameters[v].a;
        p[1] = &parameters[v].b;

        // updated parameters
        up[0] = parameters[v].a + (!sgn);
        up[1] = parameters[v].b + (sgn);

        double sumP = *p[0] + *p[1];
        double sumUP = up[0] + up[1];

        for( int k=0; k<=1; k++ )
        {
            double moment1 = *p[k] / sumP - coeff_product * up[k] / sumUP;
            double moment2 = *p[k] * (*p[k] + 1) / ((sumP) * (sumP+1)) - coeff_product * up[k] * (up[k] + 1) / ((sumUP) * (sumUP+1));

            moment1 /= normalization_constant;
            moment2 /= normalization_constant;

            *p[k] = moment1 * (moment1 - moment2) / (moment2 - moment1*moment1);
        }
    }
}

void SearchInitializer::bayesian(char* solution)
{
    int n = solver->nVars();
    for( int k=0; k<init_epochs; k++ )
    {
        for( int i=0; i<solver->nClauses(); i++ )
        {
            Clause& c = solver->getClause(i);
            bayesian_update(c);
        }

/*        for( int i=0; i<learnts_core.size(); i++ )
        {
            Clause& c = ca[learnts_core[i]];
            bayesian_update(c);
        }

        for( int i=0; i<learnts_tier2.size(); i++ )
        {
            Clause& c = ca[learnts_tier2[i]];
            bayesian_update(c);
        }

        for( int i=0; i<learnts_local.size(); i++ )
        {
            Clause& c = ca[learnts_local[i]];
            bayesian_update(c);
        }*/
    }

    if ( polarity_init_method == BMM ) {
        for( int v=0; v<n; v++ ) {
            if (solution == NULL) {
                if (solver->value(v) == l_Undef){
                    solver->setPolarity(v, (parameters[v].a > parameters[v].b) ? false : true);
                }
            } else  if (solver->value(v) == l_Undef){
                // solution[v] = (parameters[v].a > parameters[v].b) ? false : true;
                solution[v] = (parameters[v].a > parameters[v].b) ? true : false;
            } else {
                solution[v] = solver->value(v) == l_True ? true: false;
            }
        }
    }

    if ( activity_init_method == BMM ) {
        if (solution == NULL) {
            for( int v=0; v<n; v++ ) {
                if (solver->value(v) == l_Undef) {
                    solver->setActivity(v, BayesianWeight(v));
                }
            }
        }
    }
}

void SearchInitializer::init_bayesian()
{
    int n = solver->nVars();
    parameters.resize(n);
    unsigned int rnd_max = 1000000;
    for( int i=0; i<n; i++ )
    {
        parameters[i].a = (double)mersenne_next(&(solver->randgen),rnd_max) / rnd_max + 10;
        parameters[i].b = (double)mersenne_next(&(solver->randgen),rnd_max) / rnd_max + 10;
    }
}

void SearchInitializer::jeroslow_wang()
{
    int n = solver->nVars();
    vector<double> cnt(2 * n, 0.0);

    for( int i=0; i<solver->nClauses(); i++ )
    {
        Clause& c = solver->getClause(i);
        if ( c.size() >= 50 ) continue;
        double sc = pow(2, -c.size());
        for( int j=0; j<c.size(); j++ )
        {
            Lit q = c[j];
            if ( sign(q) )
                cnt[var(q) + n] += sc;
            else
                cnt[var(q)] += sc;
        }
    }

    if ( polarity_init_method == JW )
    {
        for( int v=0; v<n; v++ )
            solver->setPolarity(v, (cnt[v] > cnt[v+n]) ? false : true);
    }

    if ( activity_init_method == JW )
    {
        int m = solver->nClauses();
        if ( m == 0 ) m = 1;
        for( int v=0; v<n; v++ )
            solver->setActivity(v, (cnt[v] + cnt[v+n]) / m);
    }
}

void SearchInitializer::setEpochs(int epochs) {
    init_epochs = epochs;
}