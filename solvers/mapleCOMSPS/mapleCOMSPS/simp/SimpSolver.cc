/***********************************************************************************[SimpSolver.cc]
MiniSat -- Copyright (c) 2006,      Niklas Een, Niklas Sorensson
		   Copyright (c) 2007-2010, Niklas Sorensson

Chanseok Oh's MiniSat Patch Series -- Copyright (c) 2015, Chanseok Oh

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "../simp/SimpSolver.h"
#include "../mtl/Sort.h"
#include "../utils/System.h"
#include <m4ri/m4ri.h>
#include <set>

using namespace MapleCOMSPS;

using std::set;

typedef set<int>::const_iterator CISETIT; // Not using C++11 at the moment.

static int stat_gauss = 0, stat_gauss_case1 = 0, stat_gauss_case2 = 0, stat_gauss_bin_added = 0;
static double stat_gauss_time = 0;

static bool xors_found = false;

//=================================================================================================
// Options:

static const char* _cat = "SIMP";

static BoolOption opt_use_asymm(_cat, "asymm", "Shrink clauses by asymmetric branching.", false);
static BoolOption opt_use_rcheck(_cat, "rcheck", "Check if a clause is already implied. (costly)", false);
static BoolOption opt_use_elim(_cat, "elim", "Perform variable elimination.", true);
static IntOption opt_grow(_cat, "grow", "Allow a variable elimination step to grow by a number of clauses.", 0);
static IntOption opt_clause_lim(
	_cat,
	"cl-lim",
	"Variables are not eliminated if it produces a resolvent with a length above this limit. -1 means no limit",
	20,
	IntRange(-1, INT32_MAX));
static IntOption opt_subsumption_lim(
	_cat,
	"sub-lim",
	"Do not check if subsumption against a clause larger than this. -1 means no limit.",
	1000,
	IntRange(-1, INT32_MAX));
static DoubleOption opt_simp_garbage_frac(
	_cat,
	"simp-gc-frac",
	"The fraction of wasted memory allowed before a garbage collection is triggered during simplification.",
	0.5,
	DoubleRange(0, false, HUGE_VAL, false));

//=================================================================================================
// Constructor/Destructor:

SimpSolver::SimpSolver()
	: parsing(false)
	, grow(opt_grow)
	, clause_lim(opt_clause_lim)
	, subsumption_lim(opt_subsumption_lim)
	, simp_garbage_frac(opt_simp_garbage_frac)
	, use_asymm(opt_use_asymm)
	, use_rcheck(opt_use_rcheck)
	, use_elim(opt_use_elim)
	, merges(0)
	, asymm_lits(0)
	, eliminated_vars(0)
	, elimorder(1)
	, use_simplification(true)
	, occurs(ClauseDeleted(ca))
	, elim_heap(ElimLt(n_occ))
	, bwdsub_assigns(0)
	, n_touched(0)
{
	vec<Lit> dummy(1, lit_Undef);
	ca.extra_clause_field = true; // NOTE: must happen before allocating the dummy clause below.
	bwdsub_tmpunit = ca.alloc(dummy);
	remove_satisfied = false;
}

SimpSolver::SimpSolver(const SimpSolver& s)
	: Solver(s)
	, parsing(s.parsing)
	, grow(s.grow)
	, clause_lim(s.clause_lim)
	, subsumption_lim(s.subsumption_lim)
	, simp_garbage_frac(s.simp_garbage_frac)
	, use_asymm(s.use_asymm)
	, use_rcheck(s.use_rcheck)
	, use_elim(s.use_elim)
	, merges(s.merges)
	, asymm_lits(s.asymm_lits)
	, eliminated_vars(s.eliminated_vars)
	, elimorder(s.elimorder)
	, use_simplification(s.use_simplification)
	, occurs(ClauseDeleted(ca))
	, elim_heap(ElimLt(n_occ))
	, bwdsub_assigns(s.bwdsub_assigns)
	, n_touched(s.n_touched)
{
	vec<Lit> dummy(1, lit_Undef);
	ca.extra_clause_field = true; // NOTE: must happen before allocating the dummy
								  // clause below.
	bwdsub_tmpunit = ca.alloc(dummy);
	remove_satisfied = false;

	s.elimclauses.memCopyTo(elimclauses);
	s.touched.memCopyTo(touched);
	s.occurs.copyTo(occurs);
	s.n_occ.memCopyTo(n_occ);
	s.elim_heap.copyTo(elim_heap);
	s.subsumption_queue.copyTo(subsumption_queue);
	s.frozen.memCopyTo(frozen);
	s.eliminated.memCopyTo(eliminated);

	use_simplification = s.use_simplification;
	bwdsub_assigns = s.bwdsub_assigns;
	n_touched = s.n_touched;
	bwdsub_tmpunit = s.bwdsub_tmpunit;
	qhead = s.qhead;
	ok = s.ok;
}

SimpSolver::~SimpSolver() {}

Var
SimpSolver::newVar(bool sign, bool dvar)
{
	Var v = Solver::newVar(sign, dvar);

	frozen.push((char)false);
	eliminated.push((char)false);

	if (use_simplification) {
		n_occ.push(0);
		n_occ.push(0);
		occurs.init(v);
		touched.push(0);
		elim_heap.insert(v);
	}
	return v;
}

lbool
SimpSolver::solve_(bool do_simp, bool turn_off_simp)
{
	vec<Var> extra_frozen;
	lbool result = l_True;

	do_simp &= use_simplification;

	if (do_simp) {
		// Assumptions must be temporarily frozen to run variable elimination:
		for (int i = 0; i < assumptions.size(); i++) {
			Var v = var(assumptions[i]);

			// If an assumption has been eliminated, remember it.
			assert(!isEliminated(v));

			if (!frozen[v]) {
				// Freeze and store.
				setFrozen(v, true);
				extra_frozen.push(v);
			}
		}

		result = lbool(eliminate(turn_off_simp));
		if (strengthening)
			use_simplification = false;
	}

	if (result == l_True)
		result = Solver::solve_();
	else if (verbosity >= 1)
		printf("c ===============================================================================\n");

	if (result == l_True)
		extendModel();

	if (do_simp)
		// Unfreeze the assumptions that were frozen:
		for (int i = 0; i < extra_frozen.size(); i++)
			setFrozen(extra_frozen[i], false);

	return result;
}

bool
SimpSolver::addClause_(vec<Lit>& ps)
{
#ifndef NDEBUG
	for (int i = 0; i < ps.size(); i++)
		assert(!isEliminated(var(ps[i])));
#endif

	int nclauses = clauses.size();

	if (use_rcheck && implied(ps))
		return true;

	if (!Solver::addClause_(ps))
		return false;

	if (!parsing && drup_file) {
#ifdef BIN_DRUP
		binDRUP('a', ps, drup_file);
#else
		for (int i = 0; i < ps.size(); i++)
			fprintf(drup_file, "%i ", (var(ps[i]) + 1) * (-2 * sign(ps[i]) + 1));
		fprintf(drup_file, "0\n");
#endif
	}

	if (use_simplification && clauses.size() == nclauses + 1) {
		CRef cr = clauses.last();
		const Clause& c = ca[cr];

		// NOTE: the clause is added to the queue immediately and then
		// again during 'gatherTouchedClauses()'. If nothing happens
		// in between, it will only be checked once. Otherwise, it may
		// be checked twice unnecessarily. This is an unfortunate
		// consequence of how backward subsumption is used to mimic
		// forward subsumption.
		subsumption_queue.insert(cr);
		for (int i = 0; i < c.size(); i++) {
			occurs[var(c[i])].push(cr);
			n_occ[toInt(c[i])]++;
			touched[var(c[i])] = 1;
			n_touched++;
			if (elim_heap.inHeap(var(c[i])))
				elim_heap.increase(var(c[i]));
		}
	}

	return true;
}

void
SimpSolver::removeClause(CRef cr)
{
	const Clause& c = ca[cr];

	if (use_simplification)
		for (int i = 0; i < c.size(); i++) {
			n_occ[toInt(c[i])]--;
			updateElimHeap(var(c[i]));
			occurs.smudge(var(c[i]));
		}

	Solver::removeClause(cr);
}

bool
SimpSolver::strengthenClause(CRef cr, Lit l)
{
	Clause& c = ca[cr];
	assert(decisionLevel() == 0);
	assert(use_simplification);

	// FIX: this is too inefficient but would be nice to have (properly implemented)
	// if (!find(subsumption_queue, &c))
	subsumption_queue.insert(cr);

	if (drup_file) {
#ifdef BIN_DRUP
		binDRUP_strengthen(c, l, drup_file);
#else
		for (int i = 0; i < c.size(); i++)
			if (c[i] != l)
				fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
		fprintf(drup_file, "0\n");
#endif
	}

	if (c.size() == 2) {
		removeClause(cr);
		c.strengthen(l);
	} else {
		if (drup_file) {
#ifdef BIN_DRUP
			binDRUP('d', c, drup_file);
#else
			fprintf(drup_file, "d ");
			for (int i = 0; i < c.size(); i++)
				fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
			fprintf(drup_file, "0\n");
#endif
		}

		detachClause(cr, true);
		c.strengthen(l);
		attachClause(cr);
		remove(occurs[var(l)], cr);
		n_occ[toInt(l)]--;
		updateElimHeap(var(l));
	}

	return c.size() == 1 ? enqueue(c[0]) && propagate() == CRef_Undef : true;
}

// Returns FALSE if clause is always satisfied ('out_clause' should not be used).
bool
SimpSolver::merge(const Clause& _ps, const Clause& _qs, Var v, vec<Lit>& out_clause)
{
	merges++;
	out_clause.clear();

	bool ps_smallest = _ps.size() < _qs.size();
	const Clause& ps = ps_smallest ? _qs : _ps;
	const Clause& qs = ps_smallest ? _ps : _qs;

	for (int i = 0; i < qs.size(); i++) {
		if (var(qs[i]) != v) {
			for (int j = 0; j < ps.size(); j++)
				if (var(ps[j]) == var(qs[i]))
					if (ps[j] == ~qs[i])
						return false;
					else
						goto next;
			out_clause.push(qs[i]);
		}
	next:;
	}

	for (int i = 0; i < ps.size(); i++)
		if (var(ps[i]) != v)
			out_clause.push(ps[i]);

	return true;
}

// Returns FALSE if clause is always satisfied.
bool
SimpSolver::merge(const Clause& _ps, const Clause& _qs, Var v, int& size)
{
	merges++;

	bool ps_smallest = _ps.size() < _qs.size();
	const Clause& ps = ps_smallest ? _qs : _ps;
	const Clause& qs = ps_smallest ? _ps : _qs;
	const Lit* __ps = (const Lit*)ps;
	const Lit* __qs = (const Lit*)qs;

	size = ps.size() - 1;

	for (int i = 0; i < qs.size(); i++) {
		if (var(__qs[i]) != v) {
			for (int j = 0; j < ps.size(); j++)
				if (var(__ps[j]) == var(__qs[i]))
					if (__ps[j] == ~__qs[i])
						return false;
					else
						goto next;
			size++;
		}
	next:;
	}

	return true;
}

void
SimpSolver::gatherTouchedClauses()
{
	if (n_touched == 0)
		return;

	int i, j;
	for (i = j = 0; i < subsumption_queue.size(); i++)
		if (ca[subsumption_queue[i]].mark() == 0)
			ca[subsumption_queue[i]].mark(2);

	for (i = 0; i < touched.size(); i++)
		if (touched[i]) {
			const vec<CRef>& cs = occurs.lookup(i);
			for (j = 0; j < cs.size(); j++)
				if (ca[cs[j]].mark() == 0) {
					subsumption_queue.insert(cs[j]);
					ca[cs[j]].mark(2);
				}
			touched[i] = 0;
		}

	for (i = 0; i < subsumption_queue.size(); i++)
		if (ca[subsumption_queue[i]].mark() == 2)
			ca[subsumption_queue[i]].mark(0);

	n_touched = 0;
}

bool
SimpSolver::implied(const vec<Lit>& c)
{
	assert(decisionLevel() == 0);

	trail_lim.push(trail.size());
	for (int i = 0; i < c.size(); i++)
		if (value(c[i]) == l_True) {
			cancelUntil(0);
			return true;
		} else if (value(c[i]) != l_False) {
			assert(value(c[i]) == l_Undef);
			uncheckedEnqueue(~c[i]);
		}

	bool result = propagate() != CRef_Undef;
	cancelUntil(0);
	return result;
}

// Backward subsumption + backward subsumption resolution
bool
SimpSolver::backwardSubsumptionCheck(bool verbose)
{
	int cnt = 0;
	int subsumed = 0;
	int deleted_literals = 0;
	assert(decisionLevel() == 0);

	while (subsumption_queue.size() > 0 || bwdsub_assigns < trail.size()) {

		// Empty subsumption queue and return immediately on user-interrupt:
		if (asynch_interrupt) {
			subsumption_queue.clear();
			bwdsub_assigns = trail.size();
			break;
		}

		// Check top-level assignments by creating a dummy clause and placing it in the queue:
		if (subsumption_queue.size() == 0 && bwdsub_assigns < trail.size()) {
			Lit l = trail[bwdsub_assigns++];
			ca[bwdsub_tmpunit][0] = l;
			ca[bwdsub_tmpunit].calcAbstraction();
			subsumption_queue.insert(bwdsub_tmpunit);
		}

		CRef cr = subsumption_queue.peek();
		subsumption_queue.pop();
		Clause& c = ca[cr];

		if (c.mark())
			continue;

		if (verbose && verbosity >= 2 && cnt++ % 1000 == 0)
			printf("c subsumption left: %10d (%10d subsumed, %10d deleted literals)\r",
				   subsumption_queue.size(),
				   subsumed,
				   deleted_literals);

		assert(c.size() > 1 || value(c[0]) == l_True); // Unit-clauses should have been propagated before this point.

		// Find best variable to scan:
		Var best = var(c[0]);
		for (int i = 1; i < c.size(); i++)
			if (occurs[var(c[i])].size() < occurs[best].size())
				best = var(c[i]);

		// Search all candidates:
		vec<CRef>& _cs = occurs.lookup(best);
		CRef* cs = (CRef*)_cs;

		for (int j = 0; j < _cs.size(); j++)
			if (c.mark())
				break;
			else if (!ca[cs[j]].mark() && cs[j] != cr &&
					 (subsumption_lim == -1 || ca[cs[j]].size() < subsumption_lim)) {
				Lit l = c.subsumes(ca[cs[j]]);

				if (l == lit_Undef)
					subsumed++, removeClause(cs[j]);
				else if (l != lit_Error) {
					deleted_literals++;

					if (!strengthenClause(cs[j], ~l))
						return false;

					// Did current candidate get deleted from cs? Then check candidate at index j again:
					if (var(l) == best)
						j--;
				}
			}
	}

	return true;
}

bool
SimpSolver::asymm(Var v, CRef cr)
{
	Clause& c = ca[cr];
	assert(decisionLevel() == 0);

	if (c.mark() || satisfied(c))
		return true;

	trail_lim.push(trail.size());
	Lit l = lit_Undef;
	for (int i = 0; i < c.size(); i++)
		if (var(c[i]) != v) {
			if (value(c[i]) != l_False)
				uncheckedEnqueue(~c[i]);
		} else
			l = c[i];

	if (propagate() != CRef_Undef) {
		cancelUntil(0);
		asymm_lits++;
		if (!strengthenClause(cr, l))
			return false;
	} else
		cancelUntil(0);

	return true;
}

bool
SimpSolver::asymmVar(Var v)
{
	assert(use_simplification);

	const vec<CRef>& cls = occurs.lookup(v);

	if (value(v) != l_Undef || cls.size() == 0)
		return true;

	for (int i = 0; i < cls.size(); i++)
		if (!asymm(v, cls[i]))
			return false;

	return backwardSubsumptionCheck();
}

static void
mkElimClause(vec<uint32_t>& elimclauses, Lit x)
{
	elimclauses.push(toInt(x));
	elimclauses.push(1);
}

static void
mkElimClause(vec<uint32_t>& elimclauses, Var v, Clause& c)
{
	int first = elimclauses.size();
	int v_pos = -1;

	// Copy clause to elimclauses-vector. Remember position where the
	// variable 'v' occurs:
	for (int i = 0; i < c.size(); i++) {
		elimclauses.push(toInt(c[i]));
		if (var(c[i]) == v)
			v_pos = i + first;
	}
	assert(v_pos != -1);

	// Swap the first literal with the 'v' literal, so that the literal
	// containing 'v' will occur first in the clause:
	uint32_t tmp = elimclauses[v_pos];
	elimclauses[v_pos] = elimclauses[first];
	elimclauses[first] = tmp;

	// Store the length of the clause last:
	elimclauses.push(c.size());
}

bool
SimpSolver::eliminateVar(Var v)
{
	assert(!frozen[v]);
	assert(!isEliminated(v));
	assert(value(v) == l_Undef);

	// Split the occurrences into positive and negative:
	//
	const vec<CRef>& cls = occurs.lookup(v);
	vec<CRef> pos, neg;
	for (int i = 0; i < cls.size(); i++)
		(find(ca[cls[i]], mkLit(v)) ? pos : neg).push(cls[i]);

	// Check wether the increase in number of clauses stays within the allowed ('grow'). Moreover, no
	// clause must exceed the limit on the maximal clause size (if it is set):
	//
	int cnt = 0;
	int clause_size = 0;

	for (int i = 0; i < pos.size(); i++)
		for (int j = 0; j < neg.size(); j++)
			if (merge(ca[pos[i]], ca[neg[j]], v, clause_size) &&
				(++cnt > cls.size() + grow || (clause_lim != -1 && clause_size > clause_lim)))
				return true;

	// Delete and store old clauses:
	eliminated[v] = true;
	setDecisionVar(v, false);
	eliminated_vars++;

	if (pos.size() > neg.size()) {
		for (int i = 0; i < neg.size(); i++)
			mkElimClause(elimclauses, v, ca[neg[i]]);
		mkElimClause(elimclauses, mkLit(v));
	} else {
		for (int i = 0; i < pos.size(); i++)
			mkElimClause(elimclauses, v, ca[pos[i]]);
		mkElimClause(elimclauses, ~mkLit(v));
	}

	// Produce clauses in cross product:
	vec<Lit>& resolvent = add_tmp;
	for (int i = 0; i < pos.size(); i++)
		for (int j = 0; j < neg.size(); j++)
			if (merge(ca[pos[i]], ca[neg[j]], v, resolvent) && !addClause_(resolvent))
				return false;

	for (int i = 0; i < cls.size(); i++)
		removeClause(cls[i]);

	// Free occurs list for this variable:
	occurs[v].clear(true);

	// Free watchers lists for this variable, if possible:
	watches_bin[mkLit(v)].clear(true);
	watches_bin[~mkLit(v)].clear(true);
	watches[mkLit(v)].clear(true);
	watches[~mkLit(v)].clear(true);

	return backwardSubsumptionCheck();
}

bool
SimpSolver::substitute(Var v, Lit x)
{
	assert(!frozen[v]);
	assert(!isEliminated(v));
	assert(value(v) == l_Undef);

	if (!ok)
		return false;

	eliminated[v] = true;
	setDecisionVar(v, false);
	const vec<CRef>& cls = occurs.lookup(v);

	vec<Lit>& subst_clause = add_tmp;
	for (int i = 0; i < cls.size(); i++) {
		Clause& c = ca[cls[i]];

		subst_clause.clear();
		for (int j = 0; j < c.size(); j++) {
			Lit p = c[j];
			subst_clause.push(var(p) == v ? x ^ sign(p) : p);
		}

		if (!addClause_(subst_clause))
			return ok = false;

		removeClause(cls[i]);
	}

	return true;
}

void
SimpSolver::extendModel()
{
	int i, j;
	Lit x;

	for (i = elimclauses.size() - 1; i > 0; i -= j) {
		for (j = elimclauses[i--]; j > 1; j--, i--)
			if (modelValue(toLit(elimclauses[i])) != l_False)
				goto next;

		x = toLit(elimclauses[i]);
		model[var(x)] = lbool(!sign(x));
	next:;
	}
}

// Almost duplicate of Solver::removeSatisfied. Didn't want to make the base method 'virtual'.
void
SimpSolver::removeSatisfied()
{
	int i, j;
	for (i = j = 0; i < clauses.size(); i++) {
		const Clause& c = ca[clauses[i]];
		if (c.mark() == 0)
			if (satisfied(c))
				removeClause(clauses[i]);
			else
				clauses[j++] = clauses[i];
	}
	clauses.shrink(i - j);
}

// The technique and code are by the courtesy of the GlueMiniSat team. Thank you!
// It helps solving certain types of huge problems tremendously.
bool
SimpSolver::eliminate(bool turn_off_elim)
{
	bool res = true;
	int iter = 0;
	int n_cls, n_cls_init, n_vars;

	if (nVars() == 0 || !use_simplification) {
		if (!simplify())
			return false;
		goto cleanup;
	}

	if (GE) {
		res = gaussElim();
		if (!res)
			goto cleanup;
	}

	// Get an initial number of clauses (more accurately).
	if (trail.size() != 0)
		removeSatisfied();
	n_cls_init = nClauses();

	res = eliminate_(); // The first, usual variable elimination of MiniSat.
	if (!res)
		goto cleanup;

	n_cls = nClauses();
	n_vars = nFreeVars();

	// printf("c Reduced to %d vars, %d cls (grow=%d)\n", n_vars, n_cls, grow);

	if (!xors_found && GE) { // Second chance to do Gaussian elimination.
		res = gaussElim();
		if (!res)
			goto cleanup;
	}

	if ((double)n_cls / n_vars >= 10 || n_vars < 10000) {
		// printf("c No iterative elimination performed. (vars=%d, c/v ratio=%.1f)\n", n_vars, (double)n_cls / n_vars);
		goto cleanup;
	}

	grow = grow ? grow * 2 : 8;
	for (; grow < 10000; grow *= 2) {
		// Rebuild elimination variable heap.
		for (int i = 0; i < clauses.size(); i++) {
			const Clause& c = ca[clauses[i]];
			for (int j = 0; j < c.size(); j++)
				if (!elim_heap.inHeap(var(c[j])))
					elim_heap.insert(var(c[j]));
				else
					elim_heap.update(var(c[j]));
		}

		int n_cls_last = nClauses();
		int n_vars_last = nFreeVars();

		res = eliminate_();
		if (!res || n_vars_last == nFreeVars())
			break;
		iter++;

		int n_cls_now = nClauses();
		int n_vars_now = nFreeVars();

		double cl_inc_rate = (double)n_cls_now / n_cls_last;
		double var_dec_rate = (double)n_vars_last / n_vars_now;

		// printf("c Reduced to %d vars, %d cls (grow=%d)\n", n_vars_now, n_cls_now, grow);
		// printf("c cl_inc_rate=%.3f, var_dec_rate=%.3f\n", cl_inc_rate, var_dec_rate);

		if (n_cls_now > n_cls_init || cl_inc_rate > var_dec_rate)
			break;
	}
	// printf("c No. effective iterative eliminations: %d\n", iter);

cleanup:
	// printf("c [GE] total time: %.2f\n", stat_gauss_time);

	touched.clear(true);
	occurs.clear(true);
	n_occ.clear(true);
	elim_heap.clear(true);
	subsumption_queue.clear(true);

	use_simplification = false;
	remove_satisfied = true;
	ca.extra_clause_field = false;

	// Force full cleanup (this is safe and desirable since it only happens once):
	rebuildOrderHeap();
	garbageCollect();

	return res;
}

bool
SimpSolver::eliminate_()
{
	if (!simplify())
		return false;
	else if (!use_simplification)
		return true;

	int trail_size_last = trail.size();

	// Main simplification loop:
	//
	while (n_touched > 0 || bwdsub_assigns < trail.size() || elim_heap.size() > 0) {

		gatherTouchedClauses();
		// printf("  ## (time = %6.2f s) BWD-SUB: queue = %d, trail = %d\n", cpuTime(), subsumption_queue.size(),
		// trail.size() - bwdsub_assigns);
		if ((subsumption_queue.size() > 0 || bwdsub_assigns < trail.size()) && !backwardSubsumptionCheck(true)) {
			ok = false;
			goto cleanup;
		}

		// Empty elim_heap and return immediately on user-interrupt:
		if (asynch_interrupt) {
			assert(bwdsub_assigns == trail.size());
			assert(subsumption_queue.size() == 0);
			assert(n_touched == 0);
			elim_heap.clear();
			goto cleanup;
		}

		// printf("  ## (time = %6.2f s) ELIM: vars = %d\n", cpuTime(), elim_heap.size());
		for (int cnt = 0; !elim_heap.empty(); cnt++) {
			Var elim = elim_heap.removeMin();

			if (asynch_interrupt)
				break;

			if (isEliminated(elim) || value(elim) != l_Undef)
				continue;

			if (verbosity >= 2 && cnt % 100 == 0)
				printf("c elimination left: %10d\r", elim_heap.size());

			if (use_asymm) {
				// Temporarily freeze variable. Otherwise, it would immediately end up on the queue again:
				bool was_frozen = frozen[elim];
				frozen[elim] = true;
				if (!asymmVar(elim)) {
					ok = false;
					goto cleanup;
				}
				frozen[elim] = was_frozen;
			}

			// At this point, the variable may have been set by assymetric branching, so check it
			// again. Also, don't eliminate frozen variables:
			if (use_elim && value(elim) == l_Undef && !frozen[elim] && !eliminateVar(elim)) {
				ok = false;
				goto cleanup;
			}

			checkGarbage(simp_garbage_frac);
		}

		assert(subsumption_queue.size() == 0);
	}
cleanup:
	// To get an accurate number of clauses.
	if (trail_size_last != trail.size())
		removeSatisfied();
	else {
		int i, j;
		for (i = j = 0; i < clauses.size(); i++)
			if (ca[clauses[i]].mark() == 0)
				clauses[j++] = clauses[i];
		clauses.shrink(i - j);
	}
	checkGarbage();

	if (verbosity >= 1 && elimclauses.size() > 0)
		printf("c |  Eliminated clauses:     %10.2f Mb                                      |\n",
			   double(elimclauses.size() * sizeof(uint32_t)) / (1024 * 1024));

	return ok;
}

//=================================================================================================
// Garbage Collection methods:

void
SimpSolver::relocAll(ClauseAllocator& to)
{
	if (!use_simplification)
		return;

	// All occurs lists:
	//
	occurs.cleanAll();
	for (int i = 0; i < nVars(); i++) {
		vec<CRef>& cs = occurs[i];
		for (int j = 0; j < cs.size(); j++)
			ca.reloc(cs[j], to);
	}

	// Subsumption queue:
	//
	for (int i = 0; i < subsumption_queue.size(); i++)
		ca.reloc(subsumption_queue[i], to);

	// Temporary clause:
	//
	ca.reloc(bwdsub_tmpunit, to);
}

void
SimpSolver::garbageCollect()
{
	// Initialize the next region to a size corresponding to the estimated utilization degree. This
	// is not precise but should avoid some unnecessary reallocations for the new region:
	ClauseAllocator to(ca.size() - ca.wasted());

	to.extra_clause_field = ca.extra_clause_field; // NOTE: this is important to keep (or lose) the extra fields.
	relocAll(to);
	Solver::relocAll(to);
	if (verbosity >= 2)
		printf("c |  Garbage collection:   %12d bytes => %12d bytes             |\n",
			   ca.size() * ClauseAllocator::Unit_Size,
			   to.size() * ClauseAllocator::Unit_Size);
	to.moveTo(ca);
}

#ifndef NDEBUG
static bool
sanityCheck(const vec<XorScc*>& xor_sccs, const vec<Var>& v2scc_id, const vec<vec<Var>>& var_sccs, int nVars)
{
	int n_sccs = 0;
	vec<char> seen_id(var_sccs.size(), 0), seen_v(nVars, 0), seen_v2(nVars, 0);
	assert(v2scc_id.size() == nVars);
	for (int i = 0; i < v2scc_id.size(); i++) {
		int id = v2scc_id[i];
		if (id != -1) {
			if (!seen_id[id])
				n_sccs++;
			seen_v[i] = seen_id[id] = 1;
		}
	}
	for (int i = 0; i < xor_sccs.size(); i++) {
		n_sccs--;
		assert(xor_sccs[i]);
		assert(xor_sccs[i]->vars.size() > 2);
		assert(xor_sccs[i]->xors.size() != 0);
		for (int j = 0; j < xor_sccs[i]->xors.size(); j++) {
			const Xor& x = *xor_sccs[i]->xors[j];
			for (int k = 0; k < x.size(); k++) {
				assert(seen_v[x[k]]);
				seen_v2[x[k]] = 1;
			}
		}
	}
	for (int i = 0; i < seen_v2.size(); i++)
		if (seen_v2[i])
			assert(v2scc_id[i] != -1);
		else
			assert(v2scc_id[i] == -1);
	assert(n_sccs == 0);
	for (int i = 0; i < var_sccs.size(); i++)
		assert(var_sccs[i].size() == 0);
	return true;
}
#endif

class Stopwatch
{
	double init, ticked;

  public:
	Stopwatch() { init = ticked = cpuTime(); }
	double total() { return ticked - init; }
	double tick()
	{
		double now = cpuTime();
		double dur = now - ticked;
		ticked = now;
		return dur;
	}
};

bool
SimpSolver::gaussElim()
{
	assert(decisionLevel() == 0);
	if (drup_file)
		return true;

	vec<Xor*> xors;			// XORs found by "searchXors()".
	vec<vec<Var>> var_sccs; // SCCs in terms of vars
	vec<Var> v2scc_id;		// var --> SCC ID map
	vec<XorScc*> xor_sccs;	// SCCs in terms of XORs
	Stopwatch timer;

	xors_found = searchXors(xors);
	// printf("c [GE] XORs: %d (time: %.2f)\n", xors.size(), timer.tick());

	int upper_limit = computeVarSccs(v2scc_id, var_sccs, xors);
	computeXorSccs(xor_sccs, xors, v2scc_id, var_sccs, upper_limit);
	// printf("c [GE] XOR SCCs: %d (time: %.2f)\n", xor_sccs.size(), timer.tick());

	ok = performGaussElim(xor_sccs);

	for (int i = 0; i < xors.size(); i++)
		delete xors[i];
	for (int i = 0; i < xor_sccs.size(); i++)
		delete xor_sccs[i];

	// printf("c [GE] matrices: %d, unary xor: %d, bin xor: %d, bin added: %d (time: %.2f)\n",
	//     stat_gauss, stat_gauss_case1, stat_gauss_case2, stat_gauss_bin_added, timer.tick());
	stat_gauss_time += timer.total();
	if (!ok)
		printf("c [GE] UNSAT\n");

	return ok;
}

// TODO: might not worth checking dup; probably dups are very rare.
void
SimpSolver::addBinNoDup(Lit p, Lit q)
{
	watches_bin.cleanAll();
	const vec<Watcher>& ws_p = watches_bin[~p];
	const vec<Watcher>& ws_q = watches_bin[~q];

	if (ws_p.size() != 0 && ws_q.size() != 0) {
		const vec<Watcher>& ws = ws_p.size() < ws_q.size() ? ws_p : ws_q;
		Lit the_other = ws_p.size() < ws_q.size() ? q : p;
		for (int i = 0; i < ws.size(); i++)
			if (ws[i].blocker == the_other)
				return;
	} // A dup exists; skip.

	add_tmp.clear();
	add_tmp.push(p);
	add_tmp.push(q);
	addClause_(add_tmp);
	stat_gauss_bin_added++;
}

bool
SimpSolver::performGaussElim(vec<XorScc*>& xor_sccs)
{
	vec<int> v2mzd_v(nVars(), -1); // CNF var --> matrix var
	vec<Var> mzd_v2v;			   // matrix var --> CNF var

	for (int i = 0; i < xor_sccs.size(); i++) {
		XorScc& scc = *xor_sccs[i];
		if (scc.xors.size() == 1)
			continue;
		assert(scc.vars.size() > 3);
		assert(scc.xors.size() > 1);
		// printf("c     SCC %d: %d XORs, %d vars\n", i+1, scc.xors.size(), scc.vars.size());
		if (((uint64_t)scc.vars.size()) * scc.xors.size() > 10000000ULL)
			continue;

		mzd_v2v.clear();
		sort(scc.vars);
		for (int j = 0; j < scc.vars.size(); j++) { // Set up CNF var <--> matrix var mapping
			v2mzd_v[scc.vars[j]] = j;
			mzd_v2v.push(scc.vars[j]);
		}

		// Create and fill a matrix.
		int cols = scc.vars.size() + 1 /*rhs*/;
		mzd_t* mat = mzd_init(scc.xors.size(), cols);
		assert(mzd_is_zero(mat));
		for (int row = 0; row < scc.xors.size(); row++) {
			const Xor& x = *scc.xors[row];
			for (int k = 0; k < x.size(); k++) {
				assert(v2mzd_v[x[k]] < cols - 1);
				mzd_write_bit(mat, row, v2mzd_v[x[k]], 1);
			}
			if (x.rhs)
				mzd_write_bit(mat, row, cols - 1, 1);
		}

		stat_gauss++;
		mzd_echelonize(mat, true);

		// Examine the result.
		for (int row = 0, rhs; row < scc.xors.size(); row++) {
			vec<Var> ones;
			for (int col = 0; col < cols - 1; col++)
				if (mzd_read_bit(mat, row, col)) {
					if (ones.size() == 2)
						goto NextRow; // More than two columns have 1; give up.
					ones.push(mzd_v2v[col]);
				}

			rhs = mzd_read_bit(mat, row, cols - 1);
			if (ones.size() == 1) {
				stat_gauss_case1++;
				uncheckedEnqueue(mkLit(ones[0], !rhs));
			} else if (ones.size() == 2) { // If so, we may add two binary clauses.
				stat_gauss_case2++;

				// x + y = 1  -->       same signs, i.e., ( x v y) ^ (~x v ~y)
				// x + y = 0  -->  different signs, i.e., (~x v y) ^ ( x v ~y)
				Lit p = mkLit(ones[0], false);
				Lit q = mkLit(ones[1], !rhs);
				addBinNoDup(p, q);
				addBinNoDup(~p, ~q);
			} else // empty case
				if (rhs)
					return false; // 0 = 1, i.e., UNSAT
		NextRow:;
		}

		mzd_free(mat);
	}

	return propagate() == CRef_Undef;
}

