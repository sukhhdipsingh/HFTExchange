#pragma once

#include "OrderBook.h"
#include "LockFreeQueue.h"
#include <cstdint>

namespace hft {

// Outbound message (Trade matched). Sent back via the LockFreeQueue.
struct TradeMsg {
  uint64_t maker_order_id;
  uint64_t taker_order_id;
  uint64_t price;
  uint32_t quantity;
};

// Incoming message from the Java layer (FFM bridge) or network.
struct IncomingMsg {
  uint8_t type; // 0 = New Order, 1 = Cancel
  uint64_t order_id;
  Side side;
  uint64_t price;
  uint32_t quantity;
};

// The central orchestrator that binds the OrderBook and LockFreeQueues.
// Owns the MemoryPool to ensure centralized lifetime management.
class MatchingEngine {
public:
  MatchingEngine();
  ~MatchingEngine() = default;

  // No copy/move allowed
  MatchingEngine(const MatchingEngine&) = delete;
  MatchingEngine& operator=(const MatchingEngine&) = delete;

  // Main work loop step. Polls the inbound queue, advances the state,
  // and flushes to the outbound queue.
  // This will eventually be called in a tight while(true) spin-loop on a dedicated pinned core.
  void poll();

  // Accessors for the FFM Bridge / Testing harness to push and pull messages
  LockFreeQueue<IncomingMsg, 1024>* get_inbound_queue() { return &inbound_queue_; }
  LockFreeQueue<TradeMsg, 1024>* get_outbound_queue() { return &outbound_queue_; }

private:
  MemoryPool<Order, 1048576> memory_pool_;
  OrderBook book_;

  // SPSC Queues representing the boundary between threads/languages
  LockFreeQueue<IncomingMsg, 1024> inbound_queue_;
  LockFreeQueue<TradeMsg, 1024> outbound_queue_;

  void process_message(const IncomingMsg& msg);
};

} // namespace hft
