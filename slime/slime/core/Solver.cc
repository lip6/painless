/***************************************************************************************
SLIME -- Copyright (c) 2021, Oscar Riveros, oscar.riveros@peqnp.science,
Santiago, Chile. https://github.com/maxtuno/SLIME

DurianSat -- Copyright (c) 2020, Arijit Shaw, Kuldeep S. Meel

SLIME -- Copyright (c) 2019, Oscar Riveros, oscar.riveros@peqnp.science,
Santiago, Chile. https://maxtuno.github.io/slime-sat-solver

Maple_LCM_Dist_Chrono -- Copyright (c) 2018, Vadim Ryvchin, Alexander Nadel

GlucoseNbSAT -- Copyright (c) 2016,Chu Min LI,Mao Luo and Fan Xiao
                           Huazhong University of science and technology, China
                           MIS, Univ. Picardie Jules Verne, France

MapleSAT -- Copyright (c) 2016, Jia Hui Liang, Vijay Ganesh

MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
           Copyright (c) 2007-2010  Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <algorithm>
#include <cmath>

#include "slime/core/Solver.h"
#include "slime/utils/Ls.h"
#include "slime/mtl/Sort.h"
#include "slime/utils/System.h"

using namespace SLIME;

#ifdef MASSIVE

#include <mpi.h>

#endif

#ifdef BIN_DRUP
int Solver::buf_len = 0;
unsigned char Solver::drup_buf[2 * 1024 * 1024];
unsigned char *Solver::buf_ptr = drup_buf;
#endif

//=================================================================================================
// Options:

static const char *_cat = "CORE";

static DoubleOption opt_step_size(_cat, "step-size", "Initial step size", 0.40, DoubleRange(0, false, 1, false));
static DoubleOption opt_step_size_dec(_cat, "step-size-dec", "Step size decrement", 0.000001, DoubleRange(0, false, 1, false));
static DoubleOption opt_min_step_size(_cat, "min-step-size", "Minimal step size", 0.06, DoubleRange(0, false, 1, false));
static DoubleOption opt_var_decay(_cat, "var-decay", "The variable activity decay factor", 0.80, DoubleRange(0, false, 1, false));
static DoubleOption opt_clause_decay(_cat, "cla-decay", "The clause activity decay factor", 0.999, DoubleRange(0, false, 1, false));
static IntOption opt_ccmin_mode(_cat, "ccmin-mode", "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2));
static IntOption opt_phase_saving(_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
static IntOption opt_restart_first(_cat, "rfirst", "The base restart interval", 100, IntRange(1, INT32_MAX));
static DoubleOption opt_restart_inc(_cat, "rinc", "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
static DoubleOption opt_garbage_frac(_cat, "gc-frac", "The fraction of wasted memory allowed before a garbage collection is triggered", 0.20, DoubleRange(0, false, HUGE_VAL, false));
static IntOption opt_chrono(_cat, "chrono", "Controls if to perform chrono backtrack", 100, IntRange(-1, INT32_MAX));
static IntOption opt_conf_to_chrono(_cat, "confl-to-chrono", "Controls number of conflicts to perform chrono backtrack", 4000, IntRange(-1, INT32_MAX));

static IntOption opt_max_lbd_dup("DUP-LEARNTS", "lbd-limit", "specifies the maximum lbd of learnts to be screened for duplicates.", 14, IntRange(0, INT32_MAX));
static IntOption opt_min_dupl_app("DUP-LEARNTS", "min-dup-app", "specifies the minimum number of learnts to be included into db.", 2, IntRange(2, INT32_MAX));
static IntOption opt_VSIDS_props_limit("DUP-LEARNTS", "VSIDS-lim", "specifies the number of propagations after which the solver switches between LRB and VSIDS(in millions).", 30, IntRange(1, INT32_MAX));

static DoubleOption opt_lsids_erase_weight("LSIDS", "lsids-erase-weight", "Weight for LSIDS bump.", 2.0, DoubleRange(0, true, 5, true));

static BoolOption opt_boost("BOOST", "boost", "Specifies the use BOOST algorithm.", true);
static BoolOption opt_boost_alternate("BOOST", "boost-alternate", "Specifies the use BOOST alternate algorithm.", true);
static BoolOption opt_hess("HESS", "hess", "Specifies the use HESS algorithm.", false);
static IntOption opt_hess_order("HESS", "hess-order", "Specifies the order of HESS algorithm.", 1, IntRange(1, 2));
static BoolOption opt_massive("MASSIVE", "massive", "Specifies the use for MASSIVE execution.", false);

static IntOption     opt_polarity_init_method     (_cat, "pol-init", "Polarity initialization method (0=Always-False, 1=Bayesian-Moment-Matching, 2=Jeroslow-Wang, 3=Random)", 1, IntRange(0, 3));
static IntOption     opt_activity_init_method     (_cat, "act-init", "Activity initialization method (0=All-zero,     1=Bayesian-Moment-Matching, 2=Jeroslow-Wang, 3=Random)", 0, IntRange(0, 3));
static IntOption     opt_init_epochs              (_cat, "init-epochs", "Initial number of Epochs for learning polarity/activity weights", 10, IntRange(0, 1000));
static IntOption     opt_update_epochs            (_cat, "update-epochs", "Number of Epochs for updating polarity/activity weights using a conflict clause", 1, IntRange(0, 1000));
static BoolOption    opt_sls                      (_cat, "call-local-search", "whether CCANR is called", true);

//=================================================================================================
// Constructor/Destructor:

Solver::Solver()
        :

// Parameters (user settable):
//
        drup_file(NULL), step_size(opt_step_size), step_size_dec(opt_step_size_dec), min_step_size(opt_min_step_size), timer(5000), var_decay(opt_var_decay), clause_decay(opt_clause_decay), VSIDS(false), ccmin_mode(opt_ccmin_mode), phase_saving(opt_phase_saving), garbage_frac(opt_garbage_frac), restart_first(opt_restart_first), restart_inc(opt_restart_inc),
        min_number_of_learnts_copies(opt_min_dupl_app), max_lbd_dup(opt_max_lbd_dup), VSIDS_props_limit(opt_VSIDS_props_limit * 1000000), boost(opt_boost), boost_alternate(opt_boost_alternate), hess(opt_hess), hess_order(opt_hess_order), massive(opt_massive)
        // Parameters (the rest):
        //
        ,
        learntsize_factor((double) 1 / (double) 3), learntsize_inc(1.1)

        // Parameters (experimental):
        //
        ,
        learntsize_adjust_start_confl(100), learntsize_adjust_inc(1.5), lsids_erase_bump_weight(opt_lsids_erase_weight)
        // Statistics: (formerly in 'SolverStats')
        //
        ,
        starts(0), decisions(0), propagations(0), conflicts(0), conflicts_VSIDS(0), dec_vars(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0), chrono_backtrack(0), non_chrono_backtrack(0),
        ok(true), cla_inc(1), var_inc(1), lit_inc(1), watches_bin(WatcherDeleted(ca)), watches(WatcherDeleted(ca)), qhead(0), simpDB_assigns(-1), simpDB_props(0), order_heap_CHB(VarOrderLt(activity_CHB)), order_heap_VSIDS(VarOrderLt(activity_VSIDS)), remove_satisfied(true),
        core_lbd_cut(2), global_lbd_sum(0), lbd_queue(50), next_T2_reduce(10000), next_L_reduce(15000), confl_to_chrono(opt_conf_to_chrono), chrono(opt_chrono),
        counter(0)

        // simplfiy
        ,
        nbSimplifyAll(0)

        // simplifyAll adjust occasion
        ,
        curSimplify(1), nbconfbeforesimplify(1000), incSimplify(1000),
        my_var_decay(0.6), DISTANCE(true), var_iLevel_inc(1), order_heap_distance(VarOrderLt(activity_distance))
        
        , polarity_init_method(opt_polarity_init_method)
        , activity_init_method(opt_activity_init_method)
        , init_epochs(opt_init_epochs)
        , update_epochs(opt_update_epochs)
        , sls(opt_sls)
        , flips(0)
        , flip_ratio(0.1)
        , cbkImportClause(NULL), cbkImportUnit(NULL), cbkExportClause(NULL) {}

Solver::~Solver() {}

// simplify All
//
CRef Solver::simplePropagate() {
    CRef confl = CRef_Undef;
    int num_props = 0;
    watches.cleanAll();
    watches_bin.cleanAll();
    while (qhead < trail.size()) {
        Lit p = trail[qhead++]; // 'p' is enqueued fact to propagate.
        vec<Watcher> &ws = watches[p];
        Watcher *i, *j, *end;
        num_props++;

        // First, Propagate binary clauses
        vec<Watcher> &wbin = watches_bin[p];

        for (int k = 0; k < wbin.size(); k++) {

            Lit imp = wbin[k].blocker;

            if (value(imp) == l_False) {
                return wbin[k].cref;
            }

            if (value(imp) == l_Undef) {
                simpleUncheckEnqueue(imp, wbin[k].cref);
            }
        }
        for (i = j = (Watcher *) ws, end = i + ws.size(); i != end;) {
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True) {
                *j++ = *i++;
                continue;
            }

            // Make sure the false literal is data[1]:
            CRef cr = i->cref;
            Clause &c = ca[cr];
            Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            //  i++;

            // If 0th watch is true, then clause is already satisfied.
            // However, 0th watch is not the blocker, make it blocker using a new watcher w
            // why not simply do i->blocker=first in this case?
            Lit first = c[0];
            //  Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True) {
                i->blocker = first;
                *j++ = *i++;
                continue;
            } else { // ----------------- DEFAULT  MODE (NOT INCREMENTAL)
                for (int k = 2; k < c.size(); k++) {

                    if (value(c[k]) != l_False) {
                        // watcher i is abandonned using i++, because cr watches now ~c[k] instead of p
                        // the blocker is first in the watcher. However,
                        // the blocker in the corresponding watcher in ~first is not c[1]
                        Watcher w = Watcher(cr, first);
                        i++;
                        c[1] = c[k];
                        c[k] = false_lit;
                        watches[~c[1]].push(w);
                        goto NextClause;
                    }
                }
            }

            // Did not find watch -- clause is unit under assignment:
            i->blocker = first;
            *j++ = *i++;
            if (value(first) == l_False) {
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            } else {
                simpleUncheckEnqueue(first, cr);
            }
            NextClause:;
        }
        ws.shrink(i - j);
    }

    return confl;
}

void Solver::simpleUncheckEnqueue(Lit p, CRef from) {
    assert(value(p) == l_Undef);
    assigns[var(p)] = lbool(!sign(p)); // this makes a lbool object whose value is sign(p)
    vardata[var(p)].reason = from;
    trail.push_(p);
}

void Solver::cancelUntilTrailRecord() {
    for (int c = trail.size() - 1; c >= trailRecord; c--) {
        Var x = var(trail[c]);
        assigns[x] = l_Undef;
    }
    qhead = trailRecord;
    trail.shrink(trail.size() - trailRecord);
}

bool Solver::removed(CRef cr) { return ca[cr].mark() == 1; }

void Solver::simpleAnalyze(CRef confl, vec<Lit> &out_learnt, vec<CRef> &reason_clause, bool True_confl) {
    int pathC = 0;
    Lit p = lit_Undef;
    int index = trail.size() - 1;

    do {
        if (confl != CRef_Undef) {
            reason_clause.push(confl);
            Clause &c = ca[confl];
            // Special case for binary clauses
            // The first one has to be SAT
            if (p != lit_Undef && c.size() == 2 && value(c[0]) == l_False) {

                assert(value(c[1]) == l_True);
                Lit tmp = c[0];
                c[0] = c[1], c[1] = tmp;
            }
            // if True_confl==true, then choose p begin with the 1th index of c;
            for (int j = (p == lit_Undef && True_confl == false) ? 0 : 1; j < c.size(); j++) {
                Lit q = c[j];
                if (!seen[var(q)]) {
                    seen[var(q)] = 1;
                    pathC++;
                }
            }
        } else if (confl == CRef_Undef) {
            out_learnt.push(~p);
        }
        // if not break, while() will come to the index of trail blow 0, and fatal error occur;
        if (pathC == 0)
            break;
        // Select next clause to look at:
        while (!seen[var(trail[index--])]);
        // if the reason cr from the 0-level assigned var, we must break avoid move forth further;
        // but attention that maybe seen[x]=1 and never be clear. However makes no matter;
        if (trailRecord > index + 1)
            break;
        p = trail[index + 1];
        confl = reason(var(p));
        seen[var(p)] = 0;
        pathC--;

    } while (pathC >= 0);
}

void Solver::simplifyLearnt(Clause &c) {
    original_length_record += c.size();

    trailRecord = trail.size(); // record the start pointer

    vec<Lit> falseLit;
    falseLit.clear();

    bool True_confl = false;
    int beforeSize, afterSize;
    beforeSize = c.size();
    int i, j;
    CRef confl;

    for (i = 0, j = 0; i < c.size(); i++) {
        if (value(c[i]) == l_Undef) {
            simpleUncheckEnqueue(~c[i]);
            c[j++] = c[i];
            confl = simplePropagate();
            if (confl != CRef_Undef) {
                break;
            }
        } else {
            if (value(c[i]) == l_True) {
                c[j++] = c[i];
                True_confl = true;
                confl = reason(var(c[i]));
                break;
            } else {
                falseLit.push(c[i]);
            }
        }
    }
    c.shrink(c.size() - j);
    afterSize = c.size();

    if (confl != CRef_Undef || True_confl) {
        simp_learnt_clause.clear();
        simp_reason_clause.clear();
        if (True_confl) {
            simp_learnt_clause.push(c.last());
        }
        simpleAnalyze(confl, simp_learnt_clause, simp_reason_clause, True_confl);

        if (simp_learnt_clause.size() < c.size()) {
            for (i = 0; i < simp_learnt_clause.size(); i++) {
                c[i] = simp_learnt_clause[i];
            }
            c.shrink(c.size() - i);
        }
    }

    cancelUntilTrailRecord();

    simplified_length_record += c.size();
}

bool Solver::simplifyLearnt_core() {
    int beforeSize, afterSize;
    int learnts_core_size_before = learnts_core.size();

    int ci, cj, li, lj;
    bool sat, false_lit;
    unsigned int nblevels;

    int nbSimplified = 0;
    int nbSimplifing = 0;

    for (ci = 0, cj = 0; ci < learnts_core.size(); ci++) {
        CRef cr = learnts_core[ci];
        Clause &c = ca[cr];

        if (removed(cr))
            continue;
        else if (c.simplified()) {
            learnts_core[cj++] = learnts_core[ci];
            nbSimplified++;
        } else {
            int saved_size = c.size();
            nbSimplifing++;
            sat = false_lit = false;
            for (int i = 0; i < c.size(); i++) {
                if (value(c[i]) == l_True) {
                    sat = true;
                    break;
                } else if (value(c[i]) == l_False) {
                    false_lit = true;
                }
            }
            if (sat) {
                removeClause(cr);
            } else {
                detachClause(cr, true);

                if (false_lit) {
                    for (li = lj = 0; li < c.size(); li++) {
                        if (value(c[li]) != l_False) {
                            c[lj++] = c[li];
                        }
                    }
                    c.shrink(li - lj);
                }

                beforeSize = c.size();
                assert(c.size() > 1);
                // simplify a learnt clause c
                simplifyLearnt(c);
                assert(c.size() > 0);
                afterSize = c.size();

                if (drup_file && saved_size != c.size()) {
#ifdef BIN_DRUP
                    binDRUP('a', c, drup_file);
                    //                    binDRUP('d', add_oc, drup_file);
#else
                    for (int i = 0; i < c.size(); i++)
                        fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
                    fprintf(drup_file, "0\n");

                    //                    fprintf(drup_file, "d ");
                    //                    for (int i = 0; i < add_oc.size(); i++)
                    //                        fprintf(drup_file, "%i ", (var(add_oc[i]) + 1) * (-2 * sign(add_oc[i]) + 1));
                    //                    fprintf(drup_file, "0\n");
#endif
                }

                if (c.size() == 1) {
                    // when unit clause occur, enqueue and propagate
                    uncheckedEnqueue(c[0]);
                    if (propagate() != CRef_Undef) {
                        ok = false;
                        return false;
                    }
                    // delete the clause memory in logic
                    c.mark(1);
                    ca.free(cr);
                } else {
                    attachClause(cr);
                    learnts_core[cj++] = learnts_core[ci];

                    nblevels = computeLBD(c);
                    if (nblevels < c.lbd()) {
                        c.set_lbd(nblevels);
                    }

                    c.setSimplified(true);
                }
            }
        }
    }
    learnts_core.shrink(ci - cj);
    return true;
}

int Solver::is_duplicate(std::vector<uint32_t> &c) {
    dupl_db_size++;
    int res = 0;

    int sz = c.size();
    std::vector<uint32_t> tmp(c);
    sort(tmp.begin(), tmp.end());

    uint64_t hash = 0;

    for (int i = 0; i < sz; i++) {
        hash ^= tmp[i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

    int32_t head = tmp[0];
    auto it0 = ht.find(head);
    if (it0 != ht.end()) {
        auto it1 = ht[head].find(sz);
        if (it1 != ht[head].end()) {
            auto it2 = ht[head][sz].find(hash);
            if (it2 != ht[head][sz].end()) {
                it2->second++;
                res = it2->second;
            } else {
                ht[head][sz][hash] = 1;
            }
        } else {
            ht[head][sz][hash] = 1;
        }
    } else {
        ht[head][sz][hash] = 1;
    }
    return res;
}

bool Solver::simplifyLearnt_tier2() {
    int beforeSize, afterSize;
    int learnts_tier2_size_before = learnts_tier2.size();

    int ci, cj, li, lj;
    bool sat, false_lit;
    unsigned int nblevels;

    int nbSimplified = 0;
    int nbSimplifing = 0;

    for (ci = 0, cj = 0; ci < learnts_tier2.size(); ci++) {
        CRef cr = learnts_tier2[ci];
        Clause &c = ca[cr];

        if (removed(cr))
            continue;
        else if (c.simplified()) {
            learnts_tier2[cj++] = learnts_tier2[ci];
            nbSimplified++;
        } else {
            int saved_size = c.size();
            nbSimplifing++;
            sat = false_lit = false;
            for (int i = 0; i < c.size(); i++) {
                if (value(c[i]) == l_True) {
                    sat = true;
                    break;
                } else if (value(c[i]) == l_False) {
                    false_lit = true;
                }
            }
            if (sat) {
                removeClause(cr);
            } else {
                detachClause(cr, true);

                if (false_lit) {
                    for (li = lj = 0; li < c.size(); li++) {
                        if (value(c[li]) != l_False) {
                            c[lj++] = c[li];
                        }
                    }
                    c.shrink(li - lj);
                }

                beforeSize = c.size();
                assert(c.size() > 1);
                // simplify a learnt clause c
                simplifyLearnt(c);
                assert(c.size() > 0);
                afterSize = c.size();

                if (drup_file && saved_size != c.size()) {

#ifdef BIN_DRUP
                    binDRUP('a', c, drup_file);
                    //                    binDRUP('d', add_oc, drup_file);
#else
                    for (int i = 0; i < c.size(); i++)
                        fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
                    fprintf(drup_file, "0\n");

                    //                    fprintf(drup_file, "d ");
                    //                    for (int i = 0; i < add_oc.size(); i++)
                    //                        fprintf(drup_file, "%i ", (var(add_oc[i]) + 1) * (-2 * sign(add_oc[i]) + 1));
                    //                    fprintf(drup_file, "0\n");
#endif
                }

                if (c.size() == 1) {
                    // when unit clause occur, enqueue and propagate
                    uncheckedEnqueue(c[0]);
                    if (propagate() != CRef_Undef) {
                        ok = false;
                        return false;
                    }
                    // delete the clause memory in logic
                    c.mark(1);
                    ca.free(cr);
                } else {

                    nblevels = computeLBD(c);
                    if (nblevels < c.lbd()) {
                        c.set_lbd(nblevels);
                    }
                    // duplicate learnts
                    int id = 0;

                    std::vector<uint32_t> tmp;
                    for (int i = 0; i < c.size(); i++)
                        tmp.push_back(c[i].x);
                    id = is_duplicate(tmp);

                    // duplicate learnts

                    if (id < min_number_of_learnts_copies + 2) {
                        attachClause(cr);
                        learnts_tier2[cj++] = learnts_tier2[ci];
                        if (id == min_number_of_learnts_copies + 1) {
                            duplicates_added_minimization++;
                        }
                        if ((c.lbd() <= core_lbd_cut) || (id == min_number_of_learnts_copies + 1)) {
                            cj--;
                            learnts_core.push(cr);
                            c.mark(CORE);
                        }

                        c.setSimplified(true);
                    }
                }
            }
        }
    }
    learnts_tier2.shrink(ci - cj);

    return true;
}

bool Solver::simplifyAll() {
    simplified_length_record = original_length_record = 0;

    if (!ok || propagate() != CRef_Undef)
        return ok = false;

    if (!simplifyLearnt_core())
        return ok = false;
    if (!simplifyLearnt_tier2())
        return ok = false;

    checkGarbage();

    return true;
}
//=================================================================================================
// Minor methods:

// Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(bool sign, bool dvar) {
    int v = nVars();
    watches_bin.init(mkLit(v, false));
    watches_bin.init(mkLit(v, true));
    watches.init(mkLit(v, false));
    watches.init(mkLit(v, true));
    assigns.push(l_Undef);
    vardata.push(mkVarData(CRef_Undef, 0));
    activity_CHB.push(0);
    activity_VSIDS.push(0);
    activity_VSIDS.push(0);

    picked.push(0);
    conflicted.push(0);
    almost_conflicted.push(0);
#ifdef ANTI_EXPLORATION
    canceled.push(0);
#endif

    seen.push(0);
    seen2.push(0);
    polarity.push(sign);
    decision.push();
    trail.capacity(v + 1);
    setDecisionVar(v, dvar);

    activity_distance.push(0);
    var_iLevel.push(0);
    var_iLevel_tmp.push(0);
    pathCs.push(0);
    return v;
}

bool Solver::addClause_(vec<Lit> &ps) {
    assert(decisionLevel() == 0);
    if (!ok)
        return false;

    // Check if clause is satisfied and remove false/duplicate literals:
    sort(ps);
    Lit p;
    int i, j;

    if (drup_file) {
        add_oc.clear();
        for (int i = 0; i < ps.size(); i++)
            add_oc.push(ps[i]);
    }

    for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        if (value(ps[i]) == l_True || ps[i] == ~p)
            return true;
        else if (value(ps[i]) != l_False && ps[i] != p)
            ps[j++] = p = ps[i];
    ps.shrink(i - j);

    if (drup_file && i != j) {
#ifdef BIN_DRUP
        binDRUP('a', ps, drup_file);
        binDRUP('d', add_oc, drup_file);
#else
        for (int i = 0; i < ps.size(); i++)
            fprintf(drup_file, "%i ", (var(ps[i]) + 1) * (-2 * sign(ps[i]) + 1));
        fprintf(drup_file, "0\n");

        fprintf(drup_file, "d ");
        for (int i = 0; i < add_oc.size(); i++)
            fprintf(drup_file, "%i ", (var(add_oc[i]) + 1) * (-2 * sign(add_oc[i]) + 1));
        fprintf(drup_file, "0\n");
#endif
    }

    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1) {
        uncheckedEnqueue(ps[0]);
        if (render) {
            return ok = (propagate() == CRef_Undef);
        }
    } else {
        CRef cr = ca.alloc(ps, false);
        clauses.push(cr);
        attachClause(cr);
    }

    return true;
}

void Solver::attachClause(CRef cr) {
    const Clause &c = ca[cr];
    assert(c.size() > 1);
    OccLists<Lit, vec<Watcher>, WatcherDeleted> &ws = c.size() == 2 ? watches_bin : watches;
    ws[~c[0]].push(Watcher(cr, c[1]));
    ws[~c[1]].push(Watcher(cr, c[0]));
    if (c.learnt())
        learnts_literals += c.size();
    else
        clauses_literals += c.size();
}

void Solver::detachClause(CRef cr, bool strict) {
    const Clause &c = ca[cr];
    assert(c.size() > 1);
    OccLists<Lit, vec<Watcher>, WatcherDeleted> &ws = c.size() == 2 ? watches_bin : watches;

    if (strict) {
        remove(ws[~c[0]], Watcher(cr, c[1]));
        remove(ws[~c[1]], Watcher(cr, c[0]));
    } else {
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        ws.smudge(~c[0]);
        ws.smudge(~c[1]);
    }

    if (c.learnt())
        learnts_literals -= c.size();
    else
        clauses_literals -= c.size();
}

void Solver::removeClause(CRef cr) {
    Clause &c = ca[cr];

    if (drup_file) {
        if (c.mark() != 1) {
#ifdef BIN_DRUP
            binDRUP('d', c, drup_file);
#else
            fprintf(drup_file, "d ");
            for (int i = 0; i < c.size(); i++)
                fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
            fprintf(drup_file, "0\n");
#endif
        } else
            printf("c Bug. I don't expect this to happen.\n");
    }

    detachClause(cr);
    // Don't leave pointers to free'd memory!
    if (locked(c)) {
        Lit implied = c.size() != 2 ? c[0] : (value(c[0]) == l_True ? c[0] : c[1]);
        vardata[var(implied)].reason = CRef_Undef;
    }
    c.mark(1);
    ca.free(cr);
}

bool Solver::satisfied(const Clause &c) const {
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True)
            return true;
    return false;
}

// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int bLevel) {

    if (decisionLevel() > bLevel) {
#ifdef PRINT_OUT
        std::cout << "bt " << bLevel << "\n";
#endif
        add_tmp.clear();
        for (int c = trail.size() - 1; c >= trail_lim[bLevel]; c--) {
            Var x = var(trail[c]);

            if (level(x) <= bLevel) {
                add_tmp.push(trail[c]);
            } else {
                if (!VSIDS) {
                    uint32_t age = conflicts - picked[x];
                    if (age > 0) {
                        double adjusted_reward = ((double) (conflicted[x] + almost_conflicted[x])) / ((double) age);
                        double old_activity = activity_CHB[x];
                        activity_CHB[x] = step_size * adjusted_reward + ((1 - step_size) * old_activity);
                        if (order_heap_CHB.inHeap(x)) {
                            if (activity_CHB[x] > old_activity)
                                order_heap_CHB.decrease(x);
                            else
                                order_heap_CHB.increase(x);
                        }
                    }
#ifdef ANTI_EXPLORATION
                    canceled[x] = conflicts;
#endif
                }

                assigns[x] = l_Undef;
#ifdef PRINT_OUT
                std::cout << "undo " << x << "\n";
#endif
                if (phase_saving > 1 || (phase_saving == 1) && c > trail_lim.last()) {
                    polarity[x] = sign(trail[c]);
                    litBumpActivity(mkLit(x, !polarity[x]), lsids_erase_bump_weight);
                }
                insertVarOrder(x);
            }
        }
        qhead = trail_lim[bLevel];
        trail.shrink(trail.size() - trail_lim[bLevel]);
        trail_lim.shrink(trail_lim.size() - bLevel);
        for (int nLitId = add_tmp.size() - 1; nLitId >= 0; --nLitId) {
            trail.push_(add_tmp[nLitId]);
        }

        add_tmp.clear();
    }
}

//=================================================================================================
// Major methods:

Lit Solver::pickBranchLit() {
    Var next = var_Undef;
    Heap<VarOrderLt> &order_heap = DISTANCE ? order_heap_distance : ((!VSIDS) ? order_heap_CHB : order_heap_VSIDS);

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || !decision[next])
        if (order_heap.empty())
            return lit_Undef;
        else {
#ifdef ANTI_EXPLORATION
            if (!VSIDS) {
                Var v = order_heap_CHB[0];
                uint32_t age = conflicts - canceled[v];
                while (age > 0) {
                    double decay = pow(0.95, age);
                    activity_CHB[v] *= decay;
                    if (order_heap_CHB.inHeap(v))
                        order_heap_CHB.increase(v);
                    canceled[v] = conflicts;
                    v = order_heap_CHB[0];
                    age = conflicts - canceled[v];
                }
            }
#endif
            next = order_heap.removeMin();
        }

    if (!VSIDS) {
        if (boost) {
            polarity[trail.size()] = !polarity[trail.size()];
        }
        if (trail.size() > global) {
            global = trail.size();
            if (global > cursor) {
                cursor = global;
                if (log) {
#ifdef MASSIVE
                    printf("\rc %.2f%% [%i]                           ", 100.0 * (nVars() - global) / nVars(), rank);
#else
                    // printf("\rc %.2f%%                                ", 100.0 * (nVars() - global) / nVars());
#endif
                    fflush(stdout);
                }
            }
        } else if (trail.size() < global) {
            if (boost) {
                polarity[trail.size()] = !polarity[trail.size()];
            }
        }
    }

    return mkLit(next, polarity[next]);
}

inline Solver::ConflictData Solver::FindConflictLevel(CRef cind) {
    ConflictData data;
    Clause &conflCls = ca[cind];
    data.nHighestLevel = level(var(conflCls[0]));
    if (data.nHighestLevel == decisionLevel() && level(var(conflCls[1])) == decisionLevel()) {
        return data;
    }

    int highestId = 0;
    data.bOnlyOneLitFromHighest = true;
    // find the largest decision level in the clause
    for (int nLitId = 1; nLitId < conflCls.size(); ++nLitId) {
        int nLevel = level(var(conflCls[nLitId]));
        if (nLevel > data.nHighestLevel) {
            highestId = nLitId;
            data.nHighestLevel = nLevel;
            data.bOnlyOneLitFromHighest = true;
        } else if (nLevel == data.nHighestLevel && data.bOnlyOneLitFromHighest == true) {
            data.bOnlyOneLitFromHighest = false;
        }
    }

    if (highestId != 0) {
        std::swap(conflCls[0], conflCls[highestId]);
        if (highestId > 1) {
            OccLists<Lit, vec<Watcher>, WatcherDeleted> &ws = conflCls.size() == 2 ? watches_bin : watches;
            remove(ws[~conflCls[highestId]], Watcher(cind, conflCls[1]));
            ws[~conflCls[0]].push(Watcher(cind, conflCls[1]));
        }
    }

    return data;
}

/*_________________________________________________________________________________________________
|
|  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
|
|  Description:
|    Analyze conflict and produce a reason clause.
|
|    Pre-conditions:
|      * 'out_learnt' is assumed to be cleared.
|      * Current decision level must be greater than root level.
|
|    Post-conditions:
|      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
|      * If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the
|        rest of literals. There may be others from the same level though.
|
|________________________________________________________________________________________________@*/
void Solver::analyze(CRef confl, vec<Lit> &out_learnt, int &out_btlevel, int &out_lbd) {
    int pathC = 0;
    Lit p = lit_Undef;

    // Generate conflict clause:
    //
    out_learnt.push(); // (leave room for the asserting literal)
    int index = trail.size() - 1;
    int nDecisionLevel = level(var(ca[confl][0]));
    assert(nDecisionLevel == level(var(ca[confl][0])));

    do {
        assert(confl != CRef_Undef); // (otherwise should be UIP)
        Clause &c = ca[confl];

        // For binary clauses, we don't rearrange literals in propagate(), so check and make sure the first is an implied lit.
        if (p != lit_Undef && c.size() == 2 && value(c[0]) == l_False) {
            assert(value(c[1]) == l_True);
            Lit tmp = c[0];
            c[0] = c[1], c[1] = tmp;
        }

        // Update LBD if improved.
        if (c.learnt() && c.mark() != CORE) {
            int lbd = computeLBD(c);
            if (lbd < c.lbd()) {
                if (c.lbd() <= 30)
                    c.removable(false); // Protect once from reduction.
                c.set_lbd(lbd);
                if (lbd <= core_lbd_cut) {
                    learnts_core.push(confl);
                    c.mark(CORE);
                } else if (lbd <= 6 && c.mark() == LOCAL) {
                    // Bug: 'cr' may already be in 'learnts_tier2', e.g., if 'cr' was demoted from TIER2
                    // to LOCAL previously and if that 'cr' is not cleaned from 'learnts_tier2' yet.
                    learnts_tier2.push(confl);
                    c.mark(TIER2);
                }
            }

            if (c.mark() == TIER2)
                c.touched() = conflicts;
            else if (c.mark() == LOCAL)
                claBumpActivity(c);
        }

        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++) {
            Lit q = c[j];

            if (!seen[var(q)] && level(var(q)) > 0) {
                if (VSIDS) {
                    varBumpActivity(var(q), .5);
                    litBumpActivity(~q, .5);
                    add_tmp.push(q);
                } else
                    conflicted[var(q)]++;
                seen[var(q)] = 1;
                if (level(var(q)) >= nDecisionLevel) {
                    pathC++;
                } else
                    out_learnt.push(q);
            }
        }

        // Select next clause to look at:
        do {
            while (!seen[var(trail[index--])]);
            p = trail[index + 1];
        } while (level(var(p)) < nDecisionLevel);

        confl = reason(var(p));
        seen[var(p)] = 0;
        pathC--;

    } while (pathC > 0);
    out_learnt[0] = ~p;

    // Simplify conflict clause:
    //
    int i, j;
    out_learnt.copyTo(analyze_toclear);
    if (ccmin_mode == 2) {
        uint32_t abstract_level = 0;
        for (i = 1; i < out_learnt.size(); i++)
            abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)

        for (i = j = 1; i < out_learnt.size(); i++)
            if (reason(var(out_learnt[i])) == CRef_Undef || !litRedundant(out_learnt[i], abstract_level))
                out_learnt[j++] = out_learnt[i];

    } else if (ccmin_mode == 1) {
        for (i = j = 1; i < out_learnt.size(); i++) {
            Var x = var(out_learnt[i]);

            if (reason(x) == CRef_Undef)
                out_learnt[j++] = out_learnt[i];
            else {
                Clause &c = ca[reason(var(out_learnt[i]))];
                for (int k = c.size() == 2 ? 0 : 1; k < c.size(); k++)
                    if (!seen[var(c[k])] && level(var(c[k])) > 0) {
                        out_learnt[j++] = out_learnt[i];
                        break;
                    }
            }
        }
    } else
        i = j = out_learnt.size();

    max_literals += out_learnt.size();
    out_learnt.shrink(i - j);
    tot_literals += out_learnt.size();

    out_lbd = computeLBD(out_learnt);
    if (out_lbd <= 6 && out_learnt.size() <= 30) // Try further minimization?
        if (binResMinimize(out_learnt))
            out_lbd = computeLBD(out_learnt); // Recompute LBD if minimized.

    // Find correct backtrack level:
    //
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else {
        int max_i = 1;
        // Find the first literal assigned at the next-highest level:
        for (int i = 2; i < out_learnt.size(); i++)
            if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
                max_i = i;
        // Swap-in this literal at index 1:
        Lit p = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1] = p;
        out_btlevel = level(var(p));
    }

    if (VSIDS) {
        for (int i = 0; i < add_tmp.size(); i++) {
            Var v = var(add_tmp[i]);
            if (level(v) >= out_btlevel - 1)
                varBumpActivity(v, 1);
            litBumpActivity(~add_tmp[i], 1);
        }
        add_tmp.clear();
    } else {
        for (int i = 0; i < add_tmp.size(); i++) {
            Var v = var(add_tmp[i]);
            if (level(v) >= out_btlevel - 1) {
                litBumpActivity(~add_tmp[i], 1);
            }
        }
        add_tmp.clear();

        seen[var(p)] = true;
        for (int i = out_learnt.size() - 1; i >= 0; i--) {
            Var v = var(out_learnt[i]);
            CRef rea = reason(v);
            if (rea != CRef_Undef) {
                const Clause &reaC = ca[rea];
                for (int i = 0; i < reaC.size(); i++) {
                    Lit l = reaC[i];
                    if (!seen[var(l)]) {
                        seen[var(l)] = true;
                        almost_conflicted[var(l)]++;
                        analyze_toclear.push(l);
                    }
                }
            }
        }
    }

    for (int j = 0; j < analyze_toclear.size(); j++)
        seen[var(analyze_toclear[j])] = 0; // ('seen[]' is now cleared)
}

