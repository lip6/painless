#pragma once

#include <algorithm>
#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>

namespace pl {
// row
// ======

// A temporary object for ease of use, extension and avoiding errors in indices
// when reading a row in the formula
template<typename T = int>
class row_view
{
  // Ensure T is an integer type
  static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value,
                "T must be an integer type but not bool");
  using size_type = std::make_unsigned_t<T>;
  using value_type = T;
  using iterator = T*;
  using const_iterator = T const*;

public:
  row_view()
    : m_cls_ptr(nullptr)
  {
  }
  explicit row_view(T* ptr)
    : m_cls_ptr(ptr)
  {
    // assert(ptr && *ptr > 0); // assert (size > 0)
  }
  ~row_view() = default;

  size_type size() const { return (m_cls_ptr != nullptr) ? *m_cls_ptr : 0; }
  T* data() { return m_cls_ptr + 1; }
  bool empty() const { return size() == 0; }

  iterator begin() { return (size() > 0) ? m_cls_ptr + 1 : m_cls_ptr; }
  iterator end() { return begin() + size(); }
  const_iterator begin() const
  {
    return (size() > 0) ? m_cls_ptr + 1 : m_cls_ptr;
  }
  const_iterator end() const { return begin() + size(); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  value_type& operator[](size_type index)
  {
    assert(index < size());
    return m_cls_ptr[index +
                     1]; // to compensate the size's slot (becomes 1-based)
  }

  const value_type& operator[](size_type index) const
  {
    assert(index < size());
    return m_cls_ptr[index + 1];
  }

  value_type& at(size_type index)
  {
    if (index >= size()) {
      throw std::out_of_range("row_t::at");
    }
    return (*this)[index];
  }

  const value_type& at(size_type index) const
  {
    if (index >= size()) {
      throw std::out_of_range("row_t::at");
    }
    return (*this)[index];
  }

  // Check only pointer since the end has size
  /**
   * N complexity for sorted, and N^2 for non sorted
   */
  bool operator==(row_view<T>& other)
  {
    if (this->size() != other.size())
      return false;

    // Other's index of last equal and same position value
    uint oi = 0;
    for (uint i = 0; i < this->size(); i++) {
      // If same element at same index
      if (this->at(i) == other.at(oi)) {
        oi++;
        continue;
      }

      // Else check if the ith element is in the remaining elements in other
      if (std::find(other.begin() + oi + 1, other.end(), this->at(i)) ==
          other.end())
        return false; // not found -> not equal
    }

    return true;
  }
  bool operator!=(row_view<T>& other) { return !(this->operator==(other)); }

private:
  T* m_cls_ptr;
};

// The row Iterator, offers iteration interface with the row view as the
// frontend. Meaning, the iterator will go from row to row, with the row_view as
// a frontend interface for the data
template<typename T = int>
class csr_matrix_iterator
{
  static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value,
                "T must be an integer type but not bool");

public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = row_view<T>;
  using size_type = std::size_t;
  // Return a view value on demande (created each time). I thought not managing
  // the view in the iterator would be more readable, and since the view is only
  // a ptr wrapper, it shouldn't hurt performance Having it on demand also
  // enable us to not manage the state of the view pointer when the iterator is
  // incremented
  using pointer = row_view<T>;
  using reference = row_view<T>;

  csr_matrix_iterator()
    : m_base_ptr(nullptr)
    , m_offset_ptr(nullptr)
  {
  }
  explicit csr_matrix_iterator(T* cls_ptr, size_type const* offset_ptr)
    : m_base_ptr(cls_ptr)
    , m_offset_ptr(offset_ptr)
  {
  }

  // Dereferencing
  value_type operator*() { return row_view<T>(m_base_ptr + *m_offset_ptr); }
  pointer operator->() { return row_view<T>(m_base_ptr + *m_offset_ptr); }

  // Incrementing
  // Pre
  csr_matrix_iterator<T>& operator++()
  {
    if (m_offset_ptr) {
      m_offset_ptr++;
    }

    return *this;
  }

  // Post
  csr_matrix_iterator<T> operator++(int)
  {
    csr_matrix_iterator ret = *this;
    ++(*this);
    return ret;
  }

  // Comparison
  bool operator==(const csr_matrix_iterator<T>& other) const
  {
    return m_base_ptr == other.m_base_ptr && m_offset_ptr == other.m_offset_ptr;
  }

  bool operator!=(const csr_matrix_iterator<T>& other) const
  {
    return !(this->operator==(other));
  }

private:
  T* m_base_ptr;
  size_type const* m_offset_ptr;
};

// Formula
// =======
// CNF formula holder, [size][lit][lit]...[size][lit]...
// TODO a version without the offset index for better memory efficiency
template<typename T = int>
class csr_matrix_t : private std::vector<T>
{
  static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value,
                "T must be an integer type but not bool");

