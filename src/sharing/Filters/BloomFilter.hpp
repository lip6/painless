#pragma once

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "containers/SimpleTypes.hpp"

#include "containers/ClauseUtils.hpp"

#define NUM_BITS 1048576 // 8MB
// #define NUM_BITS 67108864 // 8MB

typedef hash_t (*hash_function_t)(const lit_t*, const csize_t size);
typedef std::vector<hash_function_t> hash_functions_t;

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

	uint64_t* bits_;
	size_t get_index(size_t bit) const;
	size_t get_mask(size_t bit) const;
	void set(size_t bit);
	bool test(size_t bit) const;

  public:
	BloomFilter(size_t mem_size, hash_functions_t hash_functions)
		: mem_size_(mem_size)
		,
		// std::pow(2, std::ceil(std::log(mem_size)/std::log(2)))),
		mem_size_bits_(mem_size * BITS_PER_ELEMENT)
		, hash_functions_(hash_functions)
	{
		// std::cout << "BF: size " << mem_size_ << " "<< mem_size << std::endl;
		bits_ = new uint64_t[mem_size_]{ 0 };
		if (hash_functions.empty())
			throw std::invalid_argument("Bloom filter has no hash functions");
	}

	// Default internal hash function
	BloomFilter(size_t mem_size)
		: BloomFilter(mem_size, { ClauseUtils::lookup3_hash_clause })
	{
	}

	BloomFilter()
		: BloomFilter(NUM_BITS)
	{
	}

	~BloomFilter() { delete[] bits_; }
	void insert(const int* clause, unsigned int size);
	uint8_t test_and_insert(const int* clause, unsigned int size);
	uint8_t test_and_insert(size_t checksum, int max_limit_duplicas);
	bool contains_or_insert(const int* clause, unsigned int size);
	bool contains(const int* clause, unsigned int size);
};