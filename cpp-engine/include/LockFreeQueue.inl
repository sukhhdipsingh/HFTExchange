// implementation of LockFreeQueue<T, Capacity>
// included by LockFreeQueue.h

#pragma once

#include "LockFreeQueue.h"

namespace hft {

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::push(const T &item) noexcept {
  // We relaxed the load of head_ because the producer is the only thread that
  // modifies it. It is safe to use relaxed ordering here.
  const std::size_t current_head = head_.load(std::memory_order_relaxed);

  // We must load tail_ with acquire to ensure we see the changes made by the
  // pop() thread.
  const std::size_t current_tail = tail_.load(std::memory_order_acquire);

  // If the buffer is full, we can't push.
  // Since capacity is a power of 2, masking the difference works for tracking
  // fullness
  if ((current_head - current_tail) == Capacity) {
    return false;
  }

  // Write the actual data.
  buffer_[current_head & K_MASK] = item;

  // Publish the new head value to the consumer thread.
  // Release memory order ensures the data written to buffer_ is visible
  // before the incremented head becomes visible.
  head_.store(current_head + 1, std::memory_order_release);
  return true;
}

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::pop(T &item) noexcept {
  // We relaxed the load of tail_ because the consumer is the only thread that
  // modifies it
  const std::size_t current_tail = tail_.load(std::memory_order_relaxed);

  // We must load head_ with acquire to ensure we see the push() thread's writes
  // to buffer_
  const std::size_t current_head = head_.load(std::memory_order_acquire);

  // If the buffer is empty, we can't
  if (current_head == current_tail) {
    return false;
  }

  // Read the actual data.
  item = buffer_[current_tail & K_MASK];

  // Publish the new tail value to the producer thread
  tail_.store(current_tail + 1, std::memory_order_release);
  return true;
}

template <typename T, std::size_t Capacity>
std::size_t LockFreeQueue<T, Capacity>::size() const noexcept {
  // Note: this is approximate in a multi-threaded context.
  // Head and tail can change while this is calculating
  const std::size_t current_head = head_.load(std::memory_order_relaxed);
  const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
  return current_head - current_tail;
}

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::empty() const noexcept {
  return size() == 0;
}

} // namespace hft