public:
  using value_type = T;
  using pointer_type = T*;
  using size_type = std::size_t;
  using iterator = csr_matrix_iterator<T>;
  using const_iterator = csr_matrix_iterator<const T>;
  using row_type = row_view<T>;
  using const_row_type = row_view<const T>;

  csr_matrix_t() {}

  // Copy constructor (fixed)
  csr_matrix_t(const csr_matrix_t& other)
    : std::vector<T>(other)
    , m_row_to_offset(other.m_row_to_offset)
  {
  }

  csr_matrix_t(csr_matrix_t&& other) noexcept
    : std::vector<T>(std::move(other))
    , m_row_to_offset(std::move(other.m_row_to_offset))
  {
  }

  // Assignment operator
  csr_matrix_t& operator=(const csr_matrix_t& other)
  {
    if (this != &other) {
      std::vector<T>::operator=(other);
      m_row_to_offset = other.m_row_to_offset;
    }
    return *this;
  }

  csr_matrix_t& operator=(csr_matrix_t&& other) noexcept
  {
    if (this != &other) {
      std::vector<value_type>::operator=(std::move(other));
      m_row_to_offset = std::move(other.m_row_to_offset);
    }
    return *this;
  }

  // Size information
  size_type gross_size() const { return std::vector<T>::size(); }
  size_type row_count() const { return m_row_to_offset.size(); }
  size_type net_size() const { return gross_size() - row_count(); }
  value_type const* data() const { return std::vector<T>::data(); }
  size_type const* index_data() const { return this->m_row_to_offset.data(); }
  value_type* data() { return std::vector<T>::data(); }
  size_type* index_data() { return this->m_row_to_offset.data(); }

  bool empty() const { return row_count() == 0; }

  // Setters

  // Add a row from iterator range
  template<typename InputIt>
  void push_row(InputIt begin, InputIt end)
  {
    T size = static_cast<T>(std::distance(begin, end));

    // New indexed value
    m_row_to_offset.push_back(gross_size());

    // Store row size first
    std::vector<T>::push_back(size);

    // Store literals
    for (auto it = begin; it != end; ++it) {
      std::vector<T>::push_back(*it);
    }
  }

  // Add a row from initializer list
  void push_row(std::initializer_list<T> literals)
  {
    push_row(literals.begin(), literals.end());
  }

  void push(T element)
  {
    if (element == 0) {
      push_row(m_temp_row.begin(), m_temp_row.end());
      m_temp_row.clear();
    } else {
      m_temp_row.push_back(element);
    }
  }

  // Getters

  // Access last row
  row_type back()
  {
    assert(!empty());
    return row_type(this->data() + m_row_to_offset.back());
  }

  // Access row by index (this accesses the i-th row, not by offset)
  row_type get_nth_row(size_type row_index)
  {
    assert(row_index < row_count());
    return row_type(this->data() + m_row_to_offset[row_index]);
  }

  row_type operator[](size_type row_index) { return get_nth_row(row_index); }

  row_type row_at(size_type row_index)
  {
    if (row_index >= row_count()) {
      throw std::out_of_range("csr_matrix_t::at_row");
    }
    return get_nth_row(row_index);
  }

  row_type row_at_offset(size_type offset)
  {
    return row_type(this->data() + offset);
  }

  std::span<value_type> span_at(size_type row_index)
  {
    if (row_index >= row_count()) {
      throw std::out_of_range("csr_matrix_t::at_row");
    }
    pointer_type ptr = this->data() + m_row_to_offset[row_index];
    // Size is at ptr, data starts at ptr+1
    return std::span<value_type>(ptr + 1, *ptr);
  }

  // Access last row
  const_row_type back() const
  {
    assert(!empty());
    return const_row_type(this->data() + m_row_to_offset.back());
  }

  // Access row by index (this accesses the i-th row, not by offset)
  const_row_type get_nth_row(size_type row_index) const
  {
    assert(row_index < row_count());
    return const_row_type(this->data() + m_row_to_offset[row_index]);
  }

  const_row_type operator[](size_type row_index) const
  {
    return get_nth_row(row_index);
  }

  const_row_type row_at(size_type row_index) const
  {
    if (row_index >= row_count()) {
      throw std::out_of_range("csr_matrix_t::at_row");
    }
    return get_nth_row(row_index);
  }

  const_row_type row_at_offset(size_type offset) const
  {
    return const_row_type(this->data() + offset);
  }

  std::span<const value_type> span_at(size_type row_index) const
  {
    if (row_index >= row_count()) {
      throw std::out_of_range("csr_matrix_t::at_row");
    }
    pointer_type ptr = this->data() + m_row_to_offset[row_index];
    // Size is at ptr, data starts at ptr+1
    return std::span<const value_type>(ptr + 1, *ptr);
  }

  // Iterator access
  iterator begin()
  {
    return empty() ? end()
                   : iterator(this->data(), this->m_row_to_offset.data());
  }

  iterator end()
  {
    return iterator(this->data(),
                    this->m_row_to_offset.data() + this->row_count());
  }

  const_iterator begin() const
  {
    return empty() ? end()
                   : const_iterator(std::vector<T>::data(),
                                    this->m_row_to_offset.data());
  }

  const_iterator end() const
  {
    return const_iterator(this->data(),
                          this->m_row_to_offset.data() + this->row_count());
  }

  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // Helpers

  /*
  Return a new ordering of the rows using their size, from the smallest to the
  largest However, be sure to only use one ordering of the rows in the
  occurrence lists
  */
  void sort_rows_index()
  {
    // std::vector<size_type> new_order(m_row_to_offset);

    std::sort(m_row_to_offset.begin(),
              m_row_to_offset.end(),
              [this](size_type a, size_type b) {
                return this->row_at_offset(a).size() <
                       this->row_at_offset(b).size();
              });
  }

  // Clear all rows
  void clear()
  {
    this->std::vector<value_type>::clear();
    m_row_to_offset.clear();
  }

  // Reserve
  void reserve(ulong elementCount, ulong rowCount = 0)
  {
    this->std::vector<value_type>::reserve(elementCount);
    m_row_to_offset.reserve(rowCount);
  }

  void shrink_to_fit()
  {
    this->m_row_to_offset.shrink_to_fit();
    this->shrink_to_fit();
  }

private:
  std::vector<size_type> m_row_to_offset;
  std::vector<T> m_temp_row;
};

}