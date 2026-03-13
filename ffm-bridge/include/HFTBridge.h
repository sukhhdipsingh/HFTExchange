#pragma once

#include <cstdint>

#if defined(_WIN32)
  #define ENGINE_API __declspec(dllexport)
#else
  #define ENGINE_API __attribute__((visibility("default")))
#endif

// pure C linkage for Java FFM API (Project Panama) binding
extern "C" {

  // opaque handle for the engine instance
  typedef void* EngineHandle;

  // heap allocate engine, return handle
  ENGINE_API EngineHandle engine_create();

  // free the engine
  ENGINE_API void engine_destroy(EngineHandle engine);

  // return raw ptr to inbound spsc queue for Java MemorySegment
  ENGINE_API void* engine_get_inbound_queue(EngineHandle engine);

  // return raw ptr to outbound spsc queue
  ENGINE_API void* engine_get_outbound_queue(EngineHandle engine);

  // force a single poll tick (mostly for testing)
  ENGINE_API void engine_poll(EngineHandle engine);

} // extern "C"