// Try further learnt clause minimization by means of binary clause resolution.
bool Solver::binResMinimize(vec<Lit> &out_learnt) {
    // Preparation: remember which false variables we have in 'out_learnt'.
    counter++;
    for (int i = 1; i < out_learnt.size(); i++)
        seen2[var(out_learnt[i])] = counter;

    // Get the list of binary clauses containing 'out_learnt[0]'.
    const vec<Watcher> &ws = watches_bin[~out_learnt[0]];

    int to_remove = 0;
    for (int i = 0; i < ws.size(); i++) {
        Lit the_other = ws[i].blocker;
        // Does 'the_other' appear negatively in 'out_learnt'?
        if (seen2[var(the_other)] == counter && value(the_other) == l_True) {
            to_remove++;
            seen2[var(the_other)] = counter - 1; // Remember to remove this variable.
        }
    }

    // Shrink.
    if (to_remove > 0) {
        int last = out_learnt.size() - 1;
        for (int i = 1; i < out_learnt.size() - to_remove; i++)
            if (seen2[var(out_learnt[i])] != counter)
                out_learnt[i--] = out_learnt[last--];
        out_learnt.shrink(to_remove);
    }
    return to_remove != 0;
}

// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool Solver::litRedundant(Lit p, uint32_t abstract_levels) {
    analyze_stack.clear();
    analyze_stack.push(p);
    int top = analyze_toclear.size();
    while (analyze_stack.size() > 0) {
        assert(reason(var(analyze_stack.last())) != CRef_Undef);
        Clause &c = ca[reason(var(analyze_stack.last()))];
        analyze_stack.pop();

        // Special handling for binary clauses like in 'analyze()'.
        if (c.size() == 2 && value(c[0]) == l_False) {
            assert(value(c[1]) == l_True);
            Lit tmp = c[0];
            c[0] = c[1], c[1] = tmp;
        }

        for (int i = 1; i < c.size(); i++) {
            Lit p = c[i];
            if (!seen[var(p)] && level(var(p)) > 0) {
                if (reason(var(p)) != CRef_Undef && (abstractLevel(var(p)) & abstract_levels) != 0) {
                    seen[var(p)] = 1;
                    analyze_stack.push(p);
                    analyze_toclear.push(p);
                } else {
                    for (int j = top; j < analyze_toclear.size(); j++)
                        seen[var(analyze_toclear[j])] = 0;
                    analyze_toclear.shrink(analyze_toclear.size() - top);
                    return false;
                }
            }
        }
    }

    return true;
}

