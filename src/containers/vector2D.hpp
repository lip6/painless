#pragma once

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

typedef unsigned int row_size_t;
typedef unsigned int row_id_t;
typedef unsigned long int data_size_t;

namespace painless {
// ==================================================================SZSPAN========================================================================

/**
 * @class skipzero_span
 * @brief A span-like container that skips zero elements when iterating.
 * @ingroup pl_containers
 * @tparam T The type of elements in the span (must be arithmetic).
 */
template<typename T>
class skipzero_span
{
	static_assert(std::is_arithmetic<T>::value, "skipzero_span: Template parameter T must be an arithmetic type");

  public:
	// Custom iterator that skips zero elements, add a m_ptr attribute for midway starting points ?
	class Iterator
	{
	  public:
		using iterator_category = std::contiguous_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;

		Iterator(pointer begin, pointer end)
			: m_ptr(begin)
			, m_begin(begin)
			, m_end(end)
		{
			skip_zeros();
		}

		reference operator*() const { return *m_ptr; }
		pointer operator->() { return m_ptr; }

		// Prefix increment
		Iterator& operator++()
		{
			++m_ptr;
			skip_zeros();
			return *this;
		}

		// Postfix increment
		Iterator operator++(int)
		{
			Iterator tmp = *this;
			++(*this);
			return tmp;
		}

		// Prefix decrement
		Iterator& operator--()
		{
			--m_ptr;
			skip_zeros_backward();
			return *this;
		}

		// Postfix decrement
		Iterator operator--(int)
		{
			Iterator tmp = *this;
			--(*this);
			return tmp;
		}

		// Difference between two iterators, it counts the zeros !
		friend difference_type operator-(const Iterator& a, const Iterator& b) { return a.m_ptr - b.m_ptr; }
		friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; }
		friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }
		friend bool operator<(const Iterator& a, const Iterator& b) { return a.m_ptr < b.m_ptr;}
		friend bool operator<=(const Iterator& a, const Iterator& b) { return a < b || a == b;}

	  private:
		void skip_zeros()
		{
			while (m_ptr != m_end && *m_ptr == 0) {
				++m_ptr;
			}
		}

		void skip_zeros_backward()
		{
			while (m_ptr != m_begin && *m_ptr == 0) {
				--m_ptr;
			}
		}

		pointer m_ptr;
		const pointer m_begin;
		const pointer m_end;
	};

	skipzero_span(T* begin, T* end, row_size_t size)
		: m_begin(begin)
		, m_end(end)
		, m_size(size)
	{
	}
	skipzero_span(std::vector<T>& vec)
		: m_begin(vec.data())
		, m_end(vec.data() + vec.size())
		, m_size(vec.size())
	{
		for (T number : vec) {
			if (!number)
				m_size--;
		}
	}

	Iterator begin() const { return Iterator(m_begin, m_end); }
	Iterator end() const { return Iterator(m_end, m_end); }

	row_size_t capacity() const { return m_end - m_begin; }
	row_size_t size() const { return this->m_size; }

	T* data() { return m_begin; }
	const T* data() const { return m_begin; }

	T& front() { return *this->begin(); }
	const T& front() const { return *this->begin(); }

	T& back() { return *(--this->end()); }
	const T& back() const { return *(--this->end()); }

  private:
	T* m_begin;
	T* m_end;
	row_size_t m_size;
};

// =================================================================2DVECTOR=======================================================================
#ifdef NDEBUG
#define CHECK_BOUNDS(i) ((void)0)
#else
#define CHECK_BOUNDS(i)                                                                                                \
	do {                                                                                                               \
		if (!ROW_SIZE(i)) {                                                                                            \
			throw std::invalid_argument("Clause at given index is deleted");                                           \
		}                                                                                                              \
		if (i >= capacities.size() || i >= indexes.size()) {                                                           \
			throw std::out_of_range("Index out of range");                                                             \
		}                                                                                                              \
	} while (false)
#endif

#define ROW_SIZE(i) this->data[indexes[i]]
#define ROW_BEGIN(i) this->indexes[i] + 1
#define ROW_CAP(i) this->capacities[i]
#define ROW_END(i) ROW_BEGIN(i) + ROW_CAP(i)

/**
 * @class vector2D
 * @brief A 2D vector implementation optimized for sparse data.
 * @ingroup pl_containers
 * @tparam T The type of elements in the vector (must be arithmetic).
 */
template<typename T>
class vector2D
{
	static_assert(std::is_arithmetic<T>::value, "vector2D: Template parameter T must be an arithmetic type");

