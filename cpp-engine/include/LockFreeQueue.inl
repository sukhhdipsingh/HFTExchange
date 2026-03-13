// implementation of LockFreeQueue<T, Capacity>
// included by LockFreeQueue.h

#pragma once

#include "LockFreeQueue.h"

namespace hft {

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::push(const T &item) noexcept {
  // Relaxed: Modified by producer only.
  const std::size_t current_head = head_.load(std::memory_order_relaxed);

  // Acquire: Synchronize with consumer's pop.
  const std::size_t current_tail = tail_.load(std::memory_order_acquire);

  // Cap is pow2, mask handles fullness math
  if ((current_head - current_tail) == Capacity) {
    return false;
  }

  // Write the actual data.
  buffer_[current_head & K_MASK] = item;

  // Write data before incrementing head (Release).
  head_.store(current_head + 1, std::memory_order_release);
  return true;
}

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::pop(T &item) noexcept {
  // Relaxed: Modified by consumer only.
  const std::size_t current_tail = tail_.load(std::memory_order_relaxed);

  // Acquire: Synchronize with producer's push.
  const std::size_t current_head = head_.load(std::memory_order_acquire);

  // If the buffer is empty, we can't
  if (current_head == current_tail) {
    return false;
  }

  // Read the actual data.
  item = buffer_[current_tail & K_MASK];

  // Publish tail update (Release).
  tail_.store(current_tail + 1, std::memory_order_release);
  return true;
}

template <typename T, std::size_t Capacity>
std::size_t LockFreeQueue<T, Capacity>::size() const noexcept {
  // Approximate size
  const std::size_t current_head = head_.load(std::memory_order_relaxed);
  const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
  return current_head - current_tail;
}

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::empty() const noexcept {
  return size() == 0;
}

} // namespace hft