/*_________________________________________________________________________________________________
|
|  analyzeFinal : (p : Lit)  ->  [void]
|
|  Description:
|    Specialized analysis procedure to express the final conflict in terms of assumptions.
|    Calculates the (possibly empty) set of assumptions that led to the assignment of 'p', and
|    stores the result in 'out_conflict'.
|________________________________________________________________________________________________@*/
void Solver::analyzeFinal(Lit p, vec<Lit> &out_conflict) {
    out_conflict.clear();
    out_conflict.push(p);

    if (decisionLevel() == 0)
        return;

    seen[var(p)] = 1;

    for (int i = trail.size() - 1; i >= trail_lim[0]; i--) {
        Var x = var(trail[i]);
        if (seen[x]) {
            if (reason(x) == CRef_Undef) {
                assert(level(x) > 0);
                out_conflict.push(~trail[i]);
            } else {
                Clause &c = ca[reason(x)];
                for (int j = c.size() == 2 ? 0 : 1; j < c.size(); j++)
                    if (level(var(c[j])) > 0)
                        seen[var(c[j])] = 1;
            }
            seen[x] = 0;
        }
    }

    seen[var(p)] = 0;
}

void Solver::uncheckedEnqueue(Lit p, int level, CRef from) {
    assert(value(p) == l_Undef);
    Var x = var(p);
    if (!VSIDS) {
        picked[x] = conflicts;
        conflicted[x] = 0;
        almost_conflicted[x] = 0;
#ifdef ANTI_EXPLORATION
        uint32_t age = conflicts - canceled[var(p)];
        if (age > 0) {
            double decay = pow(0.95, age);
            activity_CHB[var(p)] *= decay;
            if (order_heap_CHB.inHeap(var(p)))
                order_heap_CHB.increase(var(p));
        }
#endif
    }
    assigns[x] = lbool(!sign(p));
    vardata[x] = mkVarData(from, level);
    trail.push_(p);
}

