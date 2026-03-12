# HFTExchange

A hybrid exchange simulator built to understand how low-latency trading systems actually work. The core matching engine is in C++20, the simulation and risk layer is in Java 21, and they talk to each other through Java's Foreign Function & Memory API (no JNI).

This is a learning project and a portfolio project at the same time. The code is written the way a real system would be written — pre-allocated memory, lock-free queues, cache-aware data layouts — but the goal is also to understand *why* each decision was made, not just copy patterns from papers.

---

## What it does

- Central Limit Order Book with price-time priority matching
- Supports Limit, Market, IOC, FOK, and Cancel order types
- Memory pool for zero-allocation order handling on the hot path
- Lock-free SPSC queue between the engine and the risk layer
- Risk engine in Java using the Vector API for parallel exposure calculations
- Market data replay from memory-mapped files
- FFM bridge maps C++ memory pools directly into Java `MemorySegment` objects — no copying, no GC pressure

---

## Project layout

```
HFTExchange/
├── cpp-engine/          C++20 matching engine
│   ├── include/         headers only
│   ├── src/             implementations
│   ├── tests/           Google Test suites
│   └── benchmarks/      Google Benchmark harnesses
├── java-simulation/     Java 21 simulation and risk
│   └── src/main/java/
│       ├── core/        FFM manager, ring buffer
│       ├── agents/      market participants
│       └── risk/        exposure engine
├── ffm-bridge/          C++ side of the Java-C++ bridge
├── data/                sample market data for replay
├── DESIGN_DECISIONS.md
└── README.md
```

---

## Building

### C++ engine

You need CMake 3.20+, a compiler with C++20 support (GCC 12+ or Clang 14+), and an internet connection the first time (CMake fetches GoogleTest and Google Benchmark via FetchContent).

```bash
cd cpp-engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run tests:
```bash
./build/tests/hft_tests
```

Run benchmarks:
```bash
./build/benchmarks/hft_benchmarks
```

### Java simulation

You need JDK 21 and Maven 3.9+.

```bash
cd java-simulation
mvn package
```

The FFM API and Vector API are used directly — no preview flags needed in JDK 21 for FFM, but the Vector API still requires `--add-modules jdk.incubator.vector` at runtime. The Maven Surefire config handles this.

---

## Dependencies

**C++**
- [GoogleTest](https://github.com/google/googletest) — unit tests (fetched by CMake)
- [Google Benchmark](https://github.com/google/benchmark) — latency benchmarks (fetched by CMake)

**Java**
- [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor) — ring buffer for inter-agent messaging
- Java 21 stdlib only for FFM and Vector API (no external deps for the bridge)

---

## Status

Work in progress. Being built iteratively — one module at a time, tested before moving on.

| Module | Status |
|---|---|
| MemoryPool | completed |
| OrderBook | in progress |
| LockFreeQueue | completed |
| FFM Bridge | not started |
| Risk Engine | not started |
| Simulation Agents | not started |

---

## Why this architecture

See `DESIGN_DECISIONS.md` for the reasoning behind the main trade-offs. Short version: C++ for the matching engine because latency matters there and you can control memory layout exactly; Java for the risk and simulation layer because the ecosystem is good and the Vector API is genuinely useful for bulk calculations; Panama bridge instead of JNI because JNI is painful and the zero-copy MemorySegment approach is cleaner.
