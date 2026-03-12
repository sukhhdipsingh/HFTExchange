#include "MemoryPool.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

using namespace hft;

// a plain struct that represents roughly what an order looks like.
// we don't have the real Order type yet so this stands in for now.
struct DummyOrder {
  uint64_t id;
  uint64_t price;
  uint64_t qty;
  uint32_t side; // 0 = buy, 1 = sell
};

// a non-trivial type to test construct() / destroy()
struct TrackedObject {
  int value;
  bool *destroyed_flag;

  TrackedObject(int v, bool *flag) : value(v), destroyed_flag(flag) {}
  ~TrackedObject() {
    if (destroyed_flag)
      *destroyed_flag = true;
  }
};

// -------------------------------------------------------------------------
// basic allocation
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, InitialStateIsCorrect) {
  MemoryPool<DummyOrder, 64> pool;

  EXPECT_EQ(pool.available(), 64u);
  EXPECT_EQ(pool.in_use(), 0u);
  EXPECT_TRUE(pool.is_valid());
  EXPECT_EQ(pool.free_list_length(), 64u);
}

TEST(MemoryPoolTest, AcquireDecreasesAvailable) {
  MemoryPool<DummyOrder, 64> pool;

  DummyOrder *p = pool.acquire();
  ASSERT_NE(p, nullptr);

  EXPECT_EQ(pool.available(), 63u);
  EXPECT_EQ(pool.in_use(), 1u);
}

TEST(MemoryPoolTest, ReleaseRestoresAvailable) {
  MemoryPool<DummyOrder, 64> pool;

  DummyOrder *p = pool.acquire();
  ASSERT_NE(p, nullptr);

  pool.release(p);

  EXPECT_EQ(pool.available(), 64u);
  EXPECT_EQ(pool.in_use(), 0u);
  EXPECT_EQ(pool.free_list_length(), 64u);
}

TEST(MemoryPoolTest, AcquiredPointerIsWritable) {
  MemoryPool<DummyOrder, 8> pool;

  DummyOrder *order = pool.acquire();
  ASSERT_NE(order, nullptr);

  order->id = 42;
  order->price = 100'00; // $100.00 in cents
  order->qty = 500;
  order->side = 0;

  EXPECT_EQ(order->id, 42u);
  EXPECT_EQ(order->price, 10000u);
  EXPECT_EQ(order->qty, 500u);
  EXPECT_EQ(order->side, 0u);
}

TEST(MemoryPoolTest, MultipleAcquireReleaseRoundtrips) {
  MemoryPool<DummyOrder, 8> pool;

  for (int round = 0; round < 100; ++round) {
    DummyOrder *p = pool.acquire();
    ASSERT_NE(p, nullptr) << "failed on round " << round;
    pool.release(p);
    EXPECT_EQ(pool.in_use(), 0u);
  }
}

// -------------------------------------------------------------------------
// exhaustion
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, ExhaustionReturnsNullptr) {
  MemoryPool<DummyOrder, 4> pool;

  DummyOrder *slots[4];
  for (int i = 0; i < 4; ++i) {
    slots[i] = pool.acquire();
    ASSERT_NE(slots[i], nullptr);
  }

  // pool is now full
  EXPECT_EQ(pool.available(), 0u);
  EXPECT_EQ(pool.in_use(), 4u);

  DummyOrder *overflow = pool.acquire();
  EXPECT_EQ(overflow, nullptr);

  // release everything to leave the pool clean
  for (int i = 0; i < 4; ++i) {
    pool.release(slots[i]);
  }
}

TEST(MemoryPoolTest, CanAcquireAfterReleaseFromFullPool) {
  MemoryPool<DummyOrder, 2> pool;

  DummyOrder *a = pool.acquire();
  DummyOrder *b = pool.acquire();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(pool.acquire(), nullptr); // exhausted

  pool.release(a);

  DummyOrder *c = pool.acquire();
  EXPECT_NE(c, nullptr);

  pool.release(c);
  pool.release(b);
}

