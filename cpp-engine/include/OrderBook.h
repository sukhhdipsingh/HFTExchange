#pragma once

#include "LockFreeQueue.h"
#include "MemoryPool.h"
#include "Order.h"
#include <array>
#include <cstdint>

namespace hft {

// Emitted on every fill. Consumed by the Java risk layer via the outbound queue.
struct TradeMsg {
  uint64_t maker_order_id;
  uint64_t taker_order_id;
  uint64_t price;
  uint32_t quantity;
  uint32_t pad; // keeps struct at 32 bytes, matching TradeMsgLayout.java
};
static_assert(sizeof(TradeMsg) == 32,
    "TradeMsg layout changed — update TradeMsgLayout.java offsets to match");

// Intrusive doubly-linked list for order time priority.
struct PriceLevel {
  Order* head{nullptr};
  Order* tail{nullptr};
  uint32_t total_volume{0};
  uint32_t order_count{0};

  bool empty() const { return head == nullptr; }
};

class OrderBook {
public:
  // We share a MemoryPool injected from the MatchingEngine
  explicit OrderBook(MemoryPool<Order, 1048576>& order_pool,
                     LockFreeQueue<TradeMsg, 1024>& outbound_queue);
  ~OrderBook() = default;

  // No copying
  OrderBook(const OrderBook&) = delete;
  OrderBook& operator=(const OrderBook&) = delete;

  // Process a new limit order
  // For simplicity, we just pass the raw fields and let the book 
  // acquire the an Order block from the pool if it rests on the book.
  void add_order(uint64_t order_id, Side side, uint64_t price, uint32_t quantity);

  // Cancel an existing resting order by ID
  void cancel_order(uint64_t order_id);

  // Optional: Match and return a list of trades (will be piped to lock-free queue)
  // We'll hook this up to the queue later.

private:
  LockFreeQueue<TradeMsg, 1024>& outbound_queue_;
  // Flat structure, zero allocations. Max bounds for the simulator.
  static constexpr size_t MAX_PRICE_TICKS = 100000; // e.g., prices 0 to 999.99
  static constexpr size_t MAX_ORDERS = 1048576;

  MemoryPool<Order, MAX_ORDERS>& order_pool_;
  uint64_t sequence_id_{0};

  // Direct indexing by price tick. O(1) lookup.
  std::array<PriceLevel, MAX_PRICE_TICKS> bids_{};
  std::array<PriceLevel, MAX_PRICE_TICKS> asks_{};

  // Track the highest bid and lowest ask to avoid scanning.
  uint64_t best_bid_{0};
  uint64_t best_ask_{MAX_PRICE_TICKS};

  // Flat lookup array. Dense order IDs map directly to indices.
  std::array<Order*, MAX_ORDERS> order_map_{nullptr};

  // Internal helpers
  void match(Order* incoming);
  void insert_resting(Order* order);
  void remove_from_level(PriceLevel& level, Order* order);
};

} // namespace hft
