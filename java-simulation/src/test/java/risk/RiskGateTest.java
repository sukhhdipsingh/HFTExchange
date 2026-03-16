package risk;

import core.NativeSPSCQueue;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

import static org.junit.jupiter.api.Assertions.*;

// Unit tests for RiskGate — no native bridge needed, uses a stub queue.
class RiskGateTest {

    // Simple stub that records every push call so we can assert on it.
    static class StubQueue implements NativeSPSCQueue {
        record Order(long orderId, long price, int quantity, byte type, byte side) {}
        final List<Order> received = new CopyOnWriteArrayList<>();

        @Override public void bind(MemorySegment s) {}
        @Override public boolean push(long orderId, long price, int qty, byte type, byte side) {
            received.add(new Order(orderId, price, qty, type, side));
            return true;
        }
    }

    private StubQueue stub;

    @BeforeEach
    void setup() {
        stub = new StubQueue();
    }

    @Test
    void orderWithinAllLimitsPasses() {
        RiskGate gate = new RiskGate(stub,
            /* maxNotional */ 100_000_000L,
            /* maxOPS      */ 1000,
            /* posLimit    */ 50_000);

        boolean ok = gate.push(1, 10050, 10, (byte)0, (byte)0);
        assertTrue(ok);
        assertEquals(1, stub.received.size());
        assertEquals(0, gate.rejectedCount());
    }

    @Test
    void notionalCapRejectsOversizedOrder() {
        // cap is 5_000 ticks total (e.g. notional = price * qty)
        RiskGate gate = new RiskGate(stub, 5_000L, 1000, 50_000);

        // 10000 * 1 = 10000 > 5000 — should be rejected
        boolean ok = gate.push(1, 10000, 1, (byte)0, (byte)0);
        assertFalse(ok);
        assertEquals(0, stub.received.size());
        assertEquals(1, gate.rejectedCount());
    }

    @Test
    void positionLimitRejectsWhenExceeded() {
        // position limit of 5 units; buy 6 — should fail on position check
        RiskGate gate = new RiskGate(stub, Long.MAX_VALUE, 1000, 5);

        boolean ok = gate.push(1, 100, 6, (byte)0, (byte)0);
        assertFalse(ok);
        assertEquals(0, stub.received.size());
        assertEquals(1, gate.rejectedCount());
    }

    @Test
    void positionReversesOnBothSides() {
        RiskGate gate = new RiskGate(stub, Long.MAX_VALUE, 1000, 100);

        gate.push(1, 100, 50, (byte)0, (byte)0); // buy 50 -> pos = +50
        gate.push(2, 100, 30, (byte)0, (byte)1); // sell 30 -> pos = +20
        assertEquals(20, gate.grossPosition());
    }

    @Test
    void cancelOrderBypassesAllChecks() {
        // set a position limit of 0 so any new order would fail,
        // but cancel (type=1) should still pass through
        RiskGate gate = new RiskGate(stub, Long.MAX_VALUE, 1000, 0);

        boolean ok = gate.push(99, 0, 0, (byte)1, (byte)0); // cancel
        assertTrue(ok);
        assertEquals(1, stub.received.size());
        assertEquals(0, gate.rejectedCount());
    }

    @Test
    void rateLimitDropsOrdersAboveThreshold() {
        // allow only 5 orders per second
        RiskGate gate = new RiskGate(stub, Long.MAX_VALUE, 5, 100_000);

        int passed = 0;
        for (int i = 0; i < 10; i++) {
            if (gate.push(i, 10000, 1, (byte)0, (byte)0)) passed++;
        }

        assertEquals(5, passed, "only 5 orders should pass within the window");
        assertEquals(5, gate.rejectedCount());
    }
}