// -------------------------------------------------------------------------
// construct / destroy
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, ConstructCallsConstructor) {
  MemoryPool<TrackedObject, 8> pool;
  bool destroyed = false;

  TrackedObject *obj = pool.construct(99, &destroyed);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value, 99);
  EXPECT_FALSE(destroyed);

  pool.destroy(obj);
}

TEST(MemoryPoolTest, DestroyCallsDestructor) {
  MemoryPool<TrackedObject, 8> pool;
  bool destroyed = false;

  TrackedObject *obj = pool.construct(42, &destroyed);
  ASSERT_NE(obj, nullptr);
  EXPECT_FALSE(destroyed);

  pool.destroy(obj);
  EXPECT_TRUE(destroyed);

  // slot should be back in the pool
  EXPECT_EQ(pool.in_use(), 0u);
}

// -------------------------------------------------------------------------
// contains
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, ContainsReturnsTrueForOwnedPointer) {
  MemoryPool<DummyOrder, 8> pool;

  DummyOrder *p = pool.acquire();
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(pool.contains(p));

  pool.release(p);
}

TEST(MemoryPoolTest, ContainsReturnsFalseForExternalPointer) {
  MemoryPool<DummyOrder, 8> pool;

  DummyOrder external;
  EXPECT_FALSE(pool.contains(&external));
}

// -------------------------------------------------------------------------
// reset
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, ResetMakesAllSlotsAvailableAgain) {
  MemoryPool<DummyOrder, 8> pool;

  // use the pool then reset
  for (int i = 0; i < 8; ++i) {
    (void)pool.acquire();
  }
  EXPECT_EQ(pool.in_use(), 8u);

  pool.reset();

  EXPECT_EQ(pool.available(), 8u);
  EXPECT_EQ(pool.in_use(), 0u);
  EXPECT_EQ(pool.free_list_length(), 8u);
}

TEST(MemoryPoolTest, PoolIsUsableAfterReset) {
  MemoryPool<DummyOrder, 4> pool;

  (void)pool.acquire();
  pool.reset();

  // the acquired pointer is now invalid — we don't touch it. just check the pool works.
  DummyOrder *p2 = pool.acquire();
  EXPECT_NE(p2, nullptr);
  EXPECT_EQ(pool.in_use(), 1u);

  pool.release(p2);
}

// -------------------------------------------------------------------------
// free list integrity
// -------------------------------------------------------------------------

TEST(MemoryPoolTest, FreeListLengthMatchesAvailableAfterMixedOps) {
  MemoryPool<DummyOrder, 16> pool;

  DummyOrder *ptrs[8];
  for (int i = 0; i < 8; ++i) {
    ptrs[i] = pool.acquire();
  }

  // release half of them in reverse order (tests that the list links correctly)
  for (int i = 7; i >= 4; --i) {
    pool.release(ptrs[i]);
  }

  EXPECT_EQ(pool.free_list_length(), pool.available());
  EXPECT_EQ(pool.in_use(), 4u);

  // release the rest
  for (int i = 0; i < 4; ++i) {
    pool.release(ptrs[i]);
  }

  EXPECT_EQ(pool.free_list_length(), 16u);
}

TEST(MemoryPoolTest, AllAcquiredPointersAreDistinct) {
  // constexpr N as a local var doesn't work as a template arg in GCC at
  // function scope
  static constexpr std::size_t N = 32;
  MemoryPool<DummyOrder, N> pool;

  DummyOrder *ptrs[N];
  for (std::size_t i = 0; i < N; ++i) {
    ptrs[i] = pool.acquire();
    ASSERT_NE(ptrs[i], nullptr);
  }

  // check no two pointers are the same
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
      EXPECT_NE(ptrs[i], ptrs[j])
          << "duplicate pointer at i=" << i << " j=" << j;
    }
  }

  for (std::size_t i = 0; i < N; ++i) {
    pool.release(ptrs[i]);
  }
}
