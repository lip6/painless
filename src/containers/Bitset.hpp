#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

namespace pl {
/**
 * @brief A class representing a bitset with dynamic size.
 *
 * This class provides efficient storage and manipulation of a sequence of bits.
 * It uses unsigned long long as the underlying storage type.
 * Out of bound bits are set to zero for consistency in merge
 */
class Bitset
{

  public:
	/**
	 * @brief Construct a new Bitset object.
	 * @param size The number of bits in the bitset.
	 * @param default_value The default value for all bits (false by default).
	 * @details This constructor initializes a Bitset with the given number of bits.
	 * All bits are set to the specified default value. If the size is not a multiple
	 * of BITS_PER_BLOCK (typically 64), the constructor ensures that any unused bits
	 * in the last block are set to zero, maintaining consistency of the bitset.
	 */
	Bitset(size_t size, bool default_value = false)
		: num_bits(size)
	{
		bits.resize(num_blocks(), default_value ? ~0ULL : 0ULL);

		// Set out of bound bits to zero
		if (num_bits % BITS_PER_BLOCK != 0) {
			//                   |       out of bound       |
			// (1ULL << 40) - 1 = 00000000 00000000 00000000 11111111 11111111 11111111 11111111 11111111
			bits.back() &= (1ULL << (num_bits % BITS_PER_BLOCK)) - 1;
		}
	}

	/**
	 * @brief Calculate the number of blocks needed to store the bits.
	 * @return The number of blocks.
	 */
	size_t num_blocks() const { return (num_bits + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK; }

	/**
	 * @brief Access a bit in the bitset.
	 * @param pos The position of the bit to access.
	 * @return The value of the bit at the specified position.
	 */
	bool operator[](size_t pos) const
	{
		assert(pos < num_bits);
		return (bits[pos / BITS_PER_BLOCK] & (1ULL << (pos % BITS_PER_BLOCK))) != 0;
	}

	/**
	 * @brief Set a bit in the bitset.
	 * @param pos The position of the bit to set.
	 * @param value The value to set (true by default).
	 */
	void set(size_t pos, bool value = true)
	{
		if (value) {
			bits[pos / BITS_PER_BLOCK] |= (1ULL << (pos % BITS_PER_BLOCK));
		} else {
			bits[pos / BITS_PER_BLOCK] &= ~(1ULL << (pos % BITS_PER_BLOCK));
		}
	}

	/**
	 * @brief Clear all bits in the bitset.
	 */
	void clear() { std::fill(bits.begin(), bits.end(), 0ULL); }

	/**
	 * @brief Resize the bitset.
	 * @param new_size The new size of the bitset.
	 */
	void resize(size_t new_size)
	{
		// size_t old_num_blocks = num_blocks();
		num_bits = new_size;
		bits.resize(num_blocks(), 0ULL);
		if (num_bits % BITS_PER_BLOCK != 0) {
			bits.back() &= (1ULL << (num_bits % BITS_PER_BLOCK)) - 1;
		}
	}

	/**
	 * @brief Get the size of the bitset.
	 * @return The number of bits in the bitset.
	 */
	size_t size() const { return num_bits; }

	/**
	 * @brief Merge multiple bitsets with this bitset using a custom binary operation.
	 *
	 * @tparam BinaryOp A callable type that takes two unsigned long long arguments
	 *                  and returns an unsigned long long. This type is deduced from
	 *                  the argument passed to the function.
	 *
	 * @param other_bitsets A vector of Bitsets to merge with this Bitset.
	 * @param op A binary operation used to combine bits. It should be a callable
	 *           (function, lambda, or function object) that takes two unsigned long long
	 *           arguments and returns an unsigned long long.
	 *
	 * @note This function uses template argument deduction, allowing for flexible
	 *       usage with different types of binary operations without explicitly
	 *       specifying the template parameter.
	 *
	 * @details
	 *     // Using a lambda function
	 *     bitset.merge(other_bitsets, [](unsigned long long a, unsigned long long b) { return a | b; });
	 *
	 *     // Using a standard library function object
	 *     bitset.merge(other_bitsets, std::bit_or<unsigned long long>());
	 *
	 *     // Using a custom function
	 *     unsigned long long custom_or(unsigned long long a, unsigned long long b) { return a | b; }
	 *     bitset.merge(other_bitsets, custom_or);
	 */
	template<typename BinaryOp>
	void merge(const std::vector<Bitset>& other_bitsets, BinaryOp op)
	{
		for (const auto& other : other_bitsets) {
			size_t min_blocks = std::min(num_blocks(), other.num_blocks());
			for (size_t i = 0; i < min_blocks; ++i) {
				bits[i] = op(bits[i], other.bits[i]);
			}
		}
		if (num_bits % BITS_PER_BLOCK != 0) {
			bits.back() &= (1ULL << (num_bits % BITS_PER_BLOCK)) - 1;
		}
	}

	/**
	 * @brief Merge multiple bitsets using the OR operation.
	 * @param other_bitsets The bitsets to merge with.
	 */
	void merge_or(const std::vector<Bitset>& other_bitsets) { merge(other_bitsets, std::bit_or<unsigned long long>()); }

	/**
	 * @brief Merge multiple bitsets using the AND operation.
	 * @param other_bitsets The bitsets to merge with.
	 */
	void merge_and(const std::vector<Bitset>& other_bitsets)
	{
		merge(other_bitsets, std::bit_and<unsigned long long>());
	}

	/**
	 * @brief Get a pointer to the underlying data.
	 * @return A pointer to the first element of the underlying data.
	 */
	unsigned long long* data() { return bits.data(); }

	/**
	 * @brief Get a const pointer to the underlying data.
	 * @return A const pointer to the first element of the underlying data.
	 */
	const unsigned long long* data() const { return bits.data(); }

  private:
	std::vector<unsigned long long> bits;
	size_t num_bits;
	static const size_t BITS_PER_BLOCK = sizeof(unsigned long long) * 8;
};

}