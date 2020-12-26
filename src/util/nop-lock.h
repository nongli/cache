#pragma once
// A simple NOP lock for performance testing in cases where a single thread is
// used or where coarse grained locking is already protecting access.

namespace cache {
// A nop lock for when fine-grained locking in LRU makes no sense since coarse
// grained locking is used externally.
class NopLock final {
public:
  constexpr NopLock() = default;
  constexpr void lock() {}
  constexpr void unlock() {}
  constexpr bool try_lock() { return true; }
};
} // namespace cache
