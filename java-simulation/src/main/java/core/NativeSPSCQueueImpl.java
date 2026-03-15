package core;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;

// zero-allocation wrapper around C++ LockFreeQueue<IncomingMsg, 1024> memory.
// the struct memory looks like this in C++:
//   std::array<IncomingMsg, 1024> buffer_;    // at offset 0, before the atomics
//   alignas(64) std::atomic<size_t> head_;    // first 64-byte aligned slot after buffer
//   alignas(64) std::atomic<size_t> tail_;    // 64 bytes after head
//
// we mirror the same acquire/release semantics as the C++ side to stay correct.
public class NativeSPSCQueueImpl implements NativeSPSCQueue {

    private static final int CAPACITY = 1024;
    private static final int MASK     = CAPACITY - 1;

    // each IncomingMsg is 24 bytes
    private static final long ITEM_SIZE = IncomingMsgLayout.STRUCT_LAYOUT.byteSize();

    // buffer starts at byte 0 of the queue struct
    private static final long BUFFER_OFFSET = 0L;
    private static final long BUFFER_SIZE   = ITEM_SIZE * CAPACITY;

    // head_ atomic lives right after the buffer, aligned to 64 bytes
    private static final long HEAD_OFFSET = roundUpTo64(BUFFER_SIZE);

    // tail_ is 64 bytes after head_ (each atomic is on its own cache line)
    private static final long TAIL_OFFSET = HEAD_OFFSET + 64L;

    // VarHandle for reading/writing size_t (8 bytes on 64-bit) with acquire/release
    private static final VarHandle LONG_HANDLE =
        ValueLayout.JAVA_LONG.varHandle();

    private MemorySegment queueSegment;

    @Override
    public void bind(MemorySegment segment) {
        // reinterpret to allow full byte access — the segment came from C++ so it's already valid
        // TODO: figure out exact size at bind time so we can pass it in and avoid unbounded access
        this.queueSegment = segment.reinterpret(Long.MAX_VALUE);
    }

    @Override
    public boolean push(long orderId, long price, int quantity, byte type, byte side) {
        // relaxed load of head — producer is the only thread writing it
        long head = (long) LONG_HANDLE.getAcquire(queueSegment, HEAD_OFFSET);

        // acquire load of tail — we need to see the consumer's latest write
        long tail = (long) LONG_HANDLE.getAcquire(queueSegment, TAIL_OFFSET);

        if ((head - tail) == CAPACITY) {
            return false; // full
        }

        // offset of the slot we're writing into
        long slotOffset = BUFFER_OFFSET + (head & MASK) * ITEM_SIZE;

        // write struct fields directly into C++ memory
        IncomingMsgLayout.ORDER_ID_HANDLE .set(queueSegment.asSlice(slotOffset), 0L, orderId);
        IncomingMsgLayout.PRICE_HANDLE    .set(queueSegment.asSlice(slotOffset), 0L, price);
        IncomingMsgLayout.QUANTITY_HANDLE .set(queueSegment.asSlice(slotOffset), 0L, quantity);
        IncomingMsgLayout.TYPE_HANDLE     .set(queueSegment.asSlice(slotOffset), 0L, type);
        IncomingMsgLayout.SIDE_HANDLE     .set(queueSegment.asSlice(slotOffset), 0L, side);

        // release store — the consumer (C++ engine) sees the data before seeing the new head
        LONG_HANDLE.setRelease(queueSegment, HEAD_OFFSET, head + 1);
        return true;
    }

    // helper to align an offset to the next 64-byte boundary
    private static long roundUpTo64(long value) {
        return (value + 63L) & ~63L;
    }
}
