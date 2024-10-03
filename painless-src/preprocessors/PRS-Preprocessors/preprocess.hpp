#pragma once

#include "./utils-prs/hashmap.hpp"
#include "./utils-prs/bitset.hpp"
#include "utils/Parameters.h"
#include "Entity.hpp"

#include <queue>
#include <unordered_set>
#include <vector>
#include <functional>
#include <chrono>
#include <cassert>
#include <atomic>

typedef long long ll;

#define mapv(a, b) (1ll * (a) * nlit + (ll)(b))

inline int pnsign(int x)
{
    return (x > 0 ? 1 : -1);
}
inline int sign(int x)
{
    return x & 1 ? -1 : 1;
}
inline int tolit(int x)
{
    return x > 0 ? ((x - 1) << 1) : ((-x - 1) << 1 | 1);
}
inline int negative(int x)
{
    return x ^ 1;
}
inline int toiidx(int x)
{
    return (x >> 1) + 1;
}
inline int toeidx(int x)
{
    return (x & 1 ? -toiidx(x) : toiidx(x));
}

struct xorgate
{
    xorgate(int _c, int _rhs, int _sz)
    {
        c = _c, rhs = _rhs, sz = _sz;
    }

public:
    int c;
    int rhs;
    int sz;
};

struct type_gate
{
    std::vector<int> in;
    int ins, out, type;
    type_gate() : ins(0), out(0), type(0) {}
    void push_in(int x)
    {
        in.push_back(x);
        ins++;
    }
    int &operator[](int index) { return in[index]; }
    const int &operator[](int index) const { return in[index]; }
    type_gate &operator=(const type_gate &other)
    {
        ins = other.ins;
        out = other.out;
        type = other.type;
        in.resize(ins);
        for (int i = 0; i < ins; i++)
            in[i] = other.in[i];
        return *this;
    }
};

struct preprocess : public Entity
{
public:
    preprocess(int id_);
    int vars;
    int clauses;
    std::vector<std::vector<int>> clause, res_clause;

    ~preprocess();

    void release_most();

    int flag, epcec_out, maxvar, nxors, rins;
    int *psign, *psum, *fixed, *cell, *used, *model, *topo_counter;
    std::vector<int> epcec_in, epcec_rin, *inv_C;
    std::vector<type_gate> gate;

    int maxlen, orivars, oriclauses, res_clauses, resolutions;
    int *f, nlit, *a, *val, *color, *varval, *q, *seen, *resseen, *clean, *mapto, *mapfrom, *mapval;
    // HashMap* C;
    std::vector<int> *occurp, *occurn, clause_delete, nxtc, resolution;

    int find(int x);
    bool res_is_empty(int var);
    void update_var_clause_label();
    void preprocess_init();
    bool preprocess_resolution();
    bool preprocess_binary();
    bool preprocess_up();
    void get_complete_model();
    int do_preprocess(char *filename);

    std::vector<std::vector<int>> card_one;
    std::vector<std::vector<double>> mat;
    std::vector<int> *occur;
    std::vector<int> cdel;
    int check_card(int id);
    int preprocess_card();
    int search_almost_one();
    int card_elimination();
    int scc_almost_one();
    void upd_occur(int v, int s);

    int *abstract;
    int gauss_eli_unit;
    int gauss_eli_binary;
    std::vector<xorgate> xors;
    std::vector<int> scc_id;
    std::vector<std::vector<int>> scc;
    std::vector<std::vector<int>> xor_scc;
    bool preprocess_gauss();
    int search_xors();
    int cal_dup_val(int i);
    int ecc_var();
    int ecc_xor();
    int gauss_elimination();

    int rematch_eql(int x);
    int rematch_and(int x);
    int rematch_xor(int x);
    bool cnf2aig();
    int find_fa(int x);
    int preprocess_circuit();
    void epcec_preprocess();
    bool do_epcec();
    bool _simulate(Bitset **result, int bit_size);

    /* Painless */
    unsigned getVariablesCount() { return this->vars; }

    void restoreModel(std::vector<int> &model)
    {
        for (int i = 1; i <= this->orivars; i++)
            if (this->mapto[i])
                this->mapval[i] = (model[abs(this->mapto[i]) - 1] > 0 ? 1 : -1) * (this->mapto[i] > 0 ? 1 : -1);
        this->get_complete_model();
        model.clear();
        for (int i = 1; i <= this->orivars; i++)
        {
            model.push_back(i * this->mapval[i]);
        }

        LOG1("[PRS %d] restored model of size %d, vars: %d, orivars: %d", this->getId(), model.size(), this->vars, this->orivars);
    }

