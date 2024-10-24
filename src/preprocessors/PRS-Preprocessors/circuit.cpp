#include "preprocess.hpp"
#include <cassert>

int
preprocess::find_fa(int x)
{
	return f[x] == x ? x : f[x] = find_fa(f[x]);
}

int
preprocess::rematch_eql(int x)
{
	if (clause[x].size() != 2 || clause[x - 1].size() != 2)
		return 0;
	// if (abs(clause[x][0]) > abs(clause[x][1])) std::swap(clause[x][0], clause[x][1]);
	// if (abs(clause[x - 1][0]) > abs(clause[x - 1][1])) std::swap(clause[x - 1][0], clause[x - 1][1]);
	// if (clause[x][0] != -clause[x - 1][0]) return 0;
	// if (clause[x][1] != -clause[x - 1][1]) return 0;
	// assert(find(abs(clause[x][1])) == abs(clause[x][1]));
	// f[find(abs(clause[x][1]))] = find(abs(clause[x][0]));
	int a = clause[x][0], b = clause[x][1];
	int c = clause[x - 1][0], d = clause[x - 1][1];
	if (abs(a) > abs(b))
		std::swap(a, b);
	if (abs(c) > abs(d))
		std::swap(c, d);
	if (a != -c)
		return 0;
	if (b != -d)
		return 0;
	assert(find_fa(abs(b)) == abs(b));
	f[find_fa(abs(b))] = find_fa(abs(a));
	return 1;
}

int
preprocess::rematch_and(int x)
{
	++flag;
	int l = clause[x].size();
	if (l != 3)
		return 0;
	for (int i = 0; i < l; i++) {
		seen[abs(clause[x][i])] = flag;
		psign[abs(clause[x][i])] = pnsign(clause[x][i]);
		psum[abs(clause[x][i])] = 0;
	}
	for (int i = x - l + 1; i < x; i++) {
		int l1 = clause[i].size();
		if (l1 != 2)
			return 0;
		for (int j = 0; j < l1; j++) {
			if (seen[abs(clause[i][j])] != flag)
				return 0;
			if (psign[abs(clause[i][j])] != -pnsign(clause[i][j]))
				return 0;
			++psum[abs(clause[i][j])];
		}
	}
	int t = 0, r = 0;
	for (int i = 0; i < l; i++)
		if (psum[abs(clause[x][i])] != 1)
			++t, r = i;
	if (t != 1)
		return 0;
	if (psum[abs(clause[x][r])] != l - 1 || clause[x][r] < 0)
		return 0;

	gate.emplace_back();
	int id = gate.size() - 1;
	gate[id].type = 0;
	gate[id].out = abs(clause[x][r]);
	for (int i = 0; i < l; i++)
		if (i != r)
			gate[id].push_in(-clause[x][i]);
	return 1;
}

int
preprocess::rematch_xor(int x)
{
	for (int i = x - 3; i <= x; i++)
		if (clause[i].size() != 3)
			return 0;
	++flag;
	int xor_sign = true;
	for (int i = 0; i < 3; i++) {
		seen[abs(clause[x][i])] = flag;
		psign[abs(clause[x][i])] = pnsign(clause[x][i]);
		psum[abs(clause[x][i])] = 0;
		if (clause[x][i] < 0)
			xor_sign = !xor_sign;
	}
	for (int i = x - 3; i < x; i++) {
		int xor_sign_one = true;
		for (int j = 0; j < 3; j++) {
			if (seen[abs(clause[i][j])] != flag)
				return 0;
			if (psign[abs(clause[i][j])] == pnsign(clause[i][j]))
				++psum[abs(clause[i][j])];
			if (clause[i][j] < 0)
				xor_sign_one = !xor_sign_one;
		}
		if (xor_sign != xor_sign_one)
			return false;
	}
	if (psum[abs(clause[x][0])] != 1 || psum[abs(clause[x][1])] != 1 || psum[abs(clause[x][2])] != 1)
		return 0;
	gate.emplace_back();
	int id = gate.size() - 1;
	gate[id].type = 1;
	int a = abs(clause[x][0]), b = abs(clause[x][1]), c = abs(clause[x][2]);
	if (b > a)
		std::swap(a, b);
	if (c > a)
		std::swap(a, c);
	gate[id].out = a;
	if (xor_sign)
		b = -b;
	gate[id].push_in(b), gate[id].push_in(c);
	++nxors;
	return 1;
}

