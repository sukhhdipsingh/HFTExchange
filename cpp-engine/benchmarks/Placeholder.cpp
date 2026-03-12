#include <benchmark/benchmark.h>

// placeholder — real benchmarks get added here once MemoryPool and OrderBook
// are wired into the build. this file just keeps the benchmark binary target
// from failing the CMake configure step.

static void BM_Placeholder(benchmark::State &state) {
  for (auto _ : state) {
    // nothing yet
    benchmark::DoNotOptimize(0);
  }
}
BENCHMARK(BM_Placeholder);