inline static void
sortAfterCopy(const Clause& c, vec<Lit>& out)
{
	out.clear();
	for (int i = 0; i < c.size(); i++)
		out.push(c[i]);
	sort(out);
}

int
SimpSolver::toDupMarkerIdx(const Clause& c)
{
	sortAfterCopy(c, add_tmp);

	int idx = 0;
	for (int i = 0; i < add_tmp.size(); i++)
		if (sign(add_tmp[i]))
			idx |= (1 << i);
	return idx;
}

class SeenMarker
{
	const Clause& c;
	vec<char>& seen;

  public:
	SeenMarker(const Clause& c_, vec<char>& seen_)
		: c(c_)
		, seen(seen_)
	{
		for (int i = 0; i < c.size(); i++)
			seen[var(c[i])] = 1;
	}
	~SeenMarker()
	{
		for (int i = 0; i < c.size(); i++)
			seen[var(c[i])] = 0;
	}
};

class ClauseMarker
{ // Will break if garbage collection happens before destruction.
	ClauseAllocator& ca;
	vec<CRef> to_clean;

  public:
	ClauseMarker(ClauseAllocator& ca_)
		: ca(ca_)
	{
	}
	~ClauseMarker()
	{
		for (int i = 0; i < to_clean.size(); i++) {
			assert(ca[to_clean[i]].mark() == 3);
			ca[to_clean[i]].mark(0);
		}
	}

