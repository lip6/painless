#include "preprocess.hpp"
#include <algorithm>
#include <cmath>

int
preprocess::search_almost_one()
{
	HashMap* C = new HashMap();
	nlit = 2 * vars + 2;
	occur = new std::vector<int>[nlit];
	for (int i = 1; i <= clauses; i++) {
		clause_delete[i] = 0;
		if (clause[i].size() != 2)
			continue;
		int x = tolit(clause[i][0]);
		int y = tolit(clause[i][1]);
		ll id1 = mapv(x, y);
		ll id2 = mapv(y, x);
		C->insert(id1, i);
		C->insert(id2, i);
		occur[x].push_back(y);
		occur[y].push_back(x);
	}
	for (int i = 1; i <= vars; i++)
		seen[tolit(i)] = seen[tolit(-i)] = 0;
	int t = 0;
	std::vector<int> ino, nei;
	for (int i = 0; i < vars * 2; i++) {
		if (seen[i] || !occur[i].size())
			continue;
		seen[i] = 1;
		nei.clear();
		for (int j = 0; j < occur[i].size(); j++)
			if (!seen[occur[i][j]])
				nei.push_back(occur[i][j]);
		do {
			ino.clear();
			ino.push_back(i);
			for (int j = 0; j < nei.size(); j++) {
				int v = nei[j], flag = 1;
				for (int k = 0; k < ino.size(); k++) {
					ll id = mapv(v, ino[k]);
					int d1 = C->get(id, 0);
					if (!d1) {
						flag = 0;
						break;
					}
					q[k] = d1;
				}
				if (flag) {
					for (int k = 0; k < ino.size(); k++) {
						clause_delete[q[k]] = 1;
						ll id1 = mapv(v, ino[k]), id2 = mapv(ino[k], v);
						C->erase(id1);
						C->erase(id2);
					}
					ino.push_back(v);
				}
			}
			if (ino.size() >= 2) {
				card_one.emplace_back();
				for (int j = 0; j < ino.size(); j++) {
					card_one[card_one.size() - 1].push_back(-toeidx(ino[j]));
				}
			}
		} while (ino.size() != 1);
	}
	for (int i = 0; i < nlit; i++)
		occur[i].clear();
	delete[] occur;
	delete C;
	return card_one.size();
}

void
preprocess::upd_occur(int v, int s)
{
	int x = abs(v);
	int t = 0;
	if (v > 0) {
		for (int j = 0; j < occurp[x].size(); j++)
			if (occurp[x][j] != s)
				occurp[x][t++] = occurp[x][j];
		// assert(t == occurp[x].size() - 1);
		occurp[x].resize(t);
	} else {
		for (int j = 0; j < occurn[x].size(); j++)
			if (occurn[x][j] != s)
				occurn[x][t++] = occurn[x][j];
		// assert(t == occurn[x].size() - 1);
		occurn[x].resize(t);
	}
}

int
preprocess::scc_almost_one()
{
	for (int i = 1; i <= vars; i++) {
		occurp[i].clear();
		occurn[i].clear();
	}
	cdel.resize(card_one.size(), 0);
	for (int i = 0; i < card_one.size(); i++) {
		for (int j = 0; j < card_one[i].size(); j++)
			if (card_one[i][j] > 0)
				occurp[card_one[i][j]].push_back(i);
			else
				occurn[-card_one[i][j]].push_back(i);
	}
	int flag = 1;
	do {
		flag = 0;
		for (int i = 1; i <= vars; i++) {
			if (!occurp[i].size() || !occurn[i].size())
				continue;
			if (card_one.size() + occurp[i].size() * occurp[i].size() > 1e7 / vars)
				return 0;
			flag = 1;
			for (int ip = 0; ip < occurp[i].size(); ip++)
				cdel[occurp[i][ip]] = 1;
			for (int in = 0; in < occurn[i].size(); in++)
				cdel[occurn[i][in]] = 1;
			for (int ip = 0; ip < occurp[i].size(); ip++) {
				int op = occurp[i][ip];
				for (int in = 0; in < occurn[i].size(); in++) {
					int on = occurn[i][in];
					card_one.emplace_back();
					cdel.push_back(0);
					int id = card_one.size() - 1;
					for (int j = 0; j < card_one[op].size(); j++) {
						int v = card_one[op][j];
						if (abs(v) != i) {
							card_one[id].push_back(v);
							if (v > 0)
								occurp[v].push_back(id);
							else
								occurn[-v].push_back(id);
						}
					}
					for (int j = 0; j < card_one[on].size(); j++) {
						int v = card_one[on][j];
						if (abs(v) != i) {
							card_one[id].push_back(v);
							if (v > 0)
								occurp[v].push_back(id);
							else
								occurn[-v].push_back(id);
						}
					}
				}
			}
			for (int ip = 0; ip < occurp[i].size(); ip++) {
				int op = occurp[i][ip];
				for (int j = 0; j < card_one[op].size(); j++)
					upd_occur(card_one[op][j], op);
			}

			for (int in = 0; in < occurn[i].size(); in++) {
				int on = occurn[i][in];
				for (int j = 0; j < card_one[on].size(); j++)
					upd_occur(card_one[on][j], on);
			}
		}
	} while (flag);
	int t = 0;
	for (int i = 0; i < card_one.size(); i++) {
		if (cdel[i])
			continue;
		++t;
	}
	return t;
}

