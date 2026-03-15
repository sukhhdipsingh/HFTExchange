#pragma once

#include "LockFreeQueue.h"
#include "OrderBook.h"
#include <cstdint>

namespace hft {

// TradeMsg is defined in OrderBook.h (emitted at fill-time).

// Incoming message from the Java layer (FFM bridge) or network
struct IncomingMsg {
  uint64_t order_id;
  uint64_t price;
  uint32_t quantity;
  uint8_t type;
  Side side;
  uint8_t pad[2];
};
static_assert(sizeof(IncomingMsg) == 24,
    "IncomingMsg layout changed — update IncomingMsgLayout.java offsets to match");

class MatchingEngine {
public:
  MatchingEngine();
  ~MatchingEngine() = default;

  // No copy/move allowed
  MatchingEngine(const MatchingEngine &) = delete;
  MatchingEngine &operator=(const MatchingEngine &) = delete;

  // Main work loop step. Polls the inbound queue, advances the state,
  // and flushes to the outbound queue.

  void poll();

  // Accessors for the FFM Bridge / Testing harness to push and pull messages
  LockFreeQueue<IncomingMsg, 1024> *get_inbound_queue() {
    return &inbound_queue_;
  }
  LockFreeQueue<TradeMsg, 1024> *get_outbound_queue() {
    return &outbound_queue_;
  }

private:
  MemoryPool<Order, 1048576> memory_pool_;
  OrderBook book_;

  // SPSC Queues representing the boundary between threads/languages
  LockFreeQueue<IncomingMsg, 1024> inbound_queue_;
  LockFreeQueue<TradeMsg, 1024> outbound_queue_;

  void process_message(const IncomingMsg &msg);
};

} // namespace hft
