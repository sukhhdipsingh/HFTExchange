#pragma once

#include <cstdint>

namespace hft {

enum class Side : uint8_t { 
  Buy = 0, 
  Sell = 1 
};

// Represents a resting limit order in the matching engine.
// Designed to be allocated exclusively from the MemoryPool.
struct Order {
  uint64_t order_id;
  uint64_t price;     // Fixed-point tick representation (e.g. 100.50 -> 10050)
  uint32_t quantity;
  
  // Used for Price-Time (FIFO) matching priority.
  // In a real system, this might be hardware rx timestamp, 
  // but an internal monotonic sequence number works too.
  uint64_t timestamp; 

  Side side;

  // Intrusive doubly-linked list pointers.
  // We use an intrusive list so we don't have to allocate 
  // separate "Node" objects for the OrderBook price levels.
  // The Order *is* the list node.
  Order *next{nullptr};
  Order *prev{nullptr};
};

} // namespace hft
