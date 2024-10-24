#include "preprocess.hpp"
#include "./utils-prs/parse.hpp"
#include "utils/Logger.hpp"

preprocess::preprocess(int id_)
	: PreprocessorInterface(PreprocessorAlgorithm::MIX, id_)
	, vars(0)
	, clauses(0)
	, maxlen(0)
{
	/* Painless */
	this->maxVarCircuit = __globalParameters__.prsCircuitVar;
	this->maxVarGauss = __globalParameters__.prsGaussVar;
	this->maxVarCard = __globalParameters__.prsCardVar;

	this->maxClauseCircuit = __globalParameters__.prsCircuitCls;
	this->maxClauseSizeXor = __globalParameters__.prsGaussClsSize;
	this->maxClauseGauss = 	 __globalParameters__.prsGaussCls;
	this->maxClauseBinary =  __globalParameters__.prsBinCls;
	this->maxClauseCard =    __globalParameters__.prsCardCls;

	initializeTypeId<preprocess>();

	// this->preprocessors.push_back(std::bind(&preprocess::preprocess_circuit_wrapper, this));
	// this->preprocessors.push_back(std::bind(&preprocess::preprocess_gauss_wrapper, this));
	this->preprocessors.push_back(std::bind(&preprocess::preprocess_propagation_wrapper, this));
	this->preprocessors.push_back(std::bind(&preprocess::preprocess_card_wrapper, this));
	this->preprocessors.push_back(std::bind(&preprocess::preprocess_resolution_wrapper, this));
	this->preprocessors.push_back(std::bind(&preprocess::preprocess_binary_wrapper, this));
}

void
preprocess::preprocess_init()
{
	f = new int[vars + 10];
	val = new int[vars + 10];
	color = new int[vars + 10];
	varval = new int[vars + 10];
	q = new int[vars + 10];
	clean = new int[vars + 10];
	seen = new int[(vars << 1) + 10];
	psign = new int[vars + 10];
	psum = new int[vars + 10];
	fixed = new int[vars + 10];
	abstract = new int[clauses + 10];
	clause_delete.resize(clauses + 1, 0);
	nxtc.resize(clauses + 1, 0);
	occurp = new std::vector<int>[vars + 1];
	occurn = new std::vector<int>[vars + 1];
	for (int i = 1; i <= clauses; i++) {
		int l = clause[i].size();
		if (l > maxlen)
			maxlen = l;
	}
	resseen = new int[(vars << 1) + 10];
	a = new int[maxlen + 1];

	mapval = new int[vars + 10];
	mapto = new int[vars + 10];
	for (int i = 1; i <= vars; i++)
		mapto[i] = i, mapval[i] = 0;
}

preprocess::~preprocess()
{
	// cannot release since the arrays allocation is dependent on the result
}

void
preprocess::releaseMemory()
{
	delete[] f;
	delete[] val;
	delete[] color;
	delete[] varval;
	delete[] q;
	delete[] clean;
	delete[] seen;
	delete[] psign;
	delete[] psum;
	delete[] fixed;
	clause_delete.clear();
	nxtc.clear();
	delete[] resseen;
	delete[] a;
	delete[] mapfrom;
	delete[] abstract;
	for (int i = 0; i <= vars; i++)
		occurp[i].clear(), occurn[i].clear();
	delete[] occurp;
	delete[] occurn;
}

void
preprocess::update_var_clause_label()
{
	++this->count;
	int remain_var = 0;
	for (int i = 1; i <= vars; i++)
		color[i] = 0;
	for (int i = 1; i <= clauses; i++) {
		if (clause_delete[i]) {
			continue;
		}
		int l = clause[i].size();
		for (int j = 0; j < l; j++) {
			if (color[abs(clause[i][j])] == 0)
				color[abs(clause[i][j])] = ++remain_var;
		}
	}

	int id = 0;
	for (int i = 1; i <= clauses; i++) {
		if (clause_delete[i]) {
			clause[i].resize(0);
			continue;
		}
		++id;
		int l = clause[i].size();
		if (i == id) {
			for (int j = 0; j < l; j++)
				clause[id][j] = color[abs(clause[i][j])] * pnsign(clause[i][j]);
			continue;
		}
		clause[id].resize(0);
		for (int j = 0; j < l; j++)
			clause[id].push_back(color[abs(clause[i][j])] * pnsign(clause[i][j]));
	}
	LOGSTAT("[PRS %d] After preprocess: vars: %d -> %d , clauses: %d -> %d",
			this->getSolverId(),
			vars,
			remain_var,
			clauses,
			id);
	for (int i = id + 1; i <= clauses; i++)
		clause[i].clear();
	for (int i = remain_var + 1; i <= vars; i++)
		occurp[i].clear(), occurn[i].clear();
	clause.resize(id + 1);
	vars = remain_var, clauses = id;
}

