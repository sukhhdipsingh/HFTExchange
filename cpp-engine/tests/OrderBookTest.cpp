#include "OrderBook.h"
#include <gtest/gtest.h>
#include <memory>

using namespace hft;

// Internal access for testing only

TEST(OrderBookTest, AddBuyOrderRestsInBook) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  auto book = std::make_unique<OrderBook>(*pool);

  book->add_order(1, Side::Buy, 10050, 10);
  
  // Check rest status via pool allocation.
  EXPECT_EQ(pool->in_use(), 1);
}

TEST(OrderBookTest, MatchExactPriceAndQty) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  auto book = std::make_unique<OrderBook>(*pool);

  book->add_order(1, Side::Buy, 10050, 10);
  EXPECT_EQ(pool->in_use(), 1); // Buy resting

  book->add_order(2, Side::Sell, 10050, 10);
  // Exact match. Buy is destroyed.
  EXPECT_EQ(pool->in_use(), 0); 
}

TEST(OrderBookTest, CancelRemovesOrder) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  auto book = std::make_unique<OrderBook>(*pool);

  book->add_order(1, Side::Sell, 10050, 10);
  EXPECT_EQ(pool->in_use(), 1);

  book->cancel_order(1);
  EXPECT_EQ(pool->in_use(), 0); // Released from pool
}

TEST(OrderBookTest, PartialMatch) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  auto book = std::make_unique<OrderBook>(*pool);

  book->add_order(1, Side::Buy, 10050, 10);
  
  // Partial fill logic. Sell takes 4.
  book->add_order(2, Side::Sell, 10050, 4);

  // Original buy rests with 6 left. Sell matched fully, no pool allocation.
  EXPECT_EQ(pool->in_use(), 1);
}

TEST(OrderBookTest, WalkTheBook) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  auto book = std::make_unique<OrderBook>(*pool);

  // Populate multiple levels
  book->add_order(1, Side::Buy, 10030, 10);
  book->add_order(2, Side::Buy, 10050, 10); // best bid
  book->add_order(3, Side::Buy, 10040, 10);

  EXPECT_EQ(pool->in_use(), 3);

  book->add_order(4, Side::Sell, 10000, 25);
  // Large sell of 25 clears top 2 levels.
  // Level 1: 10 units
  // Level 2: 10 units
  // Level 3: 5 units matched, 5 resting.
  EXPECT_EQ(pool->in_use(), 1); 
}
