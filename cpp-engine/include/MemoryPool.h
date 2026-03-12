#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace hft {

// Fixed-size slab allocator. Allocates a single contiguous block at
// construction time and hands out slots from it. No malloc/new/free on the
// critical path.
//
// Template params:
//   T        — the type being pooled (must be trivially destructible for now)
//   Capacity — max number of objects the pool can hold at once
//
// Thread safety: not thread-safe by itself. The caller is responsible for
// synchronization, or you wrap it behind a lock-free queue.
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

  // grab a slot from the pool. returns a pointer into the pre-allocated
  // storage. returns nullptr if the pool is exhausted.
  // this should never return nullptr on the hot path — if it does,
  // your Capacity is too small.
  [[nodiscard]] T *acquire() noexcept;

  // return a slot to the pool. the pointer must have come from acquire().
  // calling this with any other pointer is undefined behavior.
  void release(T *ptr) noexcept;

  // acquire a slot and construct a T in-place with the given args.
  // this is the right way to use the pool with non-trivial types.
  // returns nullptr if the pool is exhausted.
  template <typename... Args>
  [[nodiscard]] T *construct(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>);

  // call T's destructor and return the slot to the pool.
  // use this instead of release() when T has a non-trivial destructor.
  void destroy(T *ptr) noexcept;

  // reset the pool back to fully empty — all slots available.
  // does NOT call destructors. only safe if all outstanding pointers
  // have already been released or you know they won't be used again.
  // mainly useful between test cases.
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
  // each Slot holds either an object or a pointer to the next free slot.
  // the union means we're not wasting memory on the free list — we reuse
  // the object storage itself to store the free list pointer.
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
