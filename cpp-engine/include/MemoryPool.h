#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace hft {

// Fixed-size slab allocator. Zero-allocation on critical path.
// Not thread-safe. Caller must synchronize.
// T must be trivially destructible for now.
template <typename T, std::size_t Capacity> class MemoryPool {
public:
  static_assert(Capacity > 0, "pool capacity must be > 0");
  static_assert(Capacity <= 1 << 20,
                "cap it at 1M slots, something is wrong if you need more");

  MemoryPool();
  ~MemoryPool() = default;

  // not copyable or movable — the pool owns its storage
  MemoryPool(const MemoryPool &) = delete;
  MemoryPool &operator=(const MemoryPool &) = delete;
  MemoryPool(MemoryPool &&) = delete;
  MemoryPool &operator=(MemoryPool &&) = delete;

  // Returns nullptr if exhausted (Capacity violation).
  [[nodiscard]] T *acquire() noexcept;

  // Return slot to pool. Pointer must be from acquire().
  void release(T *ptr) noexcept;

  // Acquire slot and construct T in-place. Returns nullptr if exhausted.
  template <typename... Args>
  [[nodiscard]] T *construct(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>);

  // Call T's destructor and return slot to pool.
  void destroy(T *ptr) noexcept;

  // Reset pool back to empty. Does NOT call destructors. Safe only if all ptrs released.
  void reset() noexcept;

  // returns true if the pointer came from this pool's storage block.
  // useful for assertions at call sites.
  [[nodiscard]] bool contains(const T *ptr) const noexcept;

  // how many slots are currently available
  [[nodiscard]] std::size_t available() const noexcept;

  // how many slots are currently in use
  [[nodiscard]] std::size_t in_use() const noexcept;

  // mostly for testing — true if the pool was initialized correctly
  [[nodiscard]] bool is_valid() const noexcept;

  // walks the free list and counts the nodes. O(N). only use in tests/debug.
  // if this returns a number different from available(), the free list is
  // corrupt.
  [[nodiscard]] std::size_t free_list_length() const noexcept;

private:
  // Union overlays free list pointer on unallocated storage.
  union Slot {
    alignas(T) std::byte storage[sizeof(T)];
    Slot *next_free;
  };

  // the actual storage. aligned to 64 bytes to land on a cache line boundary.
  // all slots are in a flat array so they're contiguous in memory.
  alignas(64) std::array<Slot, Capacity> storage_;

  // head of the free list. points into storage_.
  Slot *free_list_head_;

  // track how many are in use — useful for debugging and assertions
  std::size_t in_use_count_;

  // helper to build the free list. called by the constructor and reset().
  void init_free_list() noexcept;
};

} // namespace hft

// include the implementation here since it's a template
#include "MemoryPool.inl"
