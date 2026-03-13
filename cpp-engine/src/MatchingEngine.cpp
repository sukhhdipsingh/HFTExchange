#include "MatchingEngine.h"

namespace hft {

MatchingEngine::MatchingEngine()
    : memory_pool_(), 
      book_(memory_pool_) {
  // TODO: bind outbound_queue_ to OrderBook so match() can emit TradeMsg directly.
  // currently we just decrease resting quantities and drop trades on the floor.
}

void MatchingEngine::poll() {
  IncomingMsg msg;
  
  // non-blocking. return immediately if empty.
  // (assumes we are called in a tight while(true) loop on an isolcpus core)
  if (inbound_queue_.pop(msg)) {
    process_message(msg);
  }
}

void MatchingEngine::process_message(const IncomingMsg& msg) {
  // fast switch over msg type. no virtual dispatch.
  switch (msg.type) {
    case 0: // New Order
      // TODO: price/qty bounds validation before passing to book
      book_.add_order(msg.order_id, msg.side, msg.price, msg.quantity);
      break;
      
    case 1: // Cancel Order
      book_.cancel_order(msg.order_id);
      break;

    default:
      // drop unknown msg. no exceptions on hot path.
      // TODO: increment corrupted_msg_counter metric
      break;
  }
}

} // namespace hft
