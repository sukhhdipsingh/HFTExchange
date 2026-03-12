#include "OrderBook.h"
#include <algorithm>

namespace hft {

OrderBook::OrderBook(MemoryPool<Order, 1048576>& order_pool)
    : order_pool_(order_pool) {}

void OrderBook::add_order(uint64_t order_id, Side side, uint64_t price, uint32_t quantity) {
  // Temporary incoming order to hold state during matching.
  // Not allocated from pool yet.
  Order incoming{order_id, price, quantity, ++sequence_id_, side, nullptr, nullptr};
  
  match(&incoming);

  // If quantity still remains, the order must rest in the book.
  if (incoming.quantity > 0) {
    Order* resting = order_pool_.acquire();
    if (!resting) {
      // In a real system, we'd log a critical error or send a reject.
      // Pool capacity must exceed max open orders.
      return; 
    }
    
    // Copy the remaining state into the pooled order slot.
    *resting = incoming;
    resting->next = nullptr;
    resting->prev = nullptr;
    
    insert_resting(resting);
    order_map_[resting->order_id] = resting;
  }
}

void OrderBook::cancel_order(uint64_t order_id) {
  auto it = order_map_.find(order_id);
  if (it == order_map_.end()) {
    return; // Order not found, ignore
  }
  
  Order* order = it->second;
  order_map_.erase(it);

  // Find the exact price level and remove it from the intrusive list
  if (order->side == Side::Buy) {
    auto level_it = bids_.find(order->price);
    if (level_it != bids_.end()) {
      remove_from_level(level_it->second, order);
      if (level_it->second.empty()) bids_.erase(level_it);
    }
  } else {
    auto level_it = asks_.find(order->price);
    if (level_it != asks_.end()) {
      remove_from_level(level_it->second, order);
      if (level_it->second.empty()) asks_.erase(level_it);
    }
  }

  // Free memory slot back to the static pool
  order_pool_.release(order);
}

void OrderBook::match(Order* incoming) {
  if (incoming->side == Side::Buy) {
    // Buy incoming matches against lowest Asks
    auto it = asks_.begin();
    while (it != asks_.end() && incoming->quantity > 0 && incoming->price >= it->first) {
      PriceLevel& level = it->second;
      Order* resting = level.head;
      
      while (resting && incoming->quantity > 0) {
        uint32_t trade_qty = std::min(incoming->quantity, resting->quantity);
        incoming->quantity -= trade_qty;
        resting->quantity -= trade_qty;
        level.total_volume -= trade_qty;

        Order* next = resting->next;
        if (resting->quantity == 0) {
          remove_from_level(level, resting);
          order_map_.erase(resting->order_id);
          order_pool_.release(resting);
        }
        resting = next;
      }

      if (level.empty()) {
        it = asks_.erase(it);
      } else {
        ++it;
      }
    }
  } else {
    // Sell incoming matches against highest Bids
    auto it = bids_.begin();
    while (it != bids_.end() && incoming->quantity > 0 && incoming->price <= it->first) {
      PriceLevel& level = it->second;
      Order* resting = level.head;
      
      while (resting && incoming->quantity > 0) {
        uint32_t trade_qty = std::min(incoming->quantity, resting->quantity);
        incoming->quantity -= trade_qty;
        resting->quantity -= trade_qty;
        level.total_volume -= trade_qty;

        Order* next = resting->next;
        if (resting->quantity == 0) {
          remove_from_level(level, resting);
          order_map_.erase(resting->order_id);
          order_pool_.release(resting);
        }
        resting = next;
      }

      if (level.empty()) {
        it = bids_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void OrderBook::insert_resting(Order* order) {
  PriceLevel* level;
  if (order->side == Side::Buy) {
    level = &bids_[order->price];
  } else {
    level = &asks_[order->price];
  }

  // Intrusive linked list insertion at the back (FIFO guarantees time priority)
  if (level->empty()) {
    level->head = order;
    level->tail = order;
  } else {
    level->tail->next = order;
    order->prev = level->tail;
    level->tail = order;
  }

  level->total_volume += order->quantity;
  level->order_count++;
}

void OrderBook::remove_from_level(PriceLevel& level, Order* order) {
  if (order->prev) {
    order->prev->next = order->next;
  } else {
    level.head = order->next;
  }

  if (order->next) {
    order->next->prev = order->prev;
  } else {
    level.tail = order->prev;
  }

  level.total_volume -= order->quantity;
  level.order_count--;
}

} // namespace hft
