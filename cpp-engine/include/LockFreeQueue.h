#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>

namespace hft {

// SPSC (Single Producer, Single Consumer) Lock-Free Ring Buffer.
// Template params:
//   T        - Data type. Must be default constructible and copiable.
//   Capacity - Must be a power of 2 so we can use bitwise AND instead of
//   modulo
template <typename T, std::size_t Capacity> class LockFreeQueue {
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");
  static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
  LockFreeQueue() : head_(0), tail_(0) {}
  ~LockFreeQueue() = default;

  LockFreeQueue(const LockFreeQueue &) = delete;
  LockFreeQueue &operator=(const LockFreeQueue &) = delete;

  // Pushes an item onto the queue. Returns false if the queue is full.
  // Called only by the producer thread.
  bool push(const T &item) noexcept;

  // Pops an item off the queue. Returns false if the queue is empty.
  // Called only by the consumer thread
  bool pop(T &item) noexcept;

  // Current approximate size of the queue.
  [[nodiscard]] std::size_t size() const noexcept;

  // Is the queue currently empty?
  [[nodiscard]] bool empty() const noexcept;

private:
  // Mask to avoid slow modulo instructions on index wrap
  static constexpr std::size_t K_MASK = Capacity - 1;

  // The raw storage array
  std::array<T, Capacity> buffer_;

  // Align to 64 bytes to prevent false sharing between producer and consumer.
  // If head and tail share a cache line, the constant invalidations will
  // cripple performance

  // alignas(64) ensures each atomic is on its own cache line.
  // This prevents false sharing between the producer thread (writing head_)
  // and the consumer thread (writing tail_), which would otherwise cripple throughput.
  alignas(64) std::atomic<std::size_t> head_;

  // Written by consumer, read by producer
  alignas(64) std::atomic<std::size_t> tail_;
};

} // namespace hft

#include "LockFreeQueue.inl"