int
preprocess::check_card(int id)
{ // 0: wrong  -1:useless    1:normal
	int ps = 0, ns = 0, t = -1;
	double poss = 0, negs = 0;
	for (int j = 1; j <= vars; j++) {
		if (fabs(mat[id][j]) < 1e-6)
			continue;
		t = j;
		if (mat[id][j] > 0)
			++ps, poss += mat[id][j];
		if (mat[id][j] < 0)
			++ns, negs += mat[id][j];
	}
	if (ns + ps == 1) {
		double r = mat[id][0] / mat[id][t];
		if (ps) {
			if (fabs(r) < 1e-6)
				;
			else if (r < 0) {
				return 0;
			}
			return -1;
		}
		if (ns) {
			if (fabs(r - 1) < 1e-6)
				;
			else if (r > 1) {
				return 0;
			}
			return -1;
		}
	}
	if (ns + ps == 0) {
		if (mat[id][0] < -(1e-6)) {
			return 0;
		}
		return -1;
	}
	if (poss <= mat[id][0]) {
		return -1;
	}
	if (negs >= mat[id][0]) {
		if (negs == mat[id][0]) {
			// unit
		} else
			return 0;
	}
	if (mat[id][0] == 0) {
		if (ps == 0) {
			return -1;
		} else if (ns == 0) {
			// unit
		}
	}
	return 1;
}

int
preprocess::card_elimination()
{
	// sigma aixi <= b
	std::vector<int> row_size;
	for (int i = 0; i < card_one.size(); i++) {
		if (cdel[i])
			continue;
		int b = 1;
		mat.emplace_back();
		row_size.push_back(card_one[i].size());
		int id = mat.size() - 1;
		mat[id].resize(vars + 1, 0);
		for (int j = 0; j < card_one[i].size(); j++) {
			if (card_one[i][j] < 0)
				b--;
			mat[id][abs(card_one[i][j])] += pnsign(card_one[i][j]);
		}
		mat[id][0] = b;
	}
	for (int i = 0; i < card_one.size(); i++)
		card_one[i].clear();
	card_one.clear();
	cdel.clear();
	for (int i = 1; i <= clauses; i++) {
		if (clause_delete[i])
			continue;
		int b = 1;
		for (int j = 0; j < clause[i].size(); j++)
			if (clause[i][j] < 0)
				b--;
		// convert >= to <=
		mat.emplace_back();
		row_size.push_back(clause[i].size());
		int id = mat.size() - 1;
		mat[id].resize(vars + 1, 0);
		for (int j = 0; j < clause[i].size(); j++) {
			mat[id][abs(clause[i][j])] += -pnsign(clause[i][j]);
		}
		mat[id][0] = -b;
	}
	int row = mat.size();
	std::vector<int> mat_del, upp, low, var_score1, var_score2;
	mat_del.resize(row, 0);
	var_score1.resize(vars + 1, 0);
	var_score2.resize(vars + 1, 0);
	for (int v = 1; v <= vars; v++) {
		upp.clear();
		low.clear();
		for (int i = 0; i < row; i++) {
			if (fabs(mat[i][v]) < 1e-6)
				continue;
			if (mat[i][v] > 0)
				upp.push_back(i);
			else
				low.push_back(i);
		}
		var_score1[v] = upp.size() * low.size();
		for (int i = 0; i < upp.size(); i++)
			for (int j = 0; j < low.size(); j++)
				var_score2[v] += row_size[upp[i]] * row_size[low[j]];
	}
	std::vector<int> elim;
	elim.resize(vars + 1, 0);
	for (int turn = 1; turn <= vars; turn++) {
		int v = 0;
		for (int i = 1; i <= vars; i++) {
			if (elim[i])
				continue;
			if (!v)
				v = i;
			else {
				if (var_score1[i] < var_score1[v])
					v = i;
				else if (var_score1[i] == var_score1[v] && var_score2[i] < var_score2[v])
					v = i;
			}
		}
		elim[v] = 1;
		upp.clear();
		low.clear();
		for (int i = 0; i < row; i++) {
			if (mat_del[i])
				continue;
			if (fabs(mat[i][v]) < 1e-6)
				continue;
			mat_del[i] = 1;
			if (mat[i][v] > 0) {
				upp.push_back(i);
			} else {
				low.push_back(i);
			}
		}
		if (mat.size() + upp.size() * low.size() > 1e7 / vars || mat.size() > 1e3)
			return 1;
		for (int iu = 0; iu < upp.size(); iu++) {
			int u = upp[iu];
			for (int il = 0; il < low.size(); il++) {
				int l = low[il];
				double b = mat[u][0] / mat[u][v] + mat[l][0] / (-mat[l][v]);
				mat.emplace_back();
				mat_del.push_back(0);
				int id = mat.size() - 1;
				mat[id].resize(vars + 1, 0);
				for (int j = 0; j <= vars; j++)
					mat[id][j] = mat[u][j] / mat[u][v] + mat[l][j] / (-mat[l][v]);

				int check_res = check_card(id);
				if (check_res == 0)
					return 0;
				if (check_res == -1)
					mat.pop_back();
			}
		}
		row = mat.size();
	}
	return 1;
}

int
preprocess::preprocess_card()
{
	int sone = search_almost_one();
	LOGDEBUG1("[PRS %d] [CE] almost one cons: %d\n", this->getSolverId(), sone);
	if (!sone)
		return 1;
	int scc = scc_almost_one();
	int sz = card_one.size();
	for (int i = 1; i <= clauses; i++)
		if (!clause_delete[i])
			++sz;
	if (sz > 1e7 / vars || !scc || sz > 1e3) {
		for (int i = 0; i < card_one.size(); i++)
			card_one[i].clear();
		card_one.clear();
		cdel.clear();
		return 1;
	}
	// printf("c [CE] scc cons: %d\n", scc);
	int res = card_elimination();
	for (int i = 0; i < mat.size(); i++)
		mat[i].clear();
	mat.clear();
	return res;
}