package core;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;
import java.util.Optional;

// Zero-allocation reader for the C++ LockFreeQueue<TradeMsg, 1024>.
//
// C++ struct memory layout (32 bytes, matching TradeMsgLayout.java):
//   std::array<TradeMsg, 1024> buffer_;   // offset 0
//   alignas(64) std::atomic<size_t> head_; // after buffer, 64-byte aligned
//   alignas(64) std::atomic<size_t> tail_; // 64 bytes after head_
//
// This is the consumer side (Java reads, C++ writes).
// Uses acquire/release semantics to mirror the C++ atomic ordering.
public class NativeOutboundQueueImpl {

    private static final int  CAPACITY  = 1024;
    private static final int  MASK      = CAPACITY - 1;
    private static final long ITEM_SIZE = TradeMsgLayout.STRUCT_LAYOUT.byteSize(); // 32 bytes

    private static final long BUFFER_OFFSET = 0L;
    private static final long BUFFER_SIZE   = ITEM_SIZE * CAPACITY;

    // producer (C++) writes head_, consumer (Java) writes tail_
    private static final long HEAD_OFFSET = roundUpTo64(BUFFER_SIZE);
    private static final long TAIL_OFFSET = HEAD_OFFSET + 64L;

    private static final VarHandle LONG_HANDLE = ValueLayout.JAVA_LONG.varHandle();

    private MemorySegment queueSegment;

    public void bind(MemorySegment segment) {
        // reinterpret without size bound — memory is owned by C++
        this.queueSegment = segment.reinterpret(Long.MAX_VALUE);
    }

    // Try to pop one TradeMsg from the outbound queue.
    // Returns Optional.empty() if the queue is empty.
    // Allocation-free hot path: only allocates Optional on success (rare, acceptable).
    public Optional<TradeMsg> pop() {
        // acquire load of head — need to see C++ producer's latest write
        long head = (long) LONG_HANDLE.getAcquire(queueSegment, HEAD_OFFSET);

        // relaxed load of tail: consumer is the only thread writing it
        long tail = (long) LONG_HANDLE.getAcquire(queueSegment, TAIL_OFFSET);

        if (head == tail) {
            return Optional.empty(); // empty
        }

        long slotOffset = BUFFER_OFFSET + (tail & MASK) * ITEM_SIZE;
        MemorySegment slot = queueSegment.asSlice(slotOffset);

        TradeMsg msg = new TradeMsg(
            (long) TradeMsgLayout.MAKER_ORDER_ID_HANDLE.get(slot, 0L),
            (long) TradeMsgLayout.TAKER_ORDER_ID_HANDLE.get(slot, 0L),
            (long) TradeMsgLayout.PRICE_HANDLE          .get(slot, 0L),
            (int)  TradeMsgLayout.QUANTITY_HANDLE       .get(slot, 0L)
        );

        // release store: advance tail so C++ producer sees the slot as free
        LONG_HANDLE.setRelease(queueSegment, TAIL_OFFSET, tail + 1);

        return Optional.of(msg);
    }

    private static long roundUpTo64(long value) {
        return (value + 63L) & ~63L;
    }
}
