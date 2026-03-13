#include "../include/HFTBridge.h"
#include "../../cpp-engine/include/MatchingEngine.h"

// cast macro to keep it clean
#define UNWRAP(ptr) static_cast<hft::MatchingEngine*>(ptr)

extern "C" {

  ENGINE_API EngineHandle engine_create() {
    // heap alloc avoids stack overflow. Java FFM Arena will track it.
    auto engine = new hft::MatchingEngine();
    return static_cast<EngineHandle>(engine);
  }

  ENGINE_API void engine_destroy(EngineHandle engine) {
    if (engine) {
      delete UNWRAP(engine);
    }
  }

  ENGINE_API void* engine_get_inbound_queue(EngineHandle engine) {
    if (!engine) return nullptr;
    // return raw byte pointer for Java struct mapping
    return static_cast<void*>(UNWRAP(engine)->get_inbound_queue());
  }

  ENGINE_API void* engine_get_outbound_queue(EngineHandle engine) {
    if (!engine) return nullptr;
    return static_cast<void*>(UNWRAP(engine)->get_outbound_queue());
  }

  ENGINE_API void engine_poll(EngineHandle engine) {
    if (!engine) return;
    UNWRAP(engine)->poll();
  }

} // extern "C"