/*_________________________________________________________________________________________________
|
|  propagate : [void]  ->  [Clause*]
|
|  Description:
|    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
|    otherwise CRef_Undef.
|
|    Post-conditions:
|      * the propagation queue is empty, even if there was a conflict.
|________________________________________________________________________________________________@*/
CRef Solver::propagate() {
    CRef confl = CRef_Undef;
    watches.cleanAll();
    watches_bin.cleanAll();

    while (qhead < trail.size()) {
        Lit p = trail[qhead++]; // 'p' is enqueued fact to propagate.
        int currLevel = level(var(p));
        vec<Watcher> &ws = watches[p];
        Watcher *i, *j, *end;

        vec<Watcher> &ws_bin = watches_bin[p]; // Propagate binary clauses first.
        for (int k = 0; k < ws_bin.size(); k++) {
            Lit the_other = ws_bin[k].blocker;
            if (value(the_other) == l_False) {
                confl = ws_bin[k].cref;
#ifdef LOOSE_PROP_STAT
                return confl;
#else
                goto ExitProp;
#endif
            } else if (value(the_other) == l_Undef) {
                uncheckedEnqueue(the_other, currLevel, ws_bin[k].cref);
#ifdef PRINT_OUT
                std::cout << "i " << the_other << " l " << currLevel << "\n";
#endif
            }
        }

        for (i = j = (Watcher *) ws, end = i + ws.size(); i != end;) {
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True) {
                *j++ = *i++;
                continue;
            }

            // Make sure the false literal is data[1]:
            CRef cr = i->cref;
            Clause &c = ca[cr];
            Lit false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit first = c[0];
            Watcher w = Watcher(cr, first);
            if (first != blocker && value(first) == l_True) {
                *j++ = w;
                continue;
            }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False) {
                    c[1] = c[k];
                    c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause;
                }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False) {
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            } else {
                if (currLevel == decisionLevel()) {
                    uncheckedEnqueue(first, currLevel, cr);
#ifdef PRINT_OUT
                    std::cout << "i " << first << " l " << currLevel << "\n";
#endif
                } else {
                    int nMaxLevel = currLevel;
                    int nMaxInd = 1;
                    // pass over all the literals in the clause and find the one with the biggest level
                    for (int nInd = 2; nInd < c.size(); ++nInd) {
                        int nLevel = level(var(c[nInd]));
                        if (nLevel > nMaxLevel) {
                            nMaxLevel = nLevel;
                            nMaxInd = nInd;
                        }
                    }

                    if (nMaxInd != 1) {
                        std::swap(c[1], c[nMaxInd]);
                        *j--; // undo last watch
                        watches[~c[1]].push(w);
                    }

                    uncheckedEnqueue(first, nMaxLevel, cr);
#ifdef PRINT_OUT
                    std::cout << "i " << first << " l " << nMaxLevel << "\n";
#endif
                }
            }

            NextClause:;
        }
        ws.shrink(i - j);
    }
    return confl;
}

