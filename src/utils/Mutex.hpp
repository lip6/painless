#pragma once
#include <mutex>
#include <shared_mutex>

// This is because I forget to give a variable name sometimes, and I get buggy
// compilable code
#define UNIQUE_LOCK(mutex_type, mutex, lockname)                               \
  std::unique_lock<mutex_type> l##lockname(mutex)

#define UNIQUE_LOCK_DEFER(mutex_type, mutex, lockname)                         \
  std::unique_lock<mutex_type> l##lockname(mutex, std::defer_lock)

#define SHARED_LOCK(mutex_type, mutex, lockname)                               \
  std::shared_lock<mutex_type> l##lockname(mutex)

#define LOCK_GUARD(mutex_type, mutex, lockname)                                \
  std::lock_guard<mutex_type> l##lockname(mutex)

#define SCOPED_LOCK(lockname, first_mutex, ...)                                \
  std::scoped_lock l##lockname(first_mutex, ##__VA_ARGS__)
