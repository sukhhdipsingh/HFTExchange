package core;

import java.lang.foreign.MemorySegment;

/**
 * Project Panama (FFM) boundary interface.
 * Loads the C++ shared library and exposes MethodHandles for HFT Engine.
 */
public interface HFTBridge {

    // Allocates C++ matching engine and returns opaque pointer.
    MemorySegment engineCreate();

    // Destroys C++ engine instance.
    void engineDestroy(MemorySegment engine);

    // Returns raw pointer to inbound SPSC queue.
    MemorySegment getInboundQueue(MemorySegment engine);

    // Returns raw pointer to outbound SPSC queue.
    MemorySegment getOutboundQueue(MemorySegment engine);

    // Polls inbound queue once (for deterministic testing).
    void enginePoll(MemorySegment engine);

}
