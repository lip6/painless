#ifndef BITSET_H_
#define BITSET_H_
#include <random>
#include <limits>
#include <iostream>
typedef unsigned long long ull;

class Bitset {
public: 
    static const ull size_correcter = 1ull;
    static const ull one_bit = 1ull;
    static const ull zero_bit = 0ull;
    int bits = 8 * sizeof(ull);
    int m_size = 0, n = 0;
    ull *array, hashval;
    void print();
    void hash() noexcept;
    void allocate(int sz) noexcept;
    void random() noexcept;
    void free() noexcept;
    void eqs(const Bitset& u, int s) noexcept;
    void ands(const Bitset& u, const Bitset& v, int s, int s1, int s2) noexcept;
    void xors(const Bitset& u, const Bitset& v, int s, int s1, int s2) noexcept;

    bool operator==(const Bitset& rhs) const noexcept;

    constexpr int size() const noexcept;

    void set() noexcept;
    Bitset& set(int);
    void reset() noexcept;
    Bitset& reset(int);
    Bitset& flip() noexcept;

    Bitset& operator=(const Bitset& other) noexcept;
    Bitset operator~() const noexcept;
    int operator[](int) noexcept;

};


#endif