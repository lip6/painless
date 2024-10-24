#include "preprocess.hpp"

bool
preprocess::res_is_empty(int x)
{
	int op = occurp[x].size(), on = occurn[x].size();
	for (int i = 0; i < op; i++) {
		int o1 = occurp[x][i], l1 = clause[o1].size();
		if (clause_delete[o1])
			continue;
		for (int j = 0; j < l1; j++)
			if (abs(clause[o1][j]) != x)
				resseen[abs(clause[o1][j])] = pnsign(clause[o1][j]);
		for (int j = 0; j < on; j++) {
			int o2 = occurn[x][j], l2 = clause[o2].size(), flag = 0;
			if (clause_delete[o2])
				continue;
			for (int k = 0; k < l2; k++)
				if (abs(clause[o2][k]) != x && resseen[abs(clause[o2][k])] == -pnsign(clause[o2][k])) {
					flag = 1;
					break;
				}
			if (!flag) {
				for (int j = 0; j < l1; j++)
					resseen[abs(clause[o1][j])] = 0;
				return false;
			}
		}
		for (int j = 0; j < l1; j++)
			resseen[abs(clause[o1][j])] = 0;
	}
	return true;
}

bool
preprocess::preprocess_resolution()
{
	for (int i = 1; i <= vars; i++) {
		occurn[i].clear();
		occurp[i].clear();
		resseen[i] = clean[i] = seen[i] = 0;
	}
	for (int i = 1; i <= clauses; i++) {
		int l = clause[i].size();
		clause_delete[i] = 0;
		for (int j = 0; j < l; j++) {
			if (clause[i][j] > 0)
				occurp[abs(clause[i][j])].push_back(i);
			else
				occurn[abs(clause[i][j])].push_back(i);
		}
	}
	for (int i = 1; i <= vars; i++)
		if (occurn[i].size() == 0 && occurp[i].size() == 0)
			clean[i] = 1;

	int l = 1, r = 0;
	for (int i = 1; i <= vars; i++) {
		int op = occurp[i].size(), on = occurn[i].size();
		if (op * on > op + on || clean[i])
			continue;
		if (res_is_empty(i)) {
			q[++r] = i, clean[i] = 1;
		}
	}
	LOGDEBUG1("[PRS %d] [Resolution] len %d", this->getSolverId(), r);

	int now_turn = 0, seen_flag = 0;
	std::vector<int> vars;
	while (l <= r) {
		++now_turn;
		for (int j = l; j <= r; j++) {
			int i = q[j];
			int op = occurp[i].size(), on = occurn[i].size();
			for (int j = 0; j < op; j++)
				clause_delete[occurp[i][j]] = 1;
			for (int j = 0; j < on; j++)
				clause_delete[occurn[i][j]] = 1;
		}
		int ll = l;
		l = r + 1;

		vars.clear();
		++seen_flag;
		for (int u = ll; u <= r; u++) {
			int i = q[u];
			int op = occurp[i].size(), on = occurn[i].size();
			for (int j = 0; j < op; j++) {
				int o = occurp[i][j], l = clause[o].size();
				for (int k = 0; k < l; k++) {
					int v = abs(clause[o][k]);
					if (!clean[v] && seen[v] != seen_flag)
						vars.push_back(v), seen[v] = seen_flag;
				}
			}
			for (int j = 0; j < on; j++) {
				int o = occurn[i][j], l = clause[o].size();
				for (int k = 0; k < l; k++) {
					int v = abs(clause[o][k]);
					if (!clean[v] && seen[v] != seen_flag)
						vars.push_back(v), seen[v] = seen_flag;
				}
			}
		}
		for (int u = 0; u < vars.size(); u++) {
			int i = vars[u];
			int op = 0, on = 0;
			for (int j = 0; j < occurp[i].size(); j++)
				op += 1 - clause_delete[occurp[i][j]];
			for (int j = 0; j < occurn[i].size(); j++)
				on += 1 - clause_delete[occurn[i][j]];
			if (op * on > op + on) {
				continue;
			}
			if (res_is_empty(i)) {
				q[++r] = i, clean[i] = 1;
			}
		}
	}
	vars.clear();
	if (!r)
		return true;
	res_clauses = 0;
	res_clause.emplace_back();
	for (int i = 1; i <= clauses; i++) {
		if (!clause_delete[i])
			continue;
		++res_clauses;
		res_clause.emplace_back();
		int l = clause[i].size();
		for (int j = 0; j < l; j++) {
			res_clause[res_clauses].push_back(pnsign(clause[i][j]) * mapfrom[abs(clause[i][j])]);
		}
	}
	resolutions = r;
	resolution.emplace_back();
	for (int i = 1; i <= r; i++) {
		int v = mapfrom[q[i]];
		resolution.push_back(v);
		mapto[v] = 0, mapval[v] = -10;
	}
	update_var_clause_label();
	for (int i = 1; i <= orivars; i++) {
		if (mapto[i]) {
			mapto[i] = color[mapto[i]];
		}
	}
	return true;
}