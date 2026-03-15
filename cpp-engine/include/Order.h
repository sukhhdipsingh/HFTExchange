#pragma once

#include <cstdint>

namespace hft {

enum class Side : uint8_t { 
  Buy = 0, 
  Sell = 1 
};

// Represents a resting limit order in the matching engine.
// Designed to be allocated exclusively from the MemoryPool.
// Field order: largest to smallest to eliminate implicit padding.
// Previous layout (id, price, qty, timestamp, side, next, prev) had
// 4-byte pad after qty and 7-byte pad after side → sizeof = 56 bytes.
// Repacked layout below → sizeof = 48 bytes, saving 8MB on a 1M-slot pool.
// Every saved byte matters during deep book traversals (L1 cache is 64 bytes).
struct Order {
  uint64_t order_id;
  uint64_t price;      // fixed-point ticks (e.g. 100.50 → 10050)
  uint64_t timestamp;  // monotonic sequence for price-time priority
  Order   *next{nullptr};
  Order   *prev{nullptr};
  uint32_t quantity;
  Side     side;
  // 3 bytes implicit pad here — unavoidable for struct alignment to 8 bytes
};

static_assert(sizeof(Order) == 48,
    "Order struct changed size — recheck pool memory math and cache-line assumptions");

} // namespace hft
