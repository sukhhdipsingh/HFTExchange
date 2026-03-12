#include "LockFreeQueue.h"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>

using namespace hft;

TEST(LockFreeQueueTest, BasicOperations) {
  LockFreeQueue<int, 4> q;

  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0);

  EXPECT_TRUE(q.push(10));
  EXPECT_TRUE(q.push(20));

  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 2);

  int v1 = 0;
  EXPECT_TRUE(q.pop(v1));
  EXPECT_EQ(v1, 10);

  int v2 = 0;
  EXPECT_TRUE(q.pop(v2));
  EXPECT_EQ(v2, 20);

  EXPECT_TRUE(q.empty());
}

TEST(LockFreeQueueTest, PushUntilFull) {
  LockFreeQueue<int, 4> q;

  EXPECT_TRUE(q.push(1));
  EXPECT_TRUE(q.push(2));
  EXPECT_TRUE(q.push(3));
  EXPECT_TRUE(q.push(4));

  EXPECT_FALSE(q.push(5)); // Should fail, queue capacity is 4
  EXPECT_EQ(q.size(), 4);

  int val;
  EXPECT_TRUE(q.pop(val));
  EXPECT_EQ(val, 1);
  EXPECT_TRUE(q.push(6)); // Now pushing should succeed
}

TEST(LockFreeQueueTest, PopFromEmpty) {
  LockFreeQueue<int, 2> q;
  int val;
  EXPECT_FALSE(q.pop(val)); // Should fail, queue is empty
}

TEST(LockFreeQueueTest, ConcurrentProducerConsumer) {
  LockFreeQueue<int, 1024> q;
  std::atomic<int> sum_produced{0};
  std::atomic<int> sum_consumed{0};
  const int NUM_ITEMS = 100000;

  std::thread producer([&]() {
    for (int i = 1; i <= NUM_ITEMS; ++i) {
      while (!q.push(i)) {
        // busy wait
      }
      sum_produced += i;
    }
  });

  std::thread consumer([&]() {
    for (int i = 1; i <= NUM_ITEMS; ++i) {
      int val;
      while (!q.pop(val)) {
        // busy wait
      }
      sum_consumed += val;
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(sum_produced.load(), sum_consumed.load());
}