    int count = 0; /* Moved from preprocess.cpp */

    /* Modifiable attribute */

    int maxVarCircuit;
    int maxVarGauss;
    int maxVarCard;

    int maxClauseSizeXor;
    int maxClauseCircuit;
    int maxClauseBinary;
    int maxClauseGauss;
    int maxClauseCard;

    std::vector<std::function<int()>> preprocessors;

    int preprocess_circuit_wrapper()
    {
        int res = 0;
        auto init = std::chrono::high_resolution_clock::now();

        if (vars <= this->maxVarCircuit && clauses <= this->maxClauseCircuit)
        {
            res = preprocess_circuit();
            if (res == 10)
            {
                LOG("[PRS %d] Solved by Circuit Check", this->getId());
            }
            else
            {
                res = 0;
            }
        }
        auto circuit = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Circuit Check took %.3lfs", id, std::chrono::duration_cast<std::chrono::milliseconds>(circuit - init).count() / 1000.0);
        return res;
    }

    int preprocess_gauss_wrapper()
    {
        int res = 0;
        auto init = std::chrono::high_resolution_clock::now();

        if (vars <= this->maxVarGauss && clauses <= this->maxClauseGauss)
        {
            res = preprocess_gauss();
            if (!res)
            {
                LOG("[PRS %d] Solved by Gauss Elimination", this->getId());
                res = 20;
            }
            else
            {
                res = 0;
            }
        }
        auto gauss = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Gauss Elimination (xor-limit=%d) took %.3lfs", id, this->maxClauseSizeXor, std::chrono::duration_cast<std::chrono::milliseconds>(gauss - init).count() / 1000.0);
        return res;
    }

    int preprocess_propagation_wrapper()
    {
        auto init = std::chrono::high_resolution_clock::now();
        int res = preprocess_up();

        if (!res)
        {
            delete[] mapto;
            delete[] mapval;
            clause.clear();
            res = 20;
        }
        else
        {
            res = 0;
        }

        auto up = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Unit Propagation took %.3lfs", id, std::chrono::duration_cast<std::chrono::milliseconds>(up - init).count() / 1000.0);
        return res;
    }

    int preprocess_card_wrapper()
    {
        int res = 0;
        auto init = std::chrono::high_resolution_clock::now();

        if (vars <= this->maxVarCard && clauses <= this->maxClauseCard)
        {
            res = preprocess_card();
            if (!res)
            {
                LOG("[PRS %d] Solved by Card Elimination", this->getId());
                delete[] mapto;
                delete[] mapval;
                clause.clear();
                res_clause.clear();
                resolution.clear();
                res = 20;
            }
            else
            {
                res = 0;
            }
        }
        auto card = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Card Elimination took %.3lfs", id, std::chrono::duration_cast<std::chrono::milliseconds>(card - init).count() / 1000.0);
        return res;
    }

    int preprocess_resolution_wrapper()
    {
        auto init = std::chrono::high_resolution_clock::now();
        int res = preprocess_resolution();
        if (!res)
        {
            LOG("[PRS %d] Solved by Resolution", this->getId());
            delete[] mapto;
            delete[] mapval;
            clause.clear();
            res_clause.clear();
            resolution.clear();
            res = 20;
        }
        auto resol = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Resolution took %.3lfs", id, std::chrono::duration_cast<std::chrono::milliseconds>(resol - init).count() / 1000.0);
        return res;
    }

    int preprocess_binary_wrapper()
    {
        auto init = std::chrono::high_resolution_clock::now();
        int res = 0;
        if (clauses <= this->maxClauseBinary)
        {
            res = preprocess_binary();
            if (!res)
            {
                delete[] mapto;
                delete[] mapval;
                clause.clear();
                res_clause.clear();
                resolution.clear();
                res = 20;
            }
        }
        auto binary = std::chrono::high_resolution_clock::now();
        LOG1("[PRS %d] Binary took %.3lfs", id, std::chrono::duration_cast<std::chrono::milliseconds>(binary - init).count() / 1000.0);
        return res;
    }
};