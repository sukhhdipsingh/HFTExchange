#include "LockFreeQueue.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

using namespace hft;

// Benchmarking SPSC Lock-Free Ring Buffer throughput
static void BM_LockFreeQueue(benchmark::State& state) {
  // Using a large capacity to avoid full buffer conditions dragging down throughput.
  // We want to measure atomic operation overhead, not bounded buffer wait overhead.
  LockFreeQueue<uint64_t, 65536> q;
  std::atomic<bool> start_flag{false};
  std::atomic<bool> done_flag{false};
  uint64_t iter_count = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    start_flag = false;
    done_flag = false;
    
    // Drain queue in case we didn't before the previous iteration ends
    uint64_t dummy;
    while(q.pop(dummy)) {}

    std::thread producer([&]() {
      while (!start_flag.load(std::memory_order_acquire)) {}
      for (uint64_t i = 0; i < iter_count; ++i) {
        while (!q.push(i)) {} 
      }
    });

    std::thread consumer([&]() {
      while (!start_flag.load(std::memory_order_acquire)) {}
      for (uint64_t i = 0; i < iter_count; ++i) {
        uint64_t val;
        while (!q.pop(val)) {}
      }
      done_flag.store(true, std::memory_order_release);
    });

    state.ResumeTiming();
    
    // Fire the starting pistol
    start_flag.store(true, std::memory_order_release);
    
    // Wait for completion
    while (!done_flag.load(std::memory_order_acquire)) {}
    
    producer.join();
    consumer.join();
  }

  // Calculate elements processed per second
  state.SetItemsProcessed(state.iterations() * iter_count);
}

// We will let each bench iteration do 50 million ops
BENCHMARK(BM_LockFreeQueue)->Arg(50000000)->UseRealTime();

BENCHMARK_MAIN();