/*_________________________________________________________________________________________________
|
|  reduceDB : ()  ->  [void]
|
|  Description:
|    Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
|    clauses are clauses that are reason to some assignment. Binary clauses are never removed.
|________________________________________________________________________________________________@*/
struct reduceDB_lt {
    ClauseAllocator &ca;

    reduceDB_lt(ClauseAllocator &ca_) : ca(ca_) {}

    bool operator()(CRef x, CRef y) const { return ca[x].activity() < ca[y].activity(); }
};

void Solver::reduceDB() {
    int i, j;

    sort(learnts_local, reduceDB_lt(ca));

    int limit = learnts_local.size() / 2;
    for (i = j = 0; i < learnts_local.size(); i++) {
        Clause &c = ca[learnts_local[i]];
        if (c.mark() == LOCAL) {
            if (c.removable() && !locked(c) && i < limit) {
                removeClause(learnts_local[i]);
            } else {
                if (!c.removable())
                    limit++;
                c.removable(true);
                learnts_local[j++] = learnts_local[i];
            }
        }
    }
    learnts_local.shrink(i - j);

    checkGarbage();
}

void Solver::reduceDB_Tier2() {
    int i, j;
    for (i = j = 0; i < learnts_tier2.size(); i++) {
        Clause &c = ca[learnts_tier2[i]];
        if (c.mark() == TIER2) {
            if (!locked(c) && c.touched() + 30000 < conflicts) {
                learnts_local.push(learnts_tier2[i]);
                c.mark(LOCAL);
                c.activity() = 0;
                claBumpActivity(c);
            } else {
                learnts_tier2[j++] = learnts_tier2[i];
            }
        }
    }
    learnts_tier2.shrink(i - j);
}

void Solver::removeSatisfied(vec<CRef> &cs) {
    int i, j;
    for (i = j = 0; i < cs.size(); i++) {
        Clause &c = ca[cs[i]];
        if (satisfied(c))
            removeClause(cs[i]);
        else
            cs[j++] = cs[i];
    }
    cs.shrink(i - j);
}

void Solver::safeRemoveSatisfied(vec<CRef> &cs, unsigned valid_mark) {
    int i, j;
    for (i = j = 0; i < cs.size(); i++) {
        Clause &c = ca[cs[i]];
        if (c.mark() == valid_mark) {
            if (satisfied(c)) {
                removeClause(cs[i]);
            } else {
                cs[j++] = cs[i];
            }
        }
    }
    cs.shrink(i - j);
}

void Solver::rebuildOrderHeap() {
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if (decision[v] && value(v) == l_Undef)
            vs.push(v);

    order_heap_CHB.build(vs);
    order_heap_VSIDS.build(vs);
    order_heap_distance.build(vs);
}

/*_________________________________________________________________________________________________
|
|  simplify : [void]  ->  [bool]
|
|  Description:
|    Simplify the clause database according to the current top-level assigment. Currently, the only
|    thing done here is the removal of satisfied clauses, but more things can be put here.
|________________________________________________________________________________________________@*/
bool Solver::simplify() {
    assert(decisionLevel() == 0);

    if (!ok || propagate() != CRef_Undef)
        return ok = false;

    if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
        return true;

    // Remove satisfied clauses:
    removeSatisfied(learnts_core); // Should clean core first.
    safeRemoveSatisfied(learnts_tier2, TIER2);
    safeRemoveSatisfied(learnts_local, LOCAL);
    if (remove_satisfied) // Can be turned off.
        removeSatisfied(clauses);
    checkGarbage();
    rebuildOrderHeap();

    simpDB_assigns = nAssigns();
    simpDB_props = clauses_literals + learnts_literals; // (shouldn't depend on stats really, but it will do for now)

    return true;
}

// pathCs[k] is the number of variables assigned at level k,
// it is initialized to 0 at the begining and reset to 0 after the function execution
bool Solver::collectFirstUIP(CRef confl) {
    involved_lits.clear();
    int max_level = 1;
    Clause &c = ca[confl];
    int minLevel = decisionLevel();
    for (int i = 0; i < c.size(); i++) {
        Var v = var(c[i]);
        if (level(v) > 0) {
            seen[v] = 1;
            var_iLevel_tmp[v] = 1;
            pathCs[level(v)]++;
            if (minLevel > level(v)) {
                minLevel = level(v);
                assert(minLevel > 0);
            }
        }
    }

    int limit = trail_lim[minLevel - 1];
    for (int i = trail.size() - 1; i >= limit; i--) {
        Lit p = trail[i];
        Var v = var(p);
        if (seen[v]) {
            int currentDecLevel = level(v);
            seen[v] = 0;
            if (--pathCs[currentDecLevel] != 0) {
                Clause &rc = ca[reason(v)];
                int reasonVarLevel = var_iLevel_tmp[v] + 1;
                if (reasonVarLevel > max_level)
                    max_level = reasonVarLevel;
                if (rc.size() == 2 && value(rc[0]) == l_False) {
                    // Special case for binary clauses
                    // The first one has to be SAT
                    assert(value(rc[1]) != l_False);
                    Lit tmp = rc[0];
                    rc[0] = rc[1], rc[1] = tmp;
                }
                for (int j = 1; j < rc.size(); j++) {
                    Lit q = rc[j];
                    Var v1 = var(q);
                    if (level(v1) > 0) {
                        if (minLevel > level(v1)) {
                            minLevel = level(v1);
                            limit = trail_lim[minLevel - 1];
                            assert(minLevel > 0);
                        }
                        if (seen[v1]) {
                            if (var_iLevel_tmp[v1] < reasonVarLevel)
                                var_iLevel_tmp[v1] = reasonVarLevel;
                        } else {
                            var_iLevel_tmp[v1] = reasonVarLevel;
                            seen[v1] = 1;
                            pathCs[level(v1)]++;
                        }
                    }
                }
            }
            involved_lits.push(p);
        }
    }
    double inc = var_iLevel_inc;
    vec<int> level_incs;
    level_incs.clear();
    for (int i = 0; i < max_level; i++) {
        level_incs.push(inc);
        inc = inc / my_var_decay;
    }

    for (int i = 0; i < involved_lits.size(); i++) {
        Var v = var(involved_lits[i]);
        activity_distance[v] += var_iLevel_tmp[v] * level_incs[var_iLevel_tmp[v] - 1];

        if (activity_distance[v] > 1e100) {
            for (int vv = 0; vv < nVars(); vv++)
                activity_distance[vv] *= 1e-100;
            var_iLevel_inc *= 1e-100;
            for (int j = 0; j < max_level; j++)
                level_incs[j] *= 1e-100;
        }
        if (order_heap_distance.inHeap(v))
            order_heap_distance.decrease(v);
    }
    var_iLevel_inc = level_incs[level_incs.size() - 1];
    return true;
}

/*_________________________________________________________________________________________________
|
|  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
|
|  Description:
|    Search for a model the specified number of conflicts.
|
|  Output:
|    'l_True' if a partial assigment that is consistent with respect to the clauseset is found. If
|    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
|    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
|________________________________________________________________________________________________@*/

void Solver::info_based_rephase() {
    int var_nums = nVars();
    for (int i = 0; i < var_nums; ++i)
        polarity[i] = !ls_mediation_soln[i];
    if (!DISTANCE) {
        for (int i = 0; i < var_nums; ++i) {
            if (ccnr.conflict_ct[i + 1] > 0) {
                if (VSIDS) {
                    varBumpActivity(i, ccnr.conflict_ct[i + 1] * 100 / ccnr.step);
                } else {
                    conflicted[i] += max((long long int) 1, ccnr.conflict_ct[i + 1] * 100 / ccnr.step);
                }
            }
        }
    }
}

void Solver::rand_based_rephase() {
    int var_nums = nVars();
    int pick_rand = conflicts % 1000;
    // int pick_rand = mersenne_next(&randgen, 1000);


    // local search
    if ((pick_rand -= 100) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = !ls_best_soln[i];
    } else if ((pick_rand -= 300) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = !ls_mediation_soln[i];
        mediation_used = true;
    }
        // top_trail 200
    else if ((pick_rand -= 300) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = !top_trail_soln[i];
    }
        // reverse
    else if ((pick_rand -= 50) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = !polarity[i];
    } else if ((pick_rand -= 25) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = ls_best_soln[i];
    } else if ((pick_rand -= 25) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = top_trail_soln[i];
    }
        // 150
    else if ((pick_rand -= 140) < 0) {
        for(int i=0;i<var_nums;++i) polarity[i] = conflicts % 2 == 0 ? 1 : 0;
        // for(int i=0;i<var_nums;++i) polarity[i] = mersenne_next(&randgen,2)==0?1:0;
    } else if ((pick_rand -= 5) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = 1;
    } else if ((pick_rand -= 5) < 0) {
        for (int i = 0; i < var_nums; ++i)
            polarity[i] = 0;
    }
        // 50
    else {
        // do nothing
    }
}

bool Solver::importUnitClauses() {
    assert(decisionLevel() == 0);

    if (cbkImportUnit == NULL)
        return true;
    Lit l;
    while ((l = cbkImportUnit(issuer)) != lit_Undef) {
        if (value(var(l)) == l_Undef) {
            uncheckedEnqueue(l);
        } else if (value(l) == l_False) {
            return false;
        }
    }

    return true;
}

