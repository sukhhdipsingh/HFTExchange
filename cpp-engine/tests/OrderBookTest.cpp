#include "OrderBook.h"
#include <gtest/gtest.h>
#include <memory>

using namespace hft;

// Expose some internals strictly for testing if needed
// or we can test by asserting what's in the MemoryPool

TEST(OrderBookTest, AddBuyOrderRestsInBook) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  OrderBook book(*pool);

  book.add_order(1, Side::Buy, 10050, 10);
  
  // Best way to check rest status without exposing getter is that the pool
  // will have 1 in use slot since the order rested.
  EXPECT_EQ(pool->in_use(), 1);
}

TEST(OrderBookTest, MatchExactPriceAndQty) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  OrderBook book(*pool);

  book.add_order(1, Side::Buy, 10050, 10);
  EXPECT_EQ(pool->in_use(), 1); // Buy order resting

  book.add_order(2, Side::Sell, 10050, 10);
  // Matches exactly. Resting buy order is destroyed.
  EXPECT_EQ(pool->in_use(), 0); 
}

TEST(OrderBookTest, CancelRemovesOrder) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  OrderBook book(*pool);

  book.add_order(1, Side::Sell, 10050, 10);
  EXPECT_EQ(pool->in_use(), 1);

  book.cancel_order(1);
  EXPECT_EQ(pool->in_use(), 0); // Re-added to free list
}

TEST(OrderBookTest, PartialMatch) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  OrderBook book(*pool);

  book.add_order(1, Side::Buy, 10050, 10);
  
  // Sell comes in taking only 4 units
  book.add_order(2, Side::Sell, 10050, 4);

  // The original buy should still be resting, now with 6 units left.
  // The incoming sell was fully matched and never allocated on pool.
  EXPECT_EQ(pool->in_use(), 1);
}

TEST(OrderBookTest, WalkTheBook) {
  auto pool = std::make_unique<MemoryPool<Order, 1048576>>();
  OrderBook book(*pool);

  // Add 3 different buy orders
  book.add_order(1, Side::Buy, 10030, 10);
  book.add_order(2, Side::Buy, 10050, 10); // best bid
  book.add_order(3, Side::Buy, 10040, 10);

  EXPECT_EQ(pool->in_use(), 3);

  // A large sell of 25 taking out the top 2.5 levels
  book.add_order(4, Side::Sell, 10000, 25);

  // Order 2 (10) and Order 3 (10) are dead.
  // Order 1 (10) provided 5 units to the sell, leaving 5 units resting.
  // Sell order is fully dead.
  EXPECT_EQ(pool->in_use(), 1); 
}
