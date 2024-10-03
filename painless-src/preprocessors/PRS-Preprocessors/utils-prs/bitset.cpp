#include <cassert>
#include "bitset.hpp"

std::random_device rd;
std::mt19937_64 eng(1000007);
std::uniform_int_distribution<unsigned long long> distr;

#define ENABLE_AVX false

void Bitset::print() {
    for (int i = 0; i < 1; i++) {
        for (int j = 0; j < bits; j++)
            printf("%lld", (array[i] >> (bits - j - 1)) & 1);
    }
    puts("");
}

void Bitset::allocate(int sz) noexcept{
    n = sz;
    assert(n % bits == 0);
    m_size = n / bits;
    array = (ull*)aligned_alloc(64, sizeof(ull) * m_size);
}

void Bitset::random() noexcept{
    for (int i = 0; i < m_size; i++)
        array[i] = distr(eng);
}

void Bitset::hash() noexcept{
    hashval = array[0];
    for (int i = 1; i < m_size; i++)
        hashval += i * array[i];
}

void Bitset::free() noexcept{
    delete []array;
}

void Bitset::eqs(const Bitset& u, int s) noexcept{
    if (s > 0) {
        for (int i = 0; i < m_size; i++)
            array[i] = u.array[i];
    }
    else {
        for (int i = 0; i < m_size; i++)
            array[i] = ~u.array[i];
    }
}


void Bitset::ands(const Bitset& u, const Bitset& v, int s, int s1, int s2) noexcept{
    if (s1 > 0) {
        for (int i = 0; i < m_size; i++)
            array[i] = u.array[i];
    }
    else {
        for (int i = 0; i < m_size; i++) {
            array[i] = ~(u.array[i]);
        }
    }    
    if (s2 > 0) {
        for (int i = 0; i < m_size; i++)
            array[i] &= v.array[i];
    }
    else {
        for (int i = 0; i < m_size; i++)
            array[i] &= ~(v.array[i]);
    }
    if (s < 0) {
        for (int i = 0; i < m_size; i++)
            array[i] = ~array[i];
    }
}

void Bitset::xors(const Bitset& u, const Bitset& v, int s, int s1, int s2) noexcept{
    if (s1 > 0) {
        for (int i = 0; i < m_size; i++)
            array[i] = u.array[i];
    }
    else {
        for (int i = 0; i < m_size; i++)
            array[i] = ~(u.array[i]);
    }
    if (s2 > 0) {
        for (int i = 0; i < m_size; i++)
            array[i] ^= v.array[i];
    }
    else {
        for (int i = 0; i < m_size; i++)
            array[i] ^= ~(v.array[i]);
    }
    if (s < 0) {
        for (int i = 0; i < m_size; i++)
            array[i] = ~array[i];
    }
}

bool Bitset::operator==(const Bitset& rhs) const noexcept {
    assert(m_size == rhs.m_size);
    for (int i = 0; i < m_size; i++)
    {
        if (array[i] != rhs.array[i]) { return false; }
    }
    return true;
}

constexpr int Bitset::size() const noexcept {
    return n;
}

void Bitset::set() noexcept {
    for (int i = 0; i < m_size; i++)
    {
        array[i] |= ~zero_bit;
    }    
}

int Bitset::operator[](int position) noexcept {
    int index = position / bits;
    int pos = position - index * bits;
    return (array[index] & ( one_bit << pos )) != 0;
}

Bitset& Bitset::set(int position) {
    int index = position / bits;
    int pos = position - index * bits;
    array[index] |= ( one_bit << pos );
    return *this;
}


void Bitset::reset() noexcept {
    for (int i = 0; i < m_size; i++)
    {
        array[i] &= zero_bit;
    }
}


Bitset& Bitset::reset(int position) {
    int index = position / bits;
    int pos = position - index * bits;
    array[index] &= ~( one_bit << pos );
    return *this;
}

Bitset& Bitset::flip() noexcept {    
    for (int i = 0; i < m_size; i++)
    {
        array[i] = ~(array[i]);
    }
    return *this;
}

Bitset& Bitset::operator=(const Bitset& other) noexcept {
    assert(n == other.n);
    for (int i = 0; i < m_size; i++)
    {
        array[i] = other.array[i];
    }
    return *this;
}

Bitset Bitset::operator~() const noexcept {
    Bitset tmp;
    tmp.allocate(n);
    for (int i = 0; i < m_size; i++)
    {
        tmp.array[i] = ~array[i];
    }
    return tmp;
}