bool Solver::importClauses() {
    assert(decisionLevel() == 0);

    if (cbkImportClause == NULL)
        return true;
    int lbd, k, l;
    bool alreadySat;
    while (cbkImportClause(issuer, &lbd, importedClause)) {
        alreadySat = false;
        // Simplify clause before add
        for (k = l = 0; k < importedClause.size(); k++) {
            if (value(importedClause[k]) == l_True) {
                alreadySat = true;
                break;
            } else if (value(importedClause[k]) == l_Undef) {
                importedClause[l++] = importedClause[k];
            }
        }
        importedClause.shrink(k - l);

        if (alreadySat) {
            importedClause.clear();
            continue;
        }

        if (importedClause.size() == 0) {
           return false;
        } else if (importedClause.size() == 1) {
            uncheckedEnqueue(importedClause[0]);
        } else {
            CRef cr = ca.alloc(importedClause, true);
            lbd = importedClause.size();
            ca[cr].set_lbd(lbd);
            if (lbd <= core_lbd_cut) {
                learnts_core.push(cr);
                ca[cr].mark(CORE);
            } else if (lbd <= 6) {
                learnts_tier2.push(cr);
                ca[cr].mark(TIER2);
                ca[cr].touched() = conflicts;
            } else {
                learnts_local.push(cr);
                claBumpActivity(ca[cr]);
            }
            attachClause(cr);
        }
        importedClause.clear();
    }

    return true;
}

lbool Solver::search(int &nof_conflicts) {
    assert(ok);
    int backtrack_level;
    int lbd;
    vec<Lit> learnt_clause;
    bool cached = false;
    starts++;

    int conflictC = 0;
    vec<Lit> units;

    freeze_ls_restart_num--;
    bool can_call_ls = true;

    if (decisionLevel() == 0) { // We import clauses
        if (!importUnitClauses()) return l_False;
        if (!importClauses()) return l_False;
    }

    if (sls && ls_ready && starts > state_change_time) {
        // if(mersenne_next(&randgen,100)<50) info_based_rephase();
        if(conflicts % 2 == 0) info_based_rephase();
        else rand_based_rephase();
    }

    // simplify
    //
    if (conflicts >= curSimplify * nbconfbeforesimplify) {
        nbSimplifyAll++;
        if (!simplifyAll()) {
            return l_False;
        }
        curSimplify = (conflicts / nbconfbeforesimplify) + 1;
        nbconfbeforesimplify += incSimplify;
    }

    for (;;) {
        CRef confl = propagate();

        if (confl != CRef_Undef) {
            // CONFLICT
            if (VSIDS) {
                if (--timer == 0 && var_decay < 0.95)
                    timer = 5000, var_decay += 0.01;
            } else if (step_size > min_step_size)
                step_size -= step_size_dec;

            conflicts++;
            nof_conflicts--;
            ConflictData data = FindConflictLevel(confl);
            if (data.nHighestLevel == 0)
                return l_False;
            if (data.bOnlyOneLitFromHighest) {
                cancelUntil(data.nHighestLevel - 1);
                continue;
            }

            learnt_clause.clear();
            if (conflicts > 50000)
                DISTANCE = 0;
            else
                DISTANCE = 1;
            if (VSIDS && DISTANCE)
                collectFirstUIP(confl);

            analyze(confl, learnt_clause, backtrack_level, lbd);
            if (cbkExportClause != NULL)
                cbkExportClause(issuer, lbd, learnt_clause);

            // check chrono backtrack condition
            if ((confl_to_chrono < 0 || confl_to_chrono <= conflicts) && chrono > -1 && (decisionLevel() - backtrack_level) >= chrono) {
                ++chrono_backtrack;
                cancelUntil(data.nHighestLevel - 1);
            } else // default behavior
            {
                ++non_chrono_backtrack;
                cancelUntil(backtrack_level);
            }

            lbd--;
            if (VSIDS) {
                cached = false;
                conflicts_VSIDS++;
                lbd_queue.push(lbd);
                global_lbd_sum += (lbd > 50 ? 50 : lbd);
            }

            if (learnt_clause.size() == 1) {
                uncheckedEnqueue(learnt_clause[0]);
            } else {
                CRef cr = ca.alloc(learnt_clause, true);
                ca[cr].set_lbd(lbd);
                // duplicate learnts
                int id = 0;
                if (lbd <= max_lbd_dup) {
                    std::vector<uint32_t> tmp;
                    for (int i = 0; i < learnt_clause.size(); i++)
                        tmp.push_back(learnt_clause[i].x);
                    id = is_duplicate(tmp);
                    if (id == min_number_of_learnts_copies + 1) {
                        duplicates_added_conflicts++;
                    }
                    if (id == min_number_of_learnts_copies) {
                        duplicates_added_tier2++;
                    }
                }
                // duplicate learnts

                if ((lbd <= core_lbd_cut) || (id == min_number_of_learnts_copies + 1)) {
                    learnts_core.push(cr);
                    ca[cr].mark(CORE);
                } else if ((lbd <= 6) || (id == min_number_of_learnts_copies)) {
                    learnts_tier2.push(cr);
                    ca[cr].mark(TIER2);
                    ca[cr].touched() = conflicts;
                } else {
                    learnts_local.push(cr);
                    claBumpActivity(ca[cr]);
                }
                attachClause(cr);

                uncheckedEnqueue(learnt_clause[0], backtrack_level, cr);
#ifdef PRINT_OUT
                std::cout << "new " << ca[cr] << "\n";
                std::cout << "ci " << learnt_clause[0] << " l " << backtrack_level << "\n";
#endif
            }
            if (drup_file) {
#ifdef BIN_DRUP
                binDRUP('a', learnt_clause, drup_file);
#else
                for (int i = 0; i < learnt_clause.size(); i++)
                    fprintf(drup_file, "%i ", (var(learnt_clause[i]) + 1) * (-2 * sign(learnt_clause[i]) + 1));
                fprintf(drup_file, "0\n");
#endif
            }

            if (VSIDS)
                varDecayActivity();
            claDecayActivity();

            if (--learntsize_adjust_cnt == 0) {
                learntsize_adjust_confl *= learntsize_adjust_inc;
                learntsize_adjust_cnt = (int) learntsize_adjust_confl;
                max_learnts *= learntsize_inc;
            }

            // the top_trail_soln should be update after each conflict
            if (trail.size() > max_trail) {
                max_trail = trail.size();
                if (sls) {
                    int var_nums = nVars();
                    for (int idx_i = 0; idx_i < var_nums; ++idx_i) {
                        lbool value_i = value(idx_i);
                        if (value_i == l_Undef)
                            top_trail_soln[idx_i] = !polarity[idx_i];
                        else {
                            top_trail_soln[idx_i] = value_i == l_True ? 1 : 0;
                        }
                    }
                }
            }
            if (boost) {
                polarity[trail.size()] = !polarity[trail.size()];
            }
            if (VSIDS) {
                if (trail.size() > global) {
                    global = trail.size();
                    if (global > cursor) {
                        cursor = global;
                        if (log) {
#ifdef MASSIVE
                            printf("\rc %.2f%% [%i]                           ", 100.0 * (nVars() - global) / nVars(), rank);
#else
                            // printf("\rc %.2f%%                                ", 100.0 * (nVars() - global) / nVars());
#endif
                            fflush(stdout);
                        }
                    } else if (trail.size() < global) {
                        if (boost) {
                            polarity[trail.size()] = !polarity[trail.size()];
                        }
                    }
                }
            }
        } else {

            // NO_CONFLICT
            if (sls && starts > state_change_time) {
                if (can_call_ls && freeze_ls_restart_num < 1 && mediation_used && (trail.size() > (int) (conflict_ratio * nVars()) || trail.size() > (int) (percent_ratio * max_trail)) //&& up_time_ratio * search_start_cpu_time > ls_used_time
                        ) {

                    can_call_ls = false;
                    mediation_used = false;
                    freeze_ls_restart_num = restarts_gap;
                    bool res = call_ls(true);

                    if (res) {
                        solved_by_ls = true;
                        return l_True;
                    }
                }
            }
            if (polarity_init_method == 1 && !DISTANCE && freeze_ls_restart_num < 1 && (trail.size() > (int) (conflict_ratio * nVars()) || trail.size() > (int) (percent_ratio * max_trail))) {
                double before_init_time, after_init_time;
                freeze_ls_restart_num = restarts_gap;
                if (log)
                    before_init_time = cpuTime();
                search_init->bayesian(sls ? bmm_soln : NULL);
                if (log) {
                    after_init_time = cpuTime();
                    printf("c [%3.2f] Initialization time:\t%12.2f s\n", after_init_time,after_init_time - before_init_time);
                }
            }
            bool restart = false;
            if (!VSIDS)
                restart = nof_conflicts <= 0;
            else if (!cached) {
                restart = lbd_queue.full() && (lbd_queue.avg() * 0.8 > global_lbd_sum / conflicts_VSIDS);
                cached = true;
            }
            if (restart) {
                lbd_queue.clear();
                cached = false;
                // Reached bound on number of conflicts:
                cancelUntil(0);
                return l_Undef;
            }

            // Simplify the set of problem clauses:
            if (decisionLevel() == 0 && !simplify())
                return l_False;

            if (conflicts >= next_T2_reduce) {
                next_T2_reduce = conflicts + 10000;
                reduceDB_Tier2();
            }
            if (conflicts >= next_L_reduce) {
                next_L_reduce = conflicts + 15000;
                reduceDB();
            }

            Lit next = lit_Undef;
            while (decisionLevel() < assumptions.size()) {
                // Perform user provided assumption:
                Lit p = assumptions[decisionLevel()];
                if (value(p) == l_True) {
                    // Dummy decision level:
                    newDecisionLevel();
                } else if (value(p) == l_False) {
                    analyzeFinal(~p, conflict);
                    return l_False;
                } else {
                    next = p;
                    break;
                }
            }

            // New variable decision:
            decisions++;
            next = pickBranchLit();

            if (next == lit_Undef) {
                // Model found:
                for (int i = 0; i < nClauses(); i++) {
                    if (!satisfied(ca[clauses[i]])) {
                        return l_False;
                    }
                }
                return l_True;
            }

            // Increase decision level and enqueue 'next'
            newDecisionLevel();
            uncheckedEnqueue(next, decisionLevel());
        }
    }
}

/*
  Finite subsequences of the Luby-sequence:

  0: 1
  1: 1 1 2
  2: 1 1 2 1 1 2 4
  3: 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8
  ...


 */

