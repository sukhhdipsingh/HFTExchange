cmake_minimum_required(VERSION 3.20)
project(HFTExchange VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# compiler flags
# -O3 and -march=native are intentional — this code is meant to run on the
# machine it was compiled on, and we want the compiler to use whatever SIMD
# extensions the CPU supports. -flto gives link-time inlining across TUs.
# -fno-exceptions and -fno-rtti are worth enabling later once the codebase is
# stable, but leave them off for now so error messages are readable.
# ----------------------------------------------------------------------------
# (Target compiler options moved to after add_library)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wno-unused-parameter   # too noisy during skeleton phase
)

# ----------------------------------------------------------------------------
# include directories
# ----------------------------------------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/include)

# ----------------------------------------------------------------------------
# Google Test and Google Benchmark via FetchContent
# We pin to specific git tags so the build is reproducible.
# ----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
)

# don't install gtest alongside our project when someone runs cmake --install
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest benchmark)

# ----------------------------------------------------------------------------
# engine library
# All the matching engine code compiles into a static library that both
# the test binary and the benchmark binary link against. This avoids
# compiling the same sources twice.
# ----------------------------------------------------------------------------
add_library(hft_engine STATIC)

target_sources(hft_engine PRIVATE
    src/engine.cpp          # placeholder — keeps the target valid; templates live in headers
    src/OrderBook.cpp
    # src/MatchingEngine.cpp
)

target_include_directories(hft_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# apply release flags to the library target
target_compile_options(hft_engine PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native -fno-omit-frame-pointer>
)

# ----------------------------------------------------------------------------
# test binary
# ----------------------------------------------------------------------------
enable_testing()

add_executable(hft_tests)

target_sources(hft_tests PRIVATE
    # test files get added here as test suites are written
    tests/MemoryPoolTest.cpp
    tests/LockFreeQueueTest.cpp
    tests/OrderBookTest.cpp
)

target_link_libraries(hft_tests PRIVATE
    hft_engine
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hft_tests)

# ----------------------------------------------------------------------------
# benchmark binary
# ----------------------------------------------------------------------------
add_executable(hft_benchmarks)

target_sources(hft_benchmarks PRIVATE
    # benchmarks/MemoryPoolBenchmark.cpp
    benchmarks/QueueBenchmark.cpp
)

target_link_libraries(hft_benchmarks PRIVATE
    hft_engine
    benchmark::benchmark_main
)

# benchmarks always compile with optimizations, regardless of build type
target_compile_options(hft_benchmarks PRIVATE
    -O3 -march=native -fno-omit-frame-pointer
)
