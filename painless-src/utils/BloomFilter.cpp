#include "utils/BloomFilter.h"

/* Lock-free concurrent Bloom Filter implementation */

size_t BloomFilter::get_index(size_t bit) const
{
   // std::cout << "->"<< bit << " " << BITS_PER_ELEMENT << (bit / BITS_PER_ELEMENT) << std::endl;
   return bit / BITS_PER_ELEMENT;
}

size_t BloomFilter::get_mask(size_t bit) const
{
   // std::cout << bit << " " << BITS_PER_ELEMENT << " " << ((bit % BITS_PER_ELEMENT)) << std::endl;
   return 1L << (bit % BITS_PER_ELEMENT);
}

void BloomFilter::set(size_t bit)
{
   // bits_[get_index(bit)] |= get_mask(bit);
   auto tmp = get_mask(bit);
   bits_[get_index(bit)] |= tmp;
}

bool BloomFilter::test(size_t bit) const
{
   // return bits_[get_index(bit)] & get_mask(bit);
   // std::cout << get_mask(bit) << " " << get_index(bit) << std::endl;
   return bits_[get_index(bit)] & get_mask(bit);
}

void BloomFilter::insert(std::vector<int> &clause)
{
   for (const auto &f : hash_functions_)
   {
      hash_t hash = f(clause) % mem_size_bits_;
      set(hash);
   }
}

uint8_t BloomFilter::test_and_insert(std::vector<int> &clause)
{
   hash_t hash = hash_functions_[0](clause) % mem_size_bits_;
   uint8_t count = 1;
   if (!test(hash))
   {
      set(hash);
   }
   else
   {
      auto it = count_per_checksum.find(hash);
      if (it == count_per_checksum.end())
      {
         count = 2;
      }
      else
      {
         count = std::min(it->second + 1, 12);
      }
      count_per_checksum[hash] = count;
   }
   return count;
}

uint8_t BloomFilter::test_and_insert(size_t checksum, int max_limit_duplicas)
{
   if ((int64_t)checksum == -1)
      return 1;
   hash_t hash = checksum % mem_size_bits_;
   uint8_t count = 1;
   if (!test(hash))
   {
      set(hash);
   }
   else
   {
      auto it = count_per_checksum.find(hash);
      if (it == count_per_checksum.end())
      {
         count = 2;
      }
      else
      {
         count = std::min(it->second + 1, max_limit_duplicas);
      }
      count_per_checksum[hash] = count;
   }
   return count;
}

bool BloomFilter::contains(std::vector<int> &clause)
{
   hash_t hash;
   for (const auto &f : hash_functions_)
   {
      hash = f(clause) % mem_size_bits_;
      if (!test(hash))
         return false;
   }
   return true;
}

bool BloomFilter::contains(std::vector<int> &&clause)
{
   hash_t hash;
   for (const auto &f : hash_functions_)
   {
      hash = f(clause) % mem_size_bits_;
      if (test(hash))
         return false;
   }
   return true;
}

bool BloomFilter::contains_or_insert(std::vector<int> &clause)
{
   hash_t hash;
   for (const auto &f : hash_functions_)
   {
      hash = f(clause) % mem_size_bits_;
      if (!test(hash))
      {
         insert(clause); // insert to set with all hashes
         return false;
      }
   }
   return true;
}

bool BloomFilter::contains_or_insert(std::vector<int> &&clause)
{
   hash_t hash;
   for (const auto &f : hash_functions_)
   {
      hash = f(clause) % mem_size_bits_;
      if (!test(hash))
      {
         insert(clause); // insert to set with all hashes
         return false;
      }
   }
   return true;
}