static double luby(double y, int x) {

    // Find the finite subsequence that contains index 'x', and the
    // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x + 1; seq++, size = 2 * size + 1);

    while (size - 1 != x) {
        size = (size - 1) >> 1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

int Solver::oracle(int glb) {
    int falses = nClauses() + nLearnts();
    for (int i = 0; i < clauses.size(); i++) {
        falses -= satisfied(ca[clauses[i]]);
    }
    for (int i = 0; i < learnts_core.size(); i++) {
        falses -= satisfied(ca[learnts_core[i]]);
    }
    for (int i = 0; i < learnts_tier2.size(); i++) {
        falses -= satisfied(ca[learnts_tier2[i]]);
    }
    for (int i = 0; i < learnts_local.size(); i++) {
        falses -= satisfied(ca[learnts_local[i]]);
    }
    return falses;
}

lbool Solver::neg(lbool x) {
    if (x == l_Undef) {
        return conflicts % 2 ? l_False : l_True;
    } else {
        return x == l_False ? l_True : l_False;
    }
}

lbool Solver::hess_second_order() {
    int loc, glb = INT32_MAX;
    assigns.copyTo(aux);
    for (int i = 0; i < nVars(); i++) {
        assigns[i] = neg(neg(assigns[i]));
    }
    for (;;) {
        bool done = true;
        for (int k = 0; k < nVars() * nVars(); k++) {
            int i = k / nVars();
            int j = k % nVars();
            if (i == j) {
                assigns[i] = neg(assigns[j]);
            } else {
                lbool tmp = assigns[i];
                assigns[i] = neg(assigns[j]);
                assigns[j] = tmp;
            }
            loc = oracle(glb);
            if (loc < glb) {
                glb = loc;
                if (glb < hess_cursor) {
                    hess_cursor = glb;
                    if (log) {
#ifdef MASSIVE
                        printf("\rc %.2f%% [%i] (*)                       ", 100.0 * glb / (nClauses() + nLearnts()), rank);
#else
                        // printf("\rc %.2f%% (**)                           ", 100.0 * glb / (nClauses() + nLearnts()));
#endif
                        fflush(stdout);
                    }
                    if (glb == 0) {
                        solved_by_hess = true;
                        goto finally;
                    }
                    done = false;
                }
            } else if (loc > glb) {
                if (i == j) {
                    assigns[i] = neg(assigns[j]);
                } else {
                    lbool tmp = assigns[i];
                    assigns[i] = neg(assigns[j]);
                    assigns[j] = tmp;
                }
            } else {
                goto finally;
            }
        }
        if (done) {
            break;
        }
    }
    finally:
    for (int i = 0; i < nVars(); i++) {
        polarity[i] = assigns[i] != l_True;
    }
    aux.copyTo(assigns);
    cancelUntil(0);
    return l_Undef;
}

lbool Solver::hess_first_order() {
    int loc, glb = INT32_MAX;
    assigns.copyTo(aux);
    for (int i = 0; i < nVars(); i++) {
        assigns[i] = neg(neg(assigns[i]));
    }
    for (;;) {
        bool done = true;
        for (int i = 0; i < nVars(); i++) {
            assigns[i] = neg(assigns[i]);
            loc = oracle(glb);
            if (loc < glb) {
                glb = loc;
                if (glb < hess_cursor) {
                    hess_cursor = glb;
                    if (log) {
#ifdef MASSIVE
                        printf("\rc %.2f%% [%i] (*)                       ", 100.0 * glb / (nClauses() + nLearnts()), rank);
#else
                        // printf("\rc %.2f%% (*)                            ", 100.0 * glb / (nClauses() + nLearnts()));
#endif
                        fflush(stdout);
                    }
                    if (glb == 0) {
                        solved_by_hess = true;
                        goto finally;
                    }
                    done = false;
                }
            } else if (loc > glb) {
                assigns[i] = neg(assigns[i]);
            } else {
                goto finally;
            }
        }
        if (done) {
            break;
        }
    }
    finally:
    for (int i = 0; i < nVars(); i++) {
        polarity[i] = assigns[i] != l_True;
    }
    aux.copyTo(assigns);
    cancelUntil(0);
    return l_Undef;
}

void Solver::rand_init() {
    for (int i = 0; i < nVars(); i++) {
        // polarity[i] = rand() % 2;
         polarity[i] = mersenne_next(&randgen, 2);
    }
}

void Solver::init_seed(unsigned int seed) {
    rseed = seed;
    // mersenne_init_with_seed(&randgen, this->id + 1);
    mersenne_init_with_seed(&randgen, seed);
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_() {
    cursor = 0;
    hess_cursor = INT32_MAX;
    solved_by_hess = false;
    ls_ready = false;
    render = true;

#ifdef MASSIVE
    massive = true;
#endif

    if (massive) {
        seed = time(NULL) + time(NULL) * rank;
        srand(seed);
        rand_init();
    }
    if (polarity_init_method == 0) {
        rand_init();
    }

    model.clear();
    conflict.clear();
    if (!ok)
        return l_False;


    if (polarity_init_method == 1) {
        double before_init_time, after_init_time;

        init_epochs = 100;
        bmm_soln    = new char[nVars()+2];
        for (int v=0; v < nVars(); v++) {
            bmm_soln[v] = 0;
        }
        before_init_time = cpuTime();
        if (sls)
            search_init = new SearchInitializer(this, polarity_init_method, activity_init_method, init_epochs, update_epochs, bmm_soln);
        else
            search_init = new SearchInitializer(this, polarity_init_method, activity_init_method, init_epochs, update_epochs);
        after_init_time = cpuTime();

        if (log)
            printf("c [%3.2f] Initialization time:\t%12.2f s\n", after_init_time,after_init_time - before_init_time);
        search_init->setEpochs(opt_init_epochs);
    }

    max_learnts = nClauses() * learntsize_factor;
    learntsize_adjust_confl = learntsize_adjust_start_confl;
    learntsize_adjust_cnt = (int) learntsize_adjust_confl;
    lbool status = l_Undef;

    ls_mediation_soln.resize(nVars());
    ls_best_soln.resize(nVars());
    top_trail_soln.resize(nVars());

    add_tmp.clear();

    if (sls) {
        int fls_res = call_ls(false);
        if (fls_res) {
            status = l_True;
        }
    }

    VSIDS = true;
    int init = 10000;
    while (status == l_Undef && init > 0)
        status = search(init);
    VSIDS = false;

    duplicates_added_conflicts = 0;
    duplicates_added_minimization = 0;
    duplicates_added_tier2 = 0;

    dupl_db_size = 0;

    // Search:
    uint64_t curr_props = 0;
    int curr_restarts = 0;
    last_switch_conflicts = starts;

    vec<Lit> learnt_clause;
    vec<Lit> units;
    int backtrack_level;
    while (status == l_Undef) {
        global = 0;
        if (boost_alternate) {
            boost = !boost;
        }
        if (VSIDS) {
            int weighted = INT32_MAX;
            status = search(weighted);
        } else {
            int nof_conflicts = luby(restart_inc, curr_restarts) * restart_first;
            curr_restarts++;
            status = search(nof_conflicts);
        }
        if (propagations - curr_props > VSIDS_props_limit) {
            curr_props = propagations;
            VSIDS_props_limit = VSIDS_props_limit + VSIDS_props_limit / 10;
        }
        if (starts - last_switch_conflicts > switch_heristic_mod) {
            uint32_t removed_duplicates = 0;
            std::vector<std::vector<uint64_t> > tmp;
            for (auto &outer_mp : ht) {                  // variables
                for (auto &inner_mp : outer_mp.second) { // sizes
                    for (auto &in_in_mp : inner_mp.second) {
                        if (in_in_mp.second >= min_number_of_learnts_copies) {
                            tmp.push_back({(uint64_t) outer_mp.first, inner_mp.first, in_in_mp.first, in_in_mp.second});
                        }
                    }
                }
            }
            removed_duplicates = dupl_db_size - tmp.size();
            ht.clear();
            for (auto &i : tmp) {
                ht[i[0]][i[1]][i[2]] = i[3];
            }
            dupl_db_size -= removed_duplicates;

            if (hess) {
                if (hess_order == 1) {
                    hess_first_order();
                } else if (hess_order == 2) {
                    hess_second_order();
                }
            }
            if (VSIDS) {
                VSIDS = false;
            } else {
                VSIDS = true;
                picked.clear();
                conflicted.clear();
                almost_conflicted.clear();
#ifdef ANTI_EXPLORATION
                canceled.clear();
#endif
            }
            last_switch_conflicts = starts;
        }
    }

#ifdef BIN_DRUP
    if (drup_file && status == l_False)
        binDRUP_flush(drup_file);
#endif

    if (status == l_True) {
        // Extend & copy model:
        model.growTo(nVars());
        if (solved_by_ls) {
            if (log) {
                printf("c LS!                \n");
            }
            for (int i = 0; i < nVars(); i++)
                model[i] = ls_mediation_soln[i] ? l_True : l_False;
        } else {
            
            if (solved_by_hess) {
                if (log) {
                    printf("\rc HESS!             \n");
                }
                for (int i = 0; i < nVars(); i++)
                    model[i] = assigns[i];
            } else {
                if (log) {
                    printf("\rc SLIME!             \n");
                }
                for (int i = 0; i < nVars(); i++)
                    model[i] = value(i);
            }
        }

    } else if (status == l_False && conflict.size() == 0) {
        ok = false;
        if (log) {
            printf("\rc SLIME!             \n");
        }
    }
    if (solved_by_ls) {
        printf("c LS!                \n");
    }
    else if (polarity_init_method == 1) {
        printf("c BMM!                \n");
    }
    else if (sls) {
        printf("c SLIME!                \n");
    }

    cancelUntil(0);
    return status;
}

//=================================================================================================
// Writing CNF to DIMACS:
//
// FIXME: this needs to be rewritten completely.

static Var mapVar(Var x, vec<Var> &map, Var &max) {
    if (map.size() <= x || map[x] == -1) {
        map.growTo(x + 1, -1);
        map[x] = max++;
    }
    return map[x];
}

void Solver::toDimacs(FILE *f, Clause &c, vec<Var> &map, Var &max) {
    // if (satisfied(c))
    //    return;

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False)
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max) + 1);
    fprintf(f, "0\n");
}

void Solver::toDimacs(const char *file, const vec<Lit> &assumps) {
    FILE *f = fopen(file, "wr");
    if (f == NULL)
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    toDimacs(f, assumps);
    fclose(f);
}