bool
preprocess::cnf2aig()
{
	for (int i = 1; i <= vars; i++)
		f[i] = i, fixed[i] = seen[i] = psign[i] = psum[i] = 0;
	gate.emplace_back();
	int last = 0;
	flag = 0;
	std::vector<int> val;
	for (int i = 1; i <= clauses; i++) {
		if (i - last >= 2 && rematch_eql(i)) {
			if (i - last > 2)
				return false;
			last = i;
		} else if (i - last >= 3 && rematch_and(i)) {
			if (i - last > 3)
				return false;
			last = i;
		} else if (i - last >= 4 && rematch_xor(i)) {
			if (i - last > 4)
				return false;
			last = i;
		} else if (clause[i].size() == 1) {
			if (i - last > 1)
				return false;
			val.push_back(clause[i][0]), last = i;
		}
		if (i - last >= 4)
			return false;
	}
	if (last != clauses)
		return false;
	++flag;
	maxvar = vars;
	// PO: seen - psum
	// PI: psum - seen
	cell = new int[maxvar + val.size() + 1];
	for (int i = 1; i <= maxvar + val.size(); i++)
		cell[i] = 0;
	for (int i = 1; i < gate.size(); i++) {
		assert(gate[i].out > 0);
		assert(find_fa(gate[i].out) == gate[i].out);
		gate[i][0] = pnsign(gate[i][0]) * find_fa(abs(gate[i][0]));
		gate[i][1] = pnsign(gate[i][1]) * find_fa(abs(gate[i][1]));
		if (cell[gate[i].out])
			return false;
		cell[gate[i].out] = i;
		seen[gate[i].out] = flag;
		psign[abs(gate[i][0])] = psign[abs(gate[i][1])] = flag;
		// printf("%d = %d %d\n", gate[i].out, gate[i].in[0], gate[i].in[1]);
	}
	LOGDEBUG1("[PRS %d] [Circuit] PI: ", this->getSolverId());
	std::vector<int> is_input, andg;
	is_input.resize(vars + 1, 0);
	for (int i = 1; i <= vars; i++)
		if (psign[i] == flag && seen[i] != flag)
			epcec_in.push_back(i), is_input[i] = 1, fixed[i] = 0; // printf("%d ", i);
	// puts("");
	rins = epcec_in.size();
	for (int i = 0; i < val.size(); i++) {
		val[i] = find_fa(abs(val[i])) * pnsign(val[i]);
		int v = val[i], x = abs(v);
		if (is_input[x])
			fixed[x] = pnsign(v), rins--;
		else
			andg.push_back(v);
	}
	nxors /= 96;
	LOGDEBUG1("[PRS %d] [Circuit] Real inputs: %d\nc xors: %d", this->getSolverId(), rins, nxors);
	if (andg.size() == 0)
		return false;
	if (andg.size() >= 2) {
		int lastid = andg[0];
		for (int i = 1; i < andg.size(); i++) {
			gate.emplace_back();
			int v = andg[i], id = gate.size() - 1;
			gate[id].type = 0;
			gate[id].push_in(lastid);
			gate[id].push_in(v);
			gate[id].out = ++maxvar;
			cell[maxvar] = id;
			lastid = maxvar;
		}
		epcec_out = maxvar;
	} else {
		epcec_out = andg[0];
		return false;
	}
	return true;
}

void
preprocess::epcec_preprocess()
{
	used = new int[maxvar + 1];
	model = new int[maxvar + 1];
	topo_counter = new int[maxvar + 1];
	for (int i = 1; i <= maxvar; i++)
		used[i] = 0;

	inv_C = new std::vector<int>[maxvar + 1];
	for (int i = 1; i < gate.size(); i++)
		for (int j = 0; j < gate[i].ins; j++)
			inv_C[abs(gate[i][j])].push_back(i);
	for (int i = 0; i < epcec_in.size(); i++) {
		if (!fixed[epcec_in[i]])
			epcec_rin.push_back(epcec_in[i]);
	}
}

bool
preprocess::_simulate(Bitset** result, int bit_size)
{
	int* fanouts = new int[maxvar + 1];
	for (int i = 1; i <= maxvar; i++) {
		used[i] = 0;
		fanouts[i] = inv_C[i].size();
	}
	for (int i = 1; i <= gate.size(); i++)
		topo_counter[i] = 0;
	std::queue<int> q;
	for (int i = 0; i < epcec_in.size(); i++) {
		q.push(epcec_in[i]);
		used[epcec_in[i]] = 2;
	}
	int o = abs(epcec_out);
	while (!q.empty()) {
		int u = q.front();
		q.pop();
		if (used[o])
			break;

		for (int i = 0; i < inv_C[u].size(); i++) {
			int c = inv_C[u][i];
			if (++topo_counter[c] != gate[c].ins)
				continue;
			int v = abs(gate[c].out);
			q.push(v), used[v] = 1;
			result[v] = new Bitset;
			result[v]->allocate(bit_size);
			int l1 = gate[c][0], l2 = gate[c][1];
			int v1 = abs(l1), v2 = abs(l2);
			if (gate[c].type == 0)
				result[v]->ands(*result[v1], *result[v2], gate[c].out, l1, l2);
			else if (gate[c].type == 1)
				result[v]->xors(*result[v1], *result[v2], gate[c].out, l1, l2);
			fanouts[v1]--;
			fanouts[v2]--;
		}
	}

	bool res = true;
	Bitset& bit = *result[o];
	if (epcec_out < 0)
		bit.flip();
	for (int i = 0; i < bit.m_size; i++)
		if (bit.array[i] != 0)
			res = false;
	if (!res) {
		for (int i = 0; i < bit.m_size * 64; i++) {
			if (bit[i] == 0)
				continue;
			for (int j = 1; j <= vars; j++) {
				model[j] = -1;
				if (result[j] != nullptr)
					model[j] = (*result[j])[i];
			}
			break;
		}
	}
	for (int i = 1; i <= maxvar; i++)
		if (result[i] != nullptr) {
			result[i]->free();
			delete result[i];
			result[i] = nullptr;
		}
	delete fanouts;
	return res;
}

