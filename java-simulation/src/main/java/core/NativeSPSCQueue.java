package core;

import java.lang.foreign.MemorySegment;

// zero-allocation wrapper around the C++ LockFreeQueue mapped via FFM.
// capacity must match the C++ side (1024).
// uses getAcquire/setRelease to mirror acquire/release memory semantics on head/tail.
public interface NativeSPSCQueue {

    // bind queue wrapper to the raw C++ struct memory
    void bind(MemorySegment segment);

    // push an IncomingMsg directly into the ring buffer.
    // returns false if full.
    boolean push(long orderId, long price, int quantity, byte type, byte side);

    // TODO: pop() for outbound trade queue
}