void Solver::toDimacs(FILE *f, const vec<Lit> &assumps) {
    fprintf(f, "c PEQNP - www.peqnp.com\n");
    fprintf(f, "c contact@peqnp.science\n");
    fprintf(f, "c pip install PEQNP\n");

    // Handle case when solver is in contradictory state:
    if (!ok) {
        fprintf(f, "p cnf 1 2\n1 0\n-1 0\n");
        return;
    }

    vec<Var> map;
    Var max = 0;

    // Cannot use removeClauses here because it is not safe
    // to deallocate them at this point. Could be improved.
    // int cnt = 0;
    // for (int i = 0; i < clauses.size(); i++)
    //    if (!satisfied(ca[clauses[i]]))
    //        cnt++;

    for (int i = 0; i < clauses.size(); i++) {
        // if (!satisfied(ca[clauses[i]])) {
        Clause &c = ca[clauses[i]];
        for (int j = 0; j < c.size(); j++) {
            // if (value(c[j]) != l_False)
            mapVar(var(c[j]), map, max);
        }
        // }
    }

    // Assumptions are added as unit clauses:
    // cnt += assumptions.size();

    fprintf(f, "p cnf %d %d\n", max, assumptions.size() + clauses.size());

    for (int i = 0; i < assumptions.size(); i++) {
        assert(value(assumptions[i]) != l_False);
        fprintf(f, "%s%d 0\n", sign(assumptions[i]) ? "-" : "", mapVar(var(assumptions[i]), map, max) + 1);
    }

    for (int i = 0; i < clauses.size(); i++)
        toDimacs(f, ca[clauses[i]], map, max);
}

//=================================================================================================
// Garbage Collection methods:

void Solver::relocAll(ClauseAllocator &to) {
    // All watchers:
    //
    watches.cleanAll();
    watches_bin.cleanAll();
    for (int v = 0; v < nVars(); v++)
        for (int s = 0; s < 2; s++) {
            Lit p = mkLit(v, s);
            vec<Watcher> &ws = watches[p];
            for (int j = 0; j < ws.size(); j++)
                ca.reloc(ws[j].cref, to);
            vec<Watcher> &ws_bin = watches_bin[p];
            for (int j = 0; j < ws_bin.size(); j++)
                ca.reloc(ws_bin[j].cref, to);
        }

    // All reasons:
    //
    for (int i = 0; i < trail.size(); i++) {
        Var v = var(trail[i]);

        if (reason(v) != CRef_Undef && (ca[reason(v)].reloced() || locked(ca[reason(v)])))
            ca.reloc(vardata[v].reason, to);
    }

    // All learnt:
    //
    for (int i = 0; i < learnts_core.size(); i++)
        ca.reloc(learnts_core[i], to);
    for (int i = 0; i < learnts_tier2.size(); i++)
        ca.reloc(learnts_tier2[i], to);
    for (int i = 0; i < learnts_local.size(); i++)
        ca.reloc(learnts_local[i], to);

    // All original:
    //
    int i, j;
    for (i = j = 0; i < clauses.size(); i++)
        if (ca[clauses[i]].mark() != 1) {
            ca.reloc(clauses[i], to);
            clauses[j++] = clauses[i];
        }
    clauses.shrink(i - j);
}

void Solver::garbageCollect() {
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() - ca.wasted());

    relocAll(to);
    to.moveTo(ca);
}

bool Solver::call_ls(bool use_up_build) {
    double before_init_time, after_init_time;
    if (log)
        before_init_time = cpuTime();
    ls_ready = true;

    ccnr = LS::ls();
    int ls_var_nums = nVars();
    int ls_cls_nums = nClauses() + learnts_core.size() + learnts_tier2.size();
    if (trail_lim.size() > 0)
        ls_cls_nums += trail_lim[0];
    else
        ls_cls_nums += trail.size();
    ccnr.num_vars = ls_var_nums;
    ccnr.num_clauses = ls_cls_nums;
    ccnr.max_mems = ls_mems_num;
    if (!ccnr.make_space()) {
        std::cout << "c ls solver make space error." << std::endl;
        return false;
    }

    // build_instance
    int ct = 0;
    for (int idx = 0; idx < 3; ++idx) {
        vec<CRef> &vs = (idx == 0) ? clauses : (idx == 1 ? learnts_core : (idx == 2 ? learnts_tier2 : learnts_local));
        int vs_sz = vs.size();
        for (int i = 0; i < vs_sz; i++) {
            CRef &cr = vs[i];
            Clause &c = ca[cr];
            int cls_sz = c.size();
            for (int j = 0; j < cls_sz; j++) {
                int cur_lit = toFormal(c[j]);
                ccnr.clauses[ct].literals.emplace_back(cur_lit, ct);
            }
            ct++;
        }
    }
    if (trail_lim.size() > 0) {
        int cls_sz = trail_lim[0];
        for (int i = 0; i < cls_sz; i++) {
            ccnr.clauses[ct].literals.emplace_back(toFormal(trail[i]), ct);
            ct++;
        }
    } else if (trail_lim.size() == 0) {
        int trl_sz = trail.size();
        for (int i = 0; i < trl_sz; i++) {
            ccnr.clauses[ct].literals.emplace_back(toFormal(trail[i]), ct);
            ct++;
        }
    }

    for (int c = 0; c < ccnr.num_clauses; c++) {
        for (LS::lit item : ccnr.clauses[c].literals) {
            int v = item.var_num;
            ccnr.vars[v].literals.push_back(item);
        }
    }
    ccnr.build_neighborhood();

    bool res = false;
    if (use_up_build) { // do unit propagate negalate conflicts.

        // load init_soln use UP
        int var_nums = nVars();
        int t_sz = trail.size();
        int idx = qhead;

        int viewList_sz = t_sz - qhead;
        vector<Lit> viewList(var_nums + 2);
        for (int i = qhead; i < t_sz; ++i)
            viewList[i] = trail[i];

        int undef_nums = 0;
        vector<int> undef_vars(var_nums - t_sz + 2);
        vector<int> idx_undef_vars(var_nums + 1, -1); // undef_vars' idx is not -1
        for (int i = 0; i < var_nums; ++i)
            if (value(i) == l_Undef) {
                idx_undef_vars[i] = undef_nums;
                undef_vars[undef_nums++] = i;
            } else {
                ls_mediation_soln[i] = (value(i) == l_True) ? 1 : 0;
            }

        while (undef_nums > 0) {

            while (idx < viewList_sz && undef_nums > 0) {
                Lit p = viewList[idx++];

                vec<Watcher> &ws_bin = watches_bin[p];
                int ws_bin_sz = ws_bin.size();
                for (int k = 0; k < ws_bin_sz; k++) {
                    Lit the_other = ws_bin[k].blocker;
                    Var the_other_var = var(the_other);
                    if (idx_undef_vars[the_other_var] > -1) {
                        // no conflict and can decide.
                        ls_mediation_soln[the_other_var] = sign(the_other) ? 0 : 1;
                        viewList[viewList_sz++] = the_other;

                        int end_var = undef_vars[--undef_nums];
                        int idx_end_var = idx_undef_vars[the_other_var];
                        undef_vars[idx_end_var] = end_var;
                        idx_undef_vars[end_var] = idx_end_var;
                        idx_undef_vars[the_other_var] = -1;
                    }
                }
                if (undef_nums == 0)
                    break;

                vec<Watcher> &ws = watches[p];
                Watcher *i, *j, *end;
                for (i = j = (Watcher *) ws, end = i + ws.size(); i != end;) {
                    // Make sure the false literal is data[1]:
                    CRef cr = i->cref;
                    Clause &c = ca[cr];
                    Lit false_lit = ~p;
                    if (c[0] == false_lit)
                        c[0] = c[1], c[1] = false_lit;
                    i++;

                    // If 0th watch is true, then clause is already satisfied.
                    Lit first = c[0];
                    Var first_var = var(first);
                    Watcher w = Watcher(cr, first);
                    if (idx_undef_vars[first_var] == -1 && ls_mediation_soln[first_var] == (!sign(first))) {
                        *j++ = w;
                        continue;
                    }

                    int c_sz = c.size();
                    for (int k = 2; k < c_sz; ++k) {
                        Lit tmp_lit = c[k];
                        Var tmp_var = var(tmp_lit);
                        if (idx_undef_vars[tmp_var] == -1 && ls_mediation_soln[tmp_var] == sign(tmp_lit)) {
                        } else {
                            c[1] = c[k];
                            c[k] = false_lit;
                            watches[~c[1]].push(w);
                            // next clause
                            goto check_next_clause;
                        }
                    }
                    *j++ = w;
                    if (idx_undef_vars[first_var] == -1 && ls_mediation_soln[first_var] == sign(first)) {
                        // confliction bump
                        //?need to break or go on ?
                        continue;
                    } else {
                        // unit can assign
                        ls_mediation_soln[first_var] = sign(first) ? 0 : 1;
                        viewList[viewList_sz++] = first;

                        int end_var = undef_vars[--undef_nums];
                        int idx_end_var = idx_undef_vars[first_var];
                        undef_vars[idx_end_var] = end_var;
                        idx_undef_vars[end_var] = idx_end_var;
                        idx_undef_vars[first_var] = -1;
                    }
                    check_next_clause:;
                }
                ws.shrink(i - j);
            }

            if (undef_nums == 0)
                break;

            // pick and assign
            int choosevar_idx = conflicts % undef_nums;
            Var choosevar = undef_vars[choosevar_idx];
            // Var choosevar = undef_vars[mersenne_next(&randgen, undef_nums)];
            Lit choose = mkLit(choosevar, polarity[choosevar]);

            ls_mediation_soln[choosevar] = sign(choose) ? 0 : 1;
            viewList[viewList_sz++] = choose;

            int end_var = undef_vars[--undef_nums];
            int idx_end_var = idx_undef_vars[choosevar];
            undef_vars[idx_end_var] = end_var;
            idx_undef_vars[end_var] = idx_end_var;
            idx_undef_vars[choosevar] = -1;
        }
        // call ccanr
        res = ccnr.local_search(&ls_mediation_soln);

    } else {

        // use total rand mod
        // call ccanr use rand assign
        vector<char> *rand_signal = 0;
        res = ccnr.local_search(rand_signal);
    }
    if (log) {
        after_init_time = cpuTime();
        printf("c [%3.2f] Local Search time:\t%12.2f s\n", after_init_time,after_init_time - before_init_time);
    }


    // int ls_var_nums = nVars();
    // reload mediation soln
    for (int i = 0; i < ls_var_nums; ++i) {
        ls_mediation_soln[i] = ccnr.best_solution[i + 1];
    }

    int ls_unsat_back_num = ccnr.best_found_cost;
    if (ls_unsat_back_num <= ls_best_unsat_num) {
        for (int i = 0; i < ls_var_nums; ++i)
            ls_best_soln[i] = ls_mediation_soln[i];
        ls_best_unsat_num = ls_unsat_back_num;
    }

    if (res) {
        solved_by_ls = true;
    }
    return res;
}