void
preprocess::get_complete_model()
{
	int r = 0;
	for (int i = 1; i <= orivars; i++)
		if (!mapto[i]) {
			if (!mapval[i])
				;
			else if (abs(mapval[i]) != 1)
				mapval[i] = 0, ++r;
		}
	if (r) {
		occurp = new std::vector<int>[orivars + 1];
		occurn = new std::vector<int>[orivars + 1];
		for (int i = 1; i <= orivars; i++) {
			occurp[i].clear(), occurn[i].clear();
		}
		std::vector<int> clause_state;
		clause_state.resize(res_clauses + 1, 0);
		for (int i = 1; i <= res_clauses; i++) {
			int satisify = 0;
			for (int j = 0; j < res_clause[i].size(); j++) {
				int v = res_clause[i][j];
				if (v > 0)
					occurp[v].push_back(i);
				else
					occurn[-v].push_back(i);
				if (pnsign(v) * mapval[abs(v)] == 1)
					satisify = 1;
				if (!mapval[abs(v)])
					++clause_state[i];
			}
			if (satisify)
				clause_state[i] = -1;
		}
		for (int ii = resolutions; ii >= 1; ii--) {
			int v = resolution[ii];
			// attempt 1
			int assign = 1;
			for (int i = 0; i < occurn[v].size(); i++) {
				int o = occurn[v][i];
				if (clause_state[o] != -1 && clause_state[o] <= 1) {
					assign = 0;
					break;
				}
			}
			if (assign == 1) {
				mapval[v] = 1;
				for (int i = 0; i < occurn[v].size(); i++) {
					int o = occurn[v][i];
					if (clause_state[o] != -1)
						clause_state[o]--;
				}
				for (int i = 0; i < occurp[v].size(); i++)
					clause_state[occurp[v][i]] = -1;
				continue;
			}
			// attempt -1
			assign = -1;
			for (int i = 0; i < occurp[v].size(); i++) {
				int o = occurp[v][i];
				if (clause_state[o] != -1 && clause_state[o] <= 1) {
					assign = 0;
					break;
				}
			}
			if (assign == -1) {
				mapval[v] = -1;
				for (int i = 0; i < occurp[v].size(); i++) {
					int o = occurp[v][i];
					if (clause_state[o] != -1)
						clause_state[o]--;
				}
				for (int i = 0; i < occurn[v].size(); i++)
					clause_state[occurn[v][i]] = -1;
				continue;
			}
		}
		clause_state.clear();
		for (int i = 1; i <= orivars; i++) {
			occurp[i].clear(), occurn[i].clear();
		}
		delete[] occurp;
		delete[] occurn;
		res_clause.clear();
		resolution.clear();
	}
}

void
preprocess::loadFormula(const char* filename)
{
	auto startp = std::chrono::high_resolution_clock::now();
	readfile(filename, &vars, &clauses, clause);
	auto readf = std::chrono::high_resolution_clock::now();

	orivars = vars;
	oriclauses = clauses;

	LOG1("[PRS %d] Prs Parsing done in %.3lfs, orivars: %d, oriclauses: %d",
		 this->getSolverId(),
		 std::chrono::duration_cast<std::chrono::milliseconds>(readf - startp).count() / 1000.0,
		 this->orivars,
		 this->oriclauses);
}

SatResult
preprocess::solve(const std::vector<int> &cube)
{
	int res = 0;

	preprocess_init();

	// std::random_shuffle(this->preprocessors.begin(), this->preprocessors.end());

	for (auto fct : this->preprocessors) {
		res = fct();
		if (0 != res) {
			return static_cast<SatResult>(res);
		}
	}

	return static_cast<SatResult>(res);
}

std::vector<int>
preprocess::getModel()
{
	std::vector<int> model;
	for (unsigned int i = 1; i <= this->vars; i++) {
		model.push_back(this->model[i]);
	}
	return model;
}