	inline void mark3(CRef cr)
	{
		assert(ca[cr].mark() == 0);
		ca[cr].mark(3);
		to_clean.push(cr);
	}
};

#define MAX_XOR_SIZE_LIMIT 6
bool
SimpSolver::searchXors(vec<Xor*>& /*out*/ xors)
{
	assert(xors.size() == 0);
	vec<CRef> pool;
	vec<bool> dup_table;
	ClauseMarker cl_marker(ca);

	for (int i = 0; i < clauses.size(); i++) {
		Clause& c = ca[clauses[i]];
		assert(c.mark() == 0 || c.mark() == 3);

		if (c.mark() == 3)
			continue;
		cl_marker.mark3(clauses[i]);

		if (c.size() == 2 || c.size() > MAX_XOR_SIZE_LIMIT)
			continue;

		// Find a var with a min-size occurrences list.
		Var min_occ_v = var_Undef;
		int min_occ = INT_MAX;
		const int required_h = 1 << (c.size() - 2); // half of #required clauses to form a single XOR
		bool skip = false;
		for (int j = 0; j < c.size(); j++) {
			int n_occ_p = n_occ[toInt(c[j])];
			int n_occ_not_p = n_occ[toInt(~c[j])];
			if (n_occ_p < required_h || n_occ_not_p < required_h) {
				skip = true;
				break;
			}

			int occ = n_occ_p + n_occ_not_p;
			if (occ < min_occ) {
				min_occ = occ;
				min_occ_v = var(c[j]);
			}
		}
		if (skip)
			continue; // Short-circuit if not enough clauses to form an XOR.

		// Gather classes to look into.
		pool.clear();
		pool.push(clauses[i]);
		vec<CRef>& occs = occurs.lookup(min_occ_v);
		for (int j = 0; j < occs.size(); j++) {
			const Clause& c2 = ca[occs[j]];
			assert(c2.mark() == 0 || c2.mark() == 3);

			if (c2.mark() == 0 && c2.size() == c.size() && c2.abstraction() == c.abstraction())
				pool.push(occs[j]);
		}
		if (pool.size() < 2 * required_h)
			continue; // Short-circuit if not enough clauses.

		// We'll consider only clauses with the exact same set of vars.
		SeenMarker seen_marker(c, seen);

		dup_table.clear();
		dup_table.growTo(1 << MAX_XOR_SIZE_LIMIT, false);
		int rhs[2] = { 0, 0 }, idx;
		for (int j = 0; j < pool.size(); j++) {
			Clause& c2 = ca[pool[j]];
			assert(c2.size() == c.size());
			assert(j == 0 || c2.mark() == 0);
			assert(j != 0 || c2.mark() == 3);
			bool even_negs = true;
			for (int k = 0; k < c2.size(); k++) {
				if (!seen[var(c2[k])])
					goto Next;
				if (sign(c2[k]))
					even_negs = !even_negs;
			}

			// CNF might have duplicate clauses. (Typically unusual, but to be safe.)
			idx = toDupMarkerIdx(c2);
			if (dup_table[idx])
				continue;
			dup_table[idx] = true;

			rhs[even_negs]++;

			if (j != 0)
				cl_marker.mark3(pool[j]);
		Next:;
		}

		assert(rhs[0] <= 2 * required_h);
		assert(rhs[1] <= 2 * required_h);
		if (rhs[0] == 2 * required_h)
			xors.push(new Xor(c, 0 /*rhs*/));
		if (rhs[1] == 2 * required_h)
			xors.push(new Xor(c, 1 /*rhs*/));
	}

	return xors.size() != 0;
}

