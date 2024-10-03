#include "preprocess.hpp"


int preprocess::find(int x) {
    if (f[x] == x) return x;
    int fa = f[x];
    f[x] = find(fa);
    val[x] = val[fa] * val[x];
    return f[x];
}

bool preprocess::preprocess_binary() {
    HashMap *C = new HashMap();
    for (int i = 1; i <= clauses; i++) {
        int l = clause[i].size();
        for (int j = 0; j < l; j++) {
            clause[i][j] = tolit(clause[i][j]);
        }
    }
    nlit = (vars << 1) + 2;
    for (int i = 1; i <= vars; i++) f[i] = i, val[i] = 1, varval[i] = color[i] = resseen[tolit(i)] = resseen[tolit(-i)] = 0;
    for (int i = 1; i <= clauses; i++) clause_delete[i] = 0;
    int len = 0;
    for (int i = 1; i <= clauses; i++) {
        if (clause[i].size() != 2) continue;
        nxtc[++len] = i;
        ll id1 = mapv(clause[i][0], clause[i][1]),
           id2 = mapv(clause[i][1], clause[i][0]);
        C->insert(id1, i);
        C->insert(id2, i);
    }
    LOGDEBUG1("[PRS %d] After insert", this->getId());
    int simplify = 1, turn = 0;
    while (simplify) {
        simplify = 0;
        ++turn;
        int s1 = 0, s2 = 0, s3 = 0;
        for (int k = 1; k <= len; k++) {
            int i = nxtc[k];
            if (clause[i].size() != 2 || clause_delete[i]) continue;
            ll id1 = mapv(negative(clause[i][0]), negative(clause[i][1])),
               id2 = mapv(clause[i][0], negative(clause[i][1])),
               id3 = mapv(negative(clause[i][0]), clause[i][1]);
            int r = C->get(id1, 0);
            if (r) {
                simplify = 1;
                ++s1;
                clause_delete[r] = clause_delete[i] = 1;
                int u = toiidx(clause[i][0]), v = toiidx(clause[i][1]);
                int fa = find(u), fb = find(v);
                int sig = sign(clause[i][0]) * sign(clause[i][1]) * (-1);
                //sig == 1 : a = b   -1 : a = -b
                if (fa != fb) {
                    if (fa < fb) {
                        f[fa] = fb;
                        val[fa] = sig / (val[u] * val[v]);
                        if (varval[fa])
                            varval[fb] = val[fa] * varval[fa];
                    }
                    else if (fa > fb) {
                        f[fb] = fa;
                        val[fb] = sig / (val[u] * val[v]);
                        if (varval[fb])
                            varval[fa] = val[fb] * varval[fb];
                    }
                }
                else {
                    if (sig != val[u] * val[v])
                        return false;
                }
            }
            int d1 = C->get(id2, 0);
            if (d1) {
                int v = toiidx(clause[i][0]);
                if (varval[v] && varval[v] != sign(clause[i][0])) {
                    return false;
                }
                clause_delete[d1] = clause_delete[i] = 1;
                simplify = 1;
                varval[v] = sign(clause[i][0]);
            }
            int d2 = C->get(id3, 0);
            if (d2) {
                int v = toiidx(clause[i][1]);
                if (varval[v] && varval[v] != sign(clause[i][1])) {
                    return false;
                }
                clause_delete[d2] = clause_delete[i] = 1;
                simplify = 1;
                varval[v] = sign(clause[i][1]); 
            }
        }

        for (int i = 1; i <= vars; i++) {
            int x = find(i);
            if (varval[i] && x != i) {
                if (varval[x]) {
                    if (varval[x] != varval[i] * val[i])
                        return false;
                }
                else varval[x] = varval[i] * val[i];
            }
        }
        for (int i = 1; i <= vars; i++) 
            if (varval[f[i]]) {
                if (varval[i]) {
                    if (varval[f[i]] != varval[i] * val[i])
                        return false;
                }
                else varval[i] = varval[f[i]] * val[i];
            }
        len = 0;

        for (int i = 1; i <= clauses; i++) {
            if (clause_delete[i]) continue;
            int l = clause[i].size(), oril = l;
            for (int j = 0; j < l; j++) {
                int v = toiidx(clause[i][j]), fa = f[v];
                a[j] = tolit(sign(clause[i][j]) * val[v] * fa);
            }
            int t = 0;
            for (int j = 0; j < l; j++) {
                int x = varval[toiidx(a[j])];
                if (x) {
                    int k = x * sign(a[j]);
                    if (k == 1) {
                        if (!clause_delete[i]) simplify = 1;
                        clause_delete[i] = 1, a[t++] = a[j];
                    }
                }
                else a[t++] = a[j];
            }
            if (t == 0) return false;
            if (t != l) simplify = 1, l = t;
            
            t = 0;
            for (int j = 0; j < l; j++) {
                if (resseen[a[j]] == i) continue;
                resseen[a[j]] = i, a[t++] = a[j];
            }
            if (t != l) simplify = 1, l = t;
            for (int j = 0; j < l; j++)
                if (resseen[negative(a[j])] == i && !clause_delete[i]) {
                    clause_delete[i] = 1, simplify = 1;
                }
            for (int j = 0; j < l; j++) resseen[a[j]] = 0;
                
            if (l == 1) {
                if (sign(a[0]) * varval[toiidx(a[0])] == -1) return false;
                varval[toiidx(a[0])] = sign(a[0]);
                simplify = 1;
            }
            if (!clause_delete[i] && l == 2 && oril != 2) {
                nxtc[++len] = i;
                ll id1 = mapv(a[0], a[1]),
                   id2 = mapv(a[1], a[0]);
                C->insert(id1, i);
                C->insert(id2, i);
            }
            else if (!clause_delete[i] && l == 2 &&  oril == 2) {
                if (a[0] == clause[i][0] && a[1] == clause[i][1]) ;
                else if (a[1] == clause[i][0] && a[0] == clause[i][1]) ;
                else {
                    nxtc[++len] = i;
                    ll id1 = mapv(a[0], a[1]),
                       id2 = mapv(a[1], a[0]);
                    C->insert(id1, i);
                    C->insert(id2, i);
                }
            }
            clause[i].clear();
            for (int j = 0; j < l; j++)
                clause[i].push_back(a[j]);
        }
        
        for (int i = 1; i <= vars; i++) {
            int x = find(i);
            if (varval[i] && x != i) {
                if (varval[x]) {
                    if (varval[x] != varval[i] * val[i])
                        return false;
                }
                else varval[x] = varval[i] * val[i];
            }
        }
        for (int i = 1; i <= vars; i++) 
            if (varval[f[i]]) {
                if (varval[i]) {
                    if (varval[f[i]] != varval[i] * val[i])
                        return false;
                }
                else varval[i] = varval[f[i]] * val[i];
            }
    }
    LOGDEBUG1("[PRS %d] turns: %d", this->getId(), turn);

    for (int i = 1; i <= clauses; i++) {
        if (clause_delete[i]) continue;
        int l = clause[i].size();
        for (int j = 0; j < l; j++) {
            clause[i][j] = toeidx(clause[i][j]);
        }
    }

    update_var_clause_label();
    for (int i = 1; i <= orivars; i++) {
        if (mapval[i]) continue;
        int v = mapto[i], fa = find(v);
        if (varval[v] || varval[fa]) {
            mapval[i] = varval[v];
            mapto[i] = 0;
        }
        else if (color[fa]) mapto[i] = color[fa] * val[v];
        else {mapval[i] = val[v], mapto[i] = 0;}
    }
    
    return true;
}