# Clause Management in Painless: ClauseExchange and ClauseBuffer
[TOC]
## ClauseExchange

[ClauseExchange](@ref ClauseExchange) is a memory-efficient class for storing and managing SAT clauses of varying sizes. It uses a [flexible array member](http://wiki.c2.com/?StructHack) for optimized memory usage and intrusive reference counting for efficient memory management.

### Key Features

1. **Memory Layout**
   - Uses a flexible array member (`lits[0]`) for storing literals
   - Fixed-size header containing:
     ```cpp
     unsigned int lbd;          // Literal Block Distance
     int from;                  // Source identifier
     unsigned int size;         // Number of literals
     std::atomic<unsigned int> refCounter; // Reference count
     ```

2. **Smart Pointer Management**
   - Uses `boost::intrusive_ptr` for reference counting
   - Type alias: `using ClauseExchangePtr = boost::intrusive_ptr<ClauseExchange>`
   - Custom reference counting through:
     ```cpp
     void intrusive_ptr_add_ref(ClauseExchange* ce);
     void intrusive_ptr_release(ClauseExchange* ce);
     ```

3. **Iterator Support**
   - STL-compatible iterators
   - Supports range-based for loops
   ```cpp
   // Begin/end iterators
   int* begin();
   int* end();
   const int* begin() const;
   const int* end() const;
   ```

4. **Utility Functions**
   ```cpp
   // Array-style access
   int& operator[](unsigned int index);
   const int& operator[](unsigned int index) const;
   
   // Sorting functions
   void sortLiterals();
   void sortLiteralsDescending();
   
   // Debug support
   std::string toString() const;
   ```

5. **LBD Handling**
   - Forces LBD ≥ 2 for non-unit clauses
   - Unit clauses can have LBD of 0 or 1 (I am thinking of forcing it to 0).

### Usage Example

```cpp
// Create a clause with 3 literals
auto clause = ClauseExchange::create(3, 2, 1);  // size=3, lbd=2, from=1

// Fill literals
clause->lits[0] = 1;
clause->lits[1] = -2;
clause->lits[2] = 3;

// Or using iterators
for (int& lit : *clause) {
    // Process literals
}
```

## ClauseBuffer

[ClauseBuffer](@ref ClauseBuffer) is a thread-safe buffer for exchanging clauses between producers and consumers. It's built on top of `boost::lockfree::queue` for efficient multiple producers-multiple consumers operations.

### Key Features

1. **Lock-Free Operations**
   - Based on `boost::lockfree::queue`
   - Non-fixed size queue
   - Multiple producer/consumer safe

2. **Atomic Size Tracking**
   ```cpp
   std::atomic<size_t> m_size;  // Current number of elements
   ```

3. **Clause Addition**
   ```cpp
   // Add single clause
   bool addClause(ClauseExchangePtr clause);
   
   // Add multiple clauses
   size_t addClauses(const std::vector<ClauseExchangePtr>& clauses);
   
   // Bounded versions (with capacity limit)
   bool tryAddClauseBounded(ClauseExchangePtr clause);
   size_t tryAddClausesBounded(const std::vector<ClauseExchangePtr>& clauses);
   ```

4. **Clause Retrieval**
   ```cpp
   // Get single clause
   bool getClause(ClauseExchangePtr& clause);
   
   // Get all available clauses
   void getClauses(std::vector<ClauseExchangePtr>& clauses);
   ```

5. **Utility Functions**
   ```cpp
   // Get current size
   size_t size() const;
   
   // Check if empty
   bool empty() const;
   
   // Clear all clauses
   void clear();
   ```

### Usage Example

```cpp
// Create buffer with initial capacity
ClauseBuffer buffer(1000);

// Producer code
auto clause = ClauseExchange::create(/* ... */);
if (buffer.addClause(clause)) {
    // Successfully added
}

// Consumer code
ClauseExchangePtr received;
if (buffer.getClause(received)) {
    // Process received clause
}

// Batch operations
std::vector<ClauseExchangePtr> clauses;
buffer.getClauses(clauses);  // Get all available clauses
```

## toRawPtr and fromRawPtr

ClauseBuffer internally uses `boost::lockfree::queue` which operates on raw pointers, while the external interface uses `ClauseExchangePtr` (smart pointers). The `toRawPtr` and `fromRawPtr` methods of ClauseExchange bridge this gap safely.

```cpp
// In ClauseExchange
ClauseExchange* toRawPtr() {
    refCounter.fetch_add(1, std::memory_order_relaxed);
    return this;
}

// It is static, but we interact with the same ClauseExchange instance, since we have its pointer.
static ClauseExchangePtr fromRawPtr(ClauseExchange* ptr) {
    return ClauseExchangePtr(ptr, false); // false is to not increment the reference counter, since it was incremented at toRawPtr().
}
```

- **Usage in ClauseBuffer :**

```cpp
bool ClauseBuffer::addClause(ClauseExchangePtr clause) {
    // Convert to raw pointer while incrementing ref count
    ClauseExchange* raw = clause->toRawPtr();
    
    if (queue.push(raw)) {
        m_size.fetch_add(1, std::memory_order_release);
        return true;
    } else {
        // Reference count is decremented since the returned ClauseExchangePtr is not stored
        ClauseExchange::fromRawPtr(raw);
        return false;
    }
}

bool ClauseBuffer::getClause(ClauseExchangePtr& clause) {
    ClauseExchange* raw;
    if (queue.pop(raw)) {
        // Convert back to smart pointer without incrementing ref count
        clause = ClauseExchange::fromRawPtr(raw);
        m_size.fetch_sub(1, std::memory_order_release);
        return true;
    }
    return false;
}
```
   - `toRawPtr()`: Increments reference count before storing in queue
   - `fromRawPtr()`: Creates smart pointer without additional increment

> [!important]
> A ClauseExchangePtr should not be gotten through `.get()`:
>  ```cpp
>   // DON'T: Direct raw pointer usage
>   ClauseExchange* raw = clause.get();  // Wrong! Bypasses ref counting
>   // DO: Use provided conversion methods
>   ClauseExchange* raw = clause->toRawPtr();
>   ```
> Be careful to not leave hanging raw pointers, otherwise the ClauseExchange instance will never be freed.


- **Example Flow :**

```cpp
// Life cycle of a clause through the buffer
ClauseBuffer queue(10)
{
    ClauseExchangePtr clause = ClauseExchange::create(3);  // ref_count = 1
    queue.addClause(raw);                                  // ref_count = 2
    // Original clause goes out of scope                   // ref_count = 1
}
ClauseExchangePtr newClause;        
queue.getClause(newClause);                            // Same ref_count = 1
// newClause handles final cleanup                     // ref_count = 0
```