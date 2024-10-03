#include "preprocess.hpp"

bool preprocess::preprocess_up() {
    for (int i = 1; i <= vars; i++) {
        varval[i] = 0;
        occurp[i].clear();
        occurn[i].clear();
        resseen[(i - 1) << 1] = resseen[(i - 1) << 1 | 1] = 0;
    }
    for (int i = 1; i <= clauses; i++) clause_delete[i] = 0;
    int head = 1, tail = 0;
    for (int i = 1; i <= clauses; i++) {
        int l = clause[i].size(), t = 0;
        for (int j = 0; j < l; j++) {
            int lit = tolit(clause[i][j]);
            if (resseen[lit] == i) continue;
            if (resseen[negative(lit)] == i) {clause_delete[i] = 1; break;}
            clause[i][t++] = clause[i][j];
            resseen[lit] = i;
        }
        if (clause_delete[i]) continue;
        clause[i].resize(t);
        for (int j = 0; j < t; j++) 
            if (clause[i][j] > 0) occurp[clause[i][j]].push_back(i);
            else occurn[-clause[i][j]].push_back(i);
        if (t == 0) return false;
        if (t == 1) {
            int lit = clause[i][0];
            clause_delete[i] = 1;
            if (varval[abs(lit)]) {
                if (varval[abs(lit)] == pnsign(lit)) continue;
                else return false;
            }
            varval[abs(lit)] = pnsign(lit); 
            q[++tail] = abs(lit); 
        }
    }
    for (int i = 1; i <= vars + vars; i++) resseen[i] = 0;
    while (head <= tail) {
        int x = q[head++];
        if (varval[x] == 1) {
            for (int i = 0; i < occurp[x].size(); i++)
                clause_delete[occurp[x][i]] = 1;
            for (int i = 0; i < occurn[x].size(); i++) {
                int o = occurn[x][i], t = 0;
                if (clause_delete[o]) continue;
                for (int j = 0; j < clause[o].size(); j++) {
                    if (varval[abs(clause[o][j])] == pnsign(clause[o][j])) {
                        clause_delete[o] = 1; break;
                    }
                    if (varval[abs(clause[o][j])] == -pnsign(clause[o][j])) continue;
                    clause[o][t++] = clause[o][j];
                }
                if (clause_delete[o]) continue;
                clause[o].resize(t);
                if (t == 0) return false;
                if (t == 1) {
                    int lit = clause[o][0];
                    clause_delete[o] = 1;
                    if (varval[abs(lit)]) {
                        if (varval[abs(lit)] == pnsign(lit)) continue;
                        else return false;
                    }
                    varval[abs(lit)] = pnsign(lit); 
                    q[++tail] = abs(lit); 
                }
            }
        }
        else {
            for (int i = 0; i < occurn[x].size(); i++)
                clause_delete[occurn[x][i]] = 1;
            for (int i = 0; i < occurp[x].size(); i++) {
                int o = occurp[x][i], t = 0;
                if (clause_delete[o]) continue;
                for (int j = 0; j < clause[o].size(); j++) {
                    if (varval[abs(clause[o][j])] == pnsign(clause[o][j])) {
                        clause_delete[o] = 1; break;
                    }
                    if (varval[abs(clause[o][j])] == -pnsign(clause[o][j])) continue;
                    clause[o][t++] = clause[o][j];
                }
                if (clause_delete[o]) continue;
                clause[o].resize(t);
                if (t == 0) return false;
                if (t == 1) {
                    int lit = clause[o][0];
                    clause_delete[o] = 1;
                    if (varval[abs(lit)]) {
                        if (varval[abs(lit)] == pnsign(lit)) continue;
                        else return false;
                    }
                    varval[abs(lit)] = pnsign(lit); 
                    q[++tail] = abs(lit); 
                }
            }
        }
    }
    update_var_clause_label();
    for (int i = 1; i <= tail; i++) {
        int v = q[i];
        mapval[v] = varval[v];
    }
    mapfrom = new int[vars + 1];
    for (int i = 1; i <= vars; i++) mapfrom[i] = 0;
    for (int i = 1; i <= orivars; i++) {
        if (color[i])
            mapto[i] = color[i], mapfrom[color[i]] = i;
        else if (!mapval[i]) // not in unit queue, then it is no use var
            mapto[i] = 0, mapval[i] = 1;
        else
            mapto[i] = 0;
    }
    return true;
}