  public:
	/**
	 * @brief Accesses a row in the 2D vector.
	 * @param id The row ID.
	 * @return A skipzero_span of the row.
	 */
	inline skipzero_span<T> operator[](row_id_t id);

	/**
	 * @brief Accesses a row in the 2D vector (const version).
	 * @param id The row ID.
	 * @return A const skipzero_span of the row.
	 */
	inline skipzero_span<const T> operator[](row_id_t id) const;

	/**
	 * @brief Gets the beginning iterator for a specific row.
	 * @param id The row ID.
	 * @return Pointer to the first element of the row.
	 */
	inline T* begin(row_id_t id);

	/**
	 * @brief Gets the end iterator for a specific row.
	 * @param id The row ID.
	 * @return Pointer to one past the last element of the row.
	 */
	inline T* end(row_id_t id);

	/**
	 * @brief Gets the beginning const iterator for a specific row.
	 * @param id The row ID.
	 * @return Const pointer to the first element of the row.
	 */
	inline const T* begin(row_id_t id) const;

	/**
	 * @brief Gets the end const iterator for a specific row.
	 * @param id The row ID.
	 * @return Const pointer to one past the last element of the row.
	 */
	inline const T* end(row_id_t id) const;

	/**
	 * @brief Gets the size of a specific row.
	 * @param id The row ID.
	 * @return The number of non-zero elements in the row.
	 */
	inline row_size_t getRowSize(row_id_t id) const;

	/**
	 * @brief Gets the number of rows in the 2D vector.
	 * @return The number of rows.
	 */
	inline row_size_t getRowsCount() const { return this->indexes.size(); };

	/**
	 * @brief Adds a new row to the 2D vector.
	 * @param row The row to be added.
	 */
	void push_row(const std::vector<int>& row);

	/**
	 * @brief Adds a new row to the 2D vector using an initializer list.
	 * @param row The row to be added as an initializer list.
	 */
	void emplace_row(std::initializer_list<int> row);

	/**
	 * @brief Marks a row as deleted.
	 * @param id The ID of the row to be deleted.
	 */
	inline void delete_row(row_id_t id);

	/**
	 * @brief Deletes an element in a specific row.
	 * @param rowId The ID of the row.
	 * @param offset The offset of the element in the row.
	 * @return False if the entire row is deleted after this operation, true otherwise.
	 */
	bool delete_element(row_id_t rowId, row_size_t offset);

	/**
	 * @brief Cleans up the 2D vector by removing deleted rows and compacting data.
	 */
	void cleanup();

  private:
	std::vector<T> data;								  ///< The main data store.
	std::vector<row_size_t> capacities;					  ///< Capacities of rows.
	std::vector<data_size_t> indexes;					  ///< Starting indexes of rows in the data store.
	std::unordered_map<row_size_t, data_size_t> freeRows; ///< Map of free rows for potential reuse.
};

// ============================================================2DVECTORImplementation=================================================================

// Getters
// =======

template<typename T>
inline skipzero_span<T>
vector2D<T>::operator[](row_id_t id)
{
	CHECK_BOUNDS(id);
	return skipzero_span<T>(begin(id), end(id), ROW_SIZE(id));
}

template<typename T>
inline skipzero_span<const T>
vector2D<T>::operator[](row_id_t id) const
{
	CHECK_BOUNDS(id);
	return skipzero_span<const T>(begin(id), end(id), ROW_SIZE(id));
}

template<typename T>
inline T*
vector2D<T>::begin(row_id_t id)
{
	CHECK_BOUNDS(id);
	return (data.data() + ROW_BEGIN(id)); /* jumping the size cell */
}

template<typename T>
inline T*
vector2D<T>::end(row_id_t id)
{
	CHECK_BOUNDS(id);
	/* This iterator is not incrementable */
	return (data.data() + ROW_END(id));
}

template<typename T>
inline const T*
vector2D<T>::begin(row_id_t id) const
{
	CHECK_BOUNDS(id);
	return (data.data() + ROW_BEGIN(id)); /* jumping the size cell */
}

template<typename T>
inline const T*
vector2D<T>::end(row_id_t id) const
{
	CHECK_BOUNDS(id);
	/* This iterator is not incrementable */
	return (data.data() + ROW_END(id));
}

template<typename T>
inline row_size_t
vector2D<T>::getRowSize(row_id_t id) const
{
	CHECK_BOUNDS(id);
	return ROW_SIZE(id);
}

// Insertion
// =========

