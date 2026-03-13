#include "OrderBook.h"
#include <algorithm>

namespace hft {

OrderBook::OrderBook(MemoryPool<Order, 1048576>& order_pool)
    : order_pool_(order_pool) {}

void OrderBook::add_order(uint64_t order_id, Side side, uint64_t price, uint32_t quantity) {
  // Hold incoming state; no allocation yet.
  Order incoming{order_id, price, quantity, ++sequence_id_, side, nullptr, nullptr};
  
  match(&incoming);

    // Rest quantity if remaining.
  if (incoming.quantity > 0) {
    Order* resting = order_pool_.acquire();
    if (!resting) {
      // TODO: Critical error/reject on pool exhaustion.
      return; 
    }
    
    // Copy state into pool slot.
    *resting = incoming;
    resting->next = nullptr;
    resting->prev = nullptr;
    
    insert_resting(resting);
    if (resting->order_id < MAX_ORDERS) {
      order_map_[resting->order_id] = resting;
    }
  }
}

void OrderBook::cancel_order(uint64_t order_id) {
  if (order_id >= MAX_ORDERS) return;
  Order* order = order_map_[order_id];
  if (!order) return;
  
  order_map_[order_id] = nullptr;

  // Remove from intrusive list.
  if (order->side == Side::Buy) {
    remove_from_level(bids_[order->price], order);
    // Note: We could update best_bid_ here if the level became empty,
    // but lazy evaluation during match() is usually faster in HFT.
  } else {
    remove_from_level(asks_[order->price], order);
    // Note: Lazy evaluation for best_ask_ as well.
  }

  // Free back to static pool.
  order_pool_.release(order);
}

void OrderBook::match(Order* incoming) {
  if (incoming->side == Side::Buy) {
    // Match against Asks.
    while (incoming->quantity > 0 && best_ask_ <= incoming->price && best_ask_ < MAX_PRICE_TICKS) {
      PriceLevel& level = asks_[best_ask_];
      Order* resting = level.head;
      
      while (resting && incoming->quantity > 0) {
        uint32_t trade_qty = std::min(incoming->quantity, resting->quantity);
        incoming->quantity -= trade_qty;
        resting->quantity -= trade_qty;
        level.total_volume -= trade_qty;

        Order* next = resting->next;
        if (resting->quantity == 0) {
          remove_from_level(level, resting);
          if (resting->order_id < MAX_ORDERS) order_map_[resting->order_id] = nullptr;
          order_pool_.release(resting);
        }
        resting = next;
      }

      // Advance best_ask_ if current level is depleted.
      if (level.empty()) {
        best_ask_++;
        while (best_ask_ < MAX_PRICE_TICKS && asks_[best_ask_].empty()) {
          best_ask_++;
        }
      }
    }
  } else {
    // Match against Bids.
    while (incoming->quantity > 0 && best_bid_ >= incoming->price && best_bid_ > 0) {
      PriceLevel& level = bids_[best_bid_];
      Order* resting = level.head;
      
      while (resting && incoming->quantity > 0) {
        uint32_t trade_qty = std::min(incoming->quantity, resting->quantity);
        incoming->quantity -= trade_qty;
        resting->quantity -= trade_qty;
        level.total_volume -= trade_qty;

        Order* next = resting->next;
        if (resting->quantity == 0) {
          remove_from_level(level, resting);
          if (resting->order_id < MAX_ORDERS) order_map_[resting->order_id] = nullptr;
          order_pool_.release(resting);
        }
        resting = next;
      }

      // Advance best_bid_ if current level is depleted.
      if (level.empty()) {
        if (best_bid_ == 0) break;
        best_bid_--;
        while (best_bid_ > 0 && bids_[best_bid_].empty()) {
          best_bid_--;
        }
      }
    }
  }
}

void OrderBook::insert_resting(Order* order) {
  if (order->price >= MAX_PRICE_TICKS) return;

  PriceLevel* level;
  if (order->side == Side::Buy) {
    level = &bids_[order->price];
    if (order->price > best_bid_) best_bid_ = order->price;
  } else {
    level = &asks_[order->price];
    if (order->price < best_ask_) best_ask_ = order->price;
  }

  // Insert at back (FIFO).
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
