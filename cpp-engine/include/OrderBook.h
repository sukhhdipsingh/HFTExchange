#pragma once

#include "Order.h"
#include "MemoryPool.h"
#include <map>
#include <unordered_map>
#include <cstdint>

namespace hft {

// Represents a single price level in the OrderBook.
// It acts as a doubly-linked list head/tail for the incoming Orders.
// Time priority is maintained implicitly: new orders append to tail.
struct PriceLevel {
  Order* head{nullptr};
  Order* tail{nullptr};
  uint32_t total_volume{0};
  uint32_t order_count{0};

  bool empty() const { return head == nullptr; }
};

// The core matching structure.
// For now, Bids are ordered highest-to-lowest, Asks lowest-to-highest.
// This skeleton uses std::map for price levels. In a strict zero-allocation
// system, we would replace std::map with a fixed-depth array or flat_map
// using a secondary MemoryPool, but let's establish the matching logic first.
class OrderBook {
public:
  // We share a MemoryPool injected from the MatchingEngine
  explicit OrderBook(MemoryPool<Order, 1048576>& order_pool);
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
  MemoryPool<Order, 1048576>& order_pool_;
  uint64_t sequence_id_{0};

  // std::greater sorts Bids descending (buy high)
  std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids_;
  
  // std::less sorts Asks ascending (sell low)
  std::map<uint64_t, PriceLevel, std::less<uint64_t>> asks_;

  // O(1) lookup to find resting orders for fast cancellation
  std::unordered_map<uint64_t, Order*> order_map_;

  // Internal helpers
  void match(Order* incoming);
  void insert_resting(Order* order);
  void remove_from_level(PriceLevel& level, Order* order);
};

} // namespace hft
