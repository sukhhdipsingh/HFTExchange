#include "MatchingEngine.h"
#include <gtest/gtest.h>

using namespace hft;

TEST(MatchingEngineTest, PollConsumesMessage) {
  auto engine = std::make_unique<MatchingEngine>();
  auto* inbound = engine->get_inbound_queue();

  IncomingMsg msg{};
  msg.order_id = 1;
  msg.price = 10050;
  msg.quantity = 10;
  msg.type = 0; // New order
  msg.side = Side::Buy;

  EXPECT_TRUE(inbound->push(msg));
  EXPECT_FALSE(inbound->empty());

  engine->poll();

  // The engine should have popped the message off the ring buffer and fed it to the order book.
  EXPECT_TRUE(inbound->empty());
}

TEST(MatchingEngineTest, ProcessCrossingOrdersDoesNotCrash) {
  auto engine = std::make_unique<MatchingEngine>();
  auto* inbound = engine->get_inbound_queue();

  // 1. Enter Buy order
  IncomingMsg buy_msg{};
  buy_msg.order_id = 1;
  buy_msg.price = 10000;
  buy_msg.quantity = 10;
  buy_msg.type = 0;
  buy_msg.side = Side::Buy;
  inbound->push(buy_msg);
  engine->poll();

  // 2. Enter crossing Sell order
  IncomingMsg sell_msg{};
  sell_msg.order_id = 2;
  sell_msg.price = 10000;
  sell_msg.quantity = 10;
  sell_msg.type = 0;
  sell_msg.side = Side::Sell;
  inbound->push(sell_msg);
  engine->poll();

  // 3. Cancel an order (even if already matched/not present, should be a safe no-op)
  IncomingMsg cancel_msg{};
  cancel_msg.order_id = 1;
  cancel_msg.type = 1; // Cancel
  inbound->push(cancel_msg);
  engine->poll();

  // Queue should be clean and we shouldn't have segfaulted.
  // Note: Once the outbound queue is wired up, we will test that TradeMsgs are emitted here.
  EXPECT_TRUE(inbound->empty());
}
