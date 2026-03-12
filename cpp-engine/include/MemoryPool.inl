// implementation of MemoryPool<T, Capacity>
// this file is included at the bottom of MemoryPool.h — don't include it
// directly.

#pragma once

#include "MemoryPool.h"
#include <utility>

namespace hft {

// -------------------------------------------------------------------------
// internal helper — builds or rebuilds the free list.
// links slots from back to front so that the first acquire() returns slot[0].
// that ordering is arbitrary but makes debugging easier since addresses
// go up as you allocate.
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void MemoryPool<T, Capacity>::init_free_list() noexcept {
  free_list_head_ = nullptr;
  in_use_count_ = 0;

  for (std::size_t i = Capacity; i > 0; --i) {
    storage_[i - 1].next_free = free_list_head_;
    free_list_head_ = &storage_[i - 1];
  }
}

// -------------------------------------------------------------------------
// constructor
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
MemoryPool<T, Capacity>::MemoryPool() {
  init_free_list();
}

// -------------------------------------------------------------------------
// acquire — pop one slot off the free list head. O(1).
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
T *MemoryPool<T, Capacity>::acquire() noexcept {
  if (free_list_head_ == nullptr) {
    // pool exhausted. caller should never hit this on the hot path.
    // if you do, increase Capacity.
    return nullptr;
  }

  Slot *slot = free_list_head_;
  free_list_head_ = slot->next_free;
  ++in_use_count_;

  // reinterpret the storage bytes as a T*. the caller is responsible for
  // placement-new if T is non-trivial — or just use construct() below.
  return reinterpret_cast<T *>(slot->storage);
}

// -------------------------------------------------------------------------
// release — push the slot back onto the free list head. O(1).
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void MemoryPool<T, Capacity>::release(T *ptr) noexcept {
  assert(ptr != nullptr && "releasing a null pointer makes no sense");

  // make sure the pointer actually lives inside our storage block.
  // catching this in debug saves hours of debugging later.
  assert(contains(ptr) && "pointer did not come from this pool");

  Slot *slot = reinterpret_cast<Slot *>(ptr);
  slot->next_free = free_list_head_;
  free_list_head_ = slot;
  --in_use_count_;
}

// -------------------------------------------------------------------------
// construct — acquire a slot and placement-new a T in it.
// this is the right way to use the pool if T has a constructor.
// for plain structs where you just assign fields, acquire() is fine.
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
template <typename... Args>
T *MemoryPool<T, Capacity>::construct(Args &&...args) noexcept(
    std::is_nothrow_constructible_v<T, Args...>) {

  T *ptr = acquire();
  if (ptr == nullptr) {
    return nullptr;
  }

  // Se il costruttore di T può lanciare eccezioni, dobbiamo catturarle
  // per non "perdere" lo slot di memoria per sempre
  if constexpr (!std::is_nothrow_constructible_v<T, Args...>) {
    try {
      ::new (ptr) T(std::forward<Args>(args)...);
    } catch (...) {
      release(ptr);
      throw;
    }
  } else {
    ::new (ptr) T(std::forward<Args>(args)...);
  }

  return ptr;
}

// -------------------------------------------------------------------------
// destroy — call destructor and return the slot to the pool.
// use this if T has a non-trivial destructor and you used construct().
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void MemoryPool<T, Capacity>::destroy(T *ptr) noexcept {
  assert(ptr != nullptr && "destroying a null pointer makes no sense");
  assert(contains(ptr) && "pointer did not come from this pool");

  // call the destructor explicitly. this does NOT free memory —
  // that's what release() does below.
  ptr->~T();
  release(ptr);
}

// -------------------------------------------------------------------------
// reset — rebuild free list from scratch. all slots become available again.
// does NOT call any destructors. make sure outstanding pointers are gone.
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void MemoryPool<T, Capacity>::reset() noexcept {
  init_free_list();
}

// -------------------------------------------------------------------------
// contains — returns true if ptr points into our storage array.
// uses pointer arithmetic, no loops.
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
bool MemoryPool<T, Capacity>::contains(const T *ptr) const noexcept {
  const auto *p = reinterpret_cast<const std::byte *>(ptr);
  const auto *begin = reinterpret_cast<const std::byte *>(&storage_[0]);
  const auto *end = reinterpret_cast<const std::byte *>(&storage_[Capacity]);
  
  if (p >= begin && p < end) {
    // verifica aggiuntiva: controlla che punti esattamente all'inizio di uno slot
    // evita falsi positivi se uno passa un puntatore all'interno di T (es. &my_t.field2)
    return (p - begin) % sizeof(Slot) == 0;
  }
  return false;
}

// -------------------------------------------------------------------------
// available / in_use / is_valid — simple accessors
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
std::size_t MemoryPool<T, Capacity>::available() const noexcept {
  return Capacity - in_use_count_;
}

template <typename T, std::size_t Capacity>
std::size_t MemoryPool<T, Capacity>::in_use() const noexcept {
  return in_use_count_;
}

template <typename T, std::size_t Capacity>
bool MemoryPool<T, Capacity>::is_valid() const noexcept {
  return in_use_count_ <= Capacity;
}

// -------------------------------------------------------------------------
// free_list_length — walks the list and counts nodes. O(N).
// only call this in tests. if it disagrees with available(), the list is
// corrupt — likely a double-release or a bad pointer.
// -------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
std::size_t MemoryPool<T, Capacity>::free_list_length() const noexcept {
  std::size_t count = 0;
  const Slot *node = free_list_head_;

  while (node != nullptr) {
    ++count;
    node = node->next_free;

    // if count exceeds Capacity the list is cyclic or
    // corrupt. bail out to avoid infinite loop.
    if (count > Capacity) {
      assert(false &&
             "free list is corrupt — cycle or invalid pointer detected");
      return count;
    }
  }

  return count;
}

} // namespace hft