bool
preprocess::do_epcec()
{
	Bitset** result = new Bitset*[maxvar + 1];
	for (int i = 1; i <= vars; i++)
		result[i] = nullptr;
	int nri = epcec_rin.size(), ni = epcec_in.size();
	const int maxR = 20;
	int bit_size = 1 << std::min(maxR, nri);
	if (nri > maxR) {
		int extra_len = nri - maxR;
		ull extra_values = 0;

		while (extra_values < (1LL << (extra_len))) {
			if (extra_values % (1 << 7) == 0)
				LOG2("[PRS %d] [Circuit] epcec round [%llu / %lld]",
					 this->getSolverId(),
					 extra_values,
					 (1LL << (extra_len)));
			for (int i = 0; i < ni; i++) {
				int v = epcec_in[i];
				result[v] = new Bitset;
				result[v]->allocate(bit_size);
				if (fixed[v]) {
					if (fixed[v] == 1)
						result[v]->set();
					else
						result[v]->reset();
				}
			}
			for (int i = 0; i < extra_len; i++) {
				int input_var = epcec_rin[i];
				ull val = extra_values & (1LL << (extra_len - i - 1));
				if (val != 0)
					val = ~(val = 0);
				for (int j = 0; j < result[input_var]->m_size; j++) {
					result[input_var]->array[j] = val;
				}
			}
			int sz = 1 << maxR;
			int unit = sz;
			for (int i = extra_len; i < nri; i++) {
				int input_var = epcec_rin[i];
				unit >>= 1;
				if (unit >= 64) {
					const ull all_zero = 0;
					const ull all_one = ~all_zero;
					for (int j = 0; j < sz / 64; j++) {
						int value = (j * 64 / unit) % 2;
						assert(j < result[input_var]->m_size);
						if (value)
							result[input_var]->array[j] = all_one;
						else
							result[input_var]->array[j] = all_zero;
					}
				} else {
					for (int j = 0; j < 64; j++) {
						int value = (j >> (nri - i - 1)) & 1;
						if (value)
							result[input_var]->reset(j);
						else
							result[input_var]->set(j);
					}

					for (int j = 1; j < sz / 64; j++) {
						result[input_var]->array[j] = result[input_var]->array[0];
					}
				}
			}
			int res = _simulate(result, bit_size);
			extra_values++;
			if (!res)
				return 0;
		}
	} else {
		for (int i = 0; i < ni; i++) {
			int v = epcec_in[i];
			result[v] = new Bitset;
			result[v]->allocate(bit_size);
			if (fixed[v]) {
				if (fixed[v] == 1)
					result[v]->set();
				else
					result[v]->reset();
			}
		}
		int sz = 1 << nri;
		int unit = sz;
		for (int i = 0; i < epcec_rin.size(); i++) {
			int input_var = epcec_rin[i];
			unit >>= 1;
			if (unit >= 64) {
				const ull all_zero = 0;
				const ull all_one = ~all_zero;
				for (int j = 0; j < sz / 64; j++) {
					int value = (j * 64 / unit) % 2;
					if (value)
						result[input_var]->array[j] = all_one;
					else
						result[input_var]->array[j] = all_zero;
				}
			} else {
				for (int j = 0; j < 64; j++) {
					int value = (j >> (nri - i - 1)) & 1;
					if (value)
						result[input_var]->reset(j);
					else
						result[input_var]->set(j);
				}

				for (int j = 1; j < sz / 64; j++) {
					result[input_var]->array[j] = result[input_var]->array[0];
				}
			}
		}
		return _simulate(result, bit_size);
	}
	return true;
}

int
preprocess::preprocess_circuit()
{
	int res = cnf2aig();
	if (!res || rins <= 6 || rins > 32 || nxors < 10)
		goto failed;

	epcec_preprocess();
	res = do_epcec();
	if (!res) {
		LOG0("[PRS %d][Circuit] epcec result: SAT", this->getSolverId());
		int* copy_model = new int[vars + 1];
		for (int i = 1; i <= vars; i++) {
			copy_model[i] = model[i];
			int v = find_fa(i);
			if (copy_model[v] == 1) {
				model[i] = i;
			} else if (copy_model[v] == 0) {
				model[i] = -i;
			} else {
				model[i] = i;
			}
		}
		delete[] copy_model;
	}
	delete[] topo_counter;
	delete[] used;
	gate.clear();
	return res == 0 ? 10 : 0;
failed:
	gate.clear();
	return 0;
}