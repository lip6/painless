#pragma once

#include <atomic>
#include <climits>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include "utils/hashfunc.h"
// #include <spot/misc/clz.hh>

#define NUM_BITS 1048576 // 8MB
// #define NUM_BITS 67108864 // 8MB

typedef size_t hash_t;
typedef hash_t (*hash_function_t)(std::vector<int> &);
typedef std::vector<hash_function_t> hash_functions_t;

/// Jenkins lookup3 hash function.
///
///  https://burtleburtle.net/bob/c/lookup3.c
#define _jenkins_rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

inline hash_t lookup3_hash(hash_t key)
{
  size_t s1, s2;
  s1 = s2 = 0xdeadbeef;
  s2 ^= s1;
  s2 -= _jenkins_rot(s1, 14);
  key ^= s2;
  key -= _jenkins_rot(s2, 11);
  s1 ^= key;
  s1 -= _jenkins_rot(key, 25);
  s2 ^= s1;
  s2 -= _jenkins_rot(s1, 16);
  key ^= s2;
  key -= _jenkins_rot(s2, 4);
  s1 ^= key;
  s1 -= _jenkins_rot(key, 14);
  s2 ^= s1;
  s2 -= _jenkins_rot(s1, 24);

  return s2;
}

inline hash_t lookup3_hash_clause(std::vector<int> &clause)
{
  if (clause.size() == 0)
    return 0;
  hash_t hash = lookup3_hash(clause[0]);
  int clauseSize = clause.size();
  for (int i = 1; i < clauseSize; i++)
  {
    hash ^= lookup3_hash(clause[i]);
  }
  return hash;
}

inline hash_t lookup3_hash_clause(int *clause, int size)
{
  if (size == 0)
    return 0;
  hash_t hash = lookup3_hash(clause[0]);
  for (int i = 1; i < size; i++)
  {
    hash ^= lookup3_hash(clause[i]);
  }
  return hash;
}

/* Lock-free concurrent Bloom Filter implementation */
class BloomFilter
{
private:
  size_t mem_size_;
  size_t mem_size_bits_;
  hash_functions_t hash_functions_;
  std::unordered_map<hash_t, uint8_t> count_per_checksum;

  /* Internal concurrent bitset */
  static const size_t BITS_PER_ELEMENT = sizeof(uint64_t) * CHAR_BIT;

  uint64_t *bits_;
  size_t get_index(size_t bit) const;
  size_t get_mask(size_t bit) const;
  void set(size_t bit);
  bool test(size_t bit) const;

public:
  BloomFilter(size_t mem_size, hash_functions_t hash_functions)
      : mem_size_(mem_size),
        // std::pow(2, std::ceil(std::log(mem_size)/std::log(2)))),
        mem_size_bits_(mem_size * BITS_PER_ELEMENT),
        hash_functions_(hash_functions)
  {
    // std::cout << "BF: size " << mem_size_ << " "<< mem_size << std::endl;
    bits_ = new uint64_t[mem_size_]{0};
    if (hash_functions.empty())
      throw std::invalid_argument("Bloom filter has no hash functions");
  }

  // Default internal hash function
  BloomFilter(size_t mem_size)
      : BloomFilter(mem_size, {lookup3_hash_clause}) {}

  BloomFilter()
      : BloomFilter(NUM_BITS) {}

  ~BloomFilter()
  {
    delete[] bits_;
  }
  void insert(std::vector<int> &clause);
  uint8_t test_and_insert(std::vector<int> &clause);
  uint8_t test_and_insert(size_t checksum, int max_limit_duplicas);
  bool contains_or_insert(std::vector<int> &clauses);
  bool contains_or_insert(std::vector<int> &&clauses);
  bool contains(std::vector<int> &clause);
  bool contains(std::vector<int> &&clause);
};