template<typename T>
void
vector2D<T>::push_row(const std::vector<int>& row)
{
	if (row.empty())
		return;
	indexes.push_back(data.size());
	data.push_back(row.size()); /* size not counting zeroes */
	capacities.push_back(row.size());
	data.insert(data.end(), row.begin(), row.end());
}

template<typename T>
void
vector2D<T>::emplace_row(std::initializer_list<int> row)
{
	if (row.size() == 0)
		return;

	indexes.push_back(data.size());
	data.push_back(row.size()); /* size not counting zeroes */
	capacities.push_back(row.size());
	data.insert(data.end(), row.begin(), row.end());
}

// Deletion
// ========

template<typename T>
inline void
vector2D<T>::delete_row(row_id_t id)
{
	CHECK_BOUNDS(id);
	ROW_SIZE(id) = 0;
}

template<typename T>
bool
vector2D<T>::delete_element(row_id_t rowId, row_size_t offset)
{
	CHECK_BOUNDS(rowId);

	data_size_t i = ROW_BEGIN(rowId) + offset;
	assert(i < data.size());

	data[i] = 0;
	if (--ROW_SIZE(rowId) == 0) /* decrementing the size */
	{
		return false;
	}
	return true;
}

template<typename T>
void
vector2D<T>::cleanup()
{
	std::vector<T> newData;
	std::vector<row_size_t> newCapacities;
	std::vector<data_size_t> newIndexes;

	// Reserve space for newData to minimize reallocations
	newData.reserve(data.size());

	for (row_id_t rowId = 0; rowId < indexes.size(); ++rowId) {
		if (ROW_SIZE(rowId) == 0) {
			continue; // Skip deleted rows
		}

		newIndexes.push_back(newData.size()); // the index of next row
		newCapacities.push_back(ROW_SIZE(rowId));
		data_size_t start = ROW_BEGIN(rowId);
		data_size_t end = ROW_END(rowId);

		for (data_size_t i = start; i < end; ++i) {
			if (data[i] != 0) {
				newData.push_back(data[i]);
			}
		}
	}

	// Move new data to existing vectors
	data = std::move(newData);
	capacities = std::move(newCapacities);
	indexes = std::move(newIndexes);

	// Release unused memory, `shrink_to_fit` might reallocate
	data.shrink_to_fit();
	capacities.shrink_to_fit();
	indexes.shrink_to_fit();
}

// template <typename T>
// void vector2D<T>::cleanup()
// {
//     // Shrink all vectors
//     this->data.shrink_to_fit();
//     this->indexes.shrink_to_fit();
//     this->capacities.shrink_to_fit();

//     // A map size to index is to be generated by iterating over all the clauses
//     std::unordered_map<row_size_t,data_size_t> rowBySize;
//     // Keep indexes of deleted,
//     std::unordered_map<data_size_t,row_size_t> deletedRows;
//     // In the loop: Check if the row need to be shifted because of zeroes before copying it
//     row_id_t rowCount = this->indexes.size();
//     for(row_id_t rowId = 0; rowId < rowCount; rowId++)
//     {
//         if(!ROW_SIZE(rowId))
//         {
//             // Delete row
//             deletedRows.insert({rowId, ROW_CAP(rowId)});
//         }
//         else if(ROW_CAP(rowId) != ROW_SIZE(rowId))
//         {
//             row_size_t nonZeros = ROW_SIZE(rowId);
//             data_size_t idx = ROW_BEGIN(rowId);
//             data_size_t lastNonZero = ROW_BEGIN(rowId);
//             // Compact the row
//             while(!nonZeros)
//             {
//                 if(this->data[idx] != 0)
//                 {
//                     this->data[lastNonZero] = this->data[idx];
//                     this->data[idx] = this->data[lastNonZero];
//                     lastNonZero++;
//                     nonZeros--;
//                 }
//                 idx++;
//             }
//         }
//         indexesByCapacity.insert({ROW_SIZE(rowId), rowId});
//     }

//     row_id_t cutId = rowCount;

//     // Fill all the deleted rows capacities the most optimal way.
//     for(auto& drow : deletedRows)
//     {
//         /* Algorithm:
//         - Check if the index drow.first is the before cutId, if not continue
//         - Find a row or multiple rows with index > drow.fisrt that could fit in drow.second, using rowBySize map
//         - If one of the filling rows is at index cutId - 1, update cutId to the index of the row (if multiple rows,
//         check if they are not consecutive)
//         - Update the indexes and capacities vector accordingly. The capacities become their sizes, since we are
//         looking for perfect fits
//         - Update the deletedRows with the new gaps created by the moved rows
//         */
//     }
//     // Cut data from indexes.at(cutId)
// }
}