struct SizeDec
{
	bool operator()(const Xor* a, const Xor* b) const { return a->size() > b->size(); }
};

int
SimpSolver::computeVarSccs(vec<Var>& /*out*/ scc_id, vec<vec<Var>>& /*out*/ sccs, vec<Xor*>& xors) const
{
	assert(scc_id.size() == 0);
	assert(sccs.size() == 0);
	/*
		Commenting out: why bother removing solitary XORs (XOR not connected to any XORs)?
		This case will naturally be taken care of when computing SCCs.

		vec<bool> seen(nVars(), 0);
		for (int i = 0; i < xors.size(); i++)
			for (int j = 0; j < x.size(); j++)
				seen[x[j]]++;

		for (int i = j = 0; i < xors.size(); i++){
			vec<int>& x = *xors[i];
			bool solitary = true;
			for (int j = 0; j < x.size(); j++){
				if (seen[x[j]] != 1){
					solitary = false; break; }}

			if (solitary) xors[i];
			else xors[j++] = xors[i];
		}
		xors.shrink(i - j);
	*/
	scc_id.growTo(nVars(), -1); // var --> SSC ID map

	set<int> x_ids;
	sort(xors, SizeDec()); // optimization
	for (int i = 0; i < xors.size(); i++) {
		assert(xors[i]);
		const Xor& x = *xors[i];
		assert(x.size() > 2);

		x_ids.clear();
		for (int j = 0; j < x.size(); j++)
			if (scc_id[x[j]] != -1)
				x_ids.insert(scc_id[x[j]]);

		if (x_ids.empty()) { // No mapping exists for all the vars. Create a new SCC.
			sccs.push();
			for (int j = 0; j < x.size(); j++) {
				assert(x[j] < scc_id.size());
				assert(scc_id[x[j]] == -1);
				scc_id[x[j]] = sccs.size() - 1;
				assert(scc_id[x[j]] >= 0);
				sccs.last().push(x[j]);
			}
			assert(sccs.last().size() == x.size());
		} else if (x_ids.size() == 1) { // Not really necessary but for optimization.
			int id = *x_ids.begin();
			assert(id != -1);
			assert(id < sccs.size());
			for (int j = 0; j < x.size(); j++) {
				assert(x[j] < scc_id.size());
				if (scc_id[x[j]] == -1) {
					scc_id[x[j]] = id;
					sccs[id].push(x[j]);
				} else
					assert(scc_id[x[j]] == id);
			}
		} else {
			// Identify the largest SCC, into which we'll merge smaller SCCs.
			// (Not sure if it's worth doing this optimization though.)
			int id_max = -1;
			int sz_max = 0;
			for (CISETIT it = x_ids.begin(); it != x_ids.end(); it++)
				if (sccs[*it].size() > sz_max)
					sz_max = sccs[id_max = *it].size();

			// Now merge smaller SCCs into it.
			for (CISETIT it = x_ids.begin(); it != x_ids.end(); it++)
				if (*it != id_max) {
					vec<Var>& vars = sccs[*it];
					for (int j = 0; j < vars.size(); j++) {
						assert(scc_id[vars[j]] == *it);
						scc_id[vars[j]] = id_max;
						sccs[id_max].push(vars[j]);
					}
					vars.clear();
					assert(sccs[*it].size() == 0);
				}

			// Take care of the remaining vars not yet associated with any SCC.
			for (int j = 0; j < x.size(); j++)
				if (scc_id[x[j]] == -1) {
					scc_id[x[j]] = id_max;
					sccs[id_max].push(x[j]);
				}
		}
	}

	return sccs.size(); // Upper limit, as there are merged (empty) components.
}

void
SimpSolver::computeXorSccs(vec<XorScc*>& /*out*/ xor_sccs,
						   const vec<Xor*>& xors,
						   const vec<Var>& v2scc_id,
						   vec<vec<Var>>& var_sccs,
						   int upper_limit) const
{
	assert(xor_sccs.size() == 0);
	xor_sccs.growTo(upper_limit, NULL);

	for (int i = 0; i < xors.size(); i++) {
		const Xor& x = *xors[i];
		assert(x.size() > 2);
		int id = v2scc_id[x[0]];
		assert(id != -1);
		assert(id < xor_sccs.size());

		XorScc* scc = xor_sccs[id];
		if (scc == NULL)
			xor_sccs[id] = scc = new XorScc;
		scc->xors.push(xors[i]);
		if (scc->vars.size() == 0) {
			assert(var_sccs[id].size() != 0);
			var_sccs[id].moveTo(scc->vars);
		} else
			assert(var_sccs[id].size() == 0);
	}

	// Compact the data structure.
	int i, j;
	for (i = j = 0; i < xor_sccs.size(); i++)
		if (xor_sccs[i] != NULL) {
			assert(xor_sccs[i]->xors.size() != 0);
			assert(xor_sccs[i]->vars.size() > 2);
			xor_sccs[j++] = xor_sccs[i];
		}
	xor_sccs.shrink(i - j);
	assert(sanityCheck(xor_sccs, v2scc_id, var_sccs, nVars()));
}
