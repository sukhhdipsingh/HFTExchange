package risk;

import core.NativeSPSCQueue;
import java.lang.foreign.MemorySegment;
import java.util.concurrent.atomic.AtomicLong;

// Pre-trade risk gate. Wraps NativeSPSCQueue with three hard checks:
//   1. notional cap   — price * qty <= maxNotionalPerOrder
//   2. rate limit     — token bucket, maxOrdersPerSecond tokens per second
//   3. position limit — abs(gross) <= positionLimit
// Cancel orders (type=1) bypass all checks — they reduce exposure.
// Thread-safe: multiple agent threads can call push() concurrently.
public class RiskGate implements NativeSPSCQueue {

    private final NativeSPSCQueue downstream;

    // immutable thresholds set at construction
    private final long maxNotionalPerOrder;
    private final long maxOrdersPerSecond;
    private final long positionLimit;

    // token bucket state
    private final AtomicLong tokens;        // tokens remaining this window
    private final AtomicLong lastRefillNs;  // nanotime of last full refill

    // buy = +qty, sell = -qty; we check abs(sum) <= positionLimit
    private final AtomicLong grossPosition = new AtomicLong(0);

    // monotonic counter — useful for dashboards/logging
    private final AtomicLong rejectedCount = new AtomicLong(0);

    public RiskGate(NativeSPSCQueue downstream,
                    long maxNotionalPerOrder,
                    long maxOrdersPerSecond,
                    long positionLimit) {
        this.downstream          = downstream;
        this.maxNotionalPerOrder = maxNotionalPerOrder;
        this.maxOrdersPerSecond  = maxOrdersPerSecond;
        this.positionLimit       = positionLimit;

        // start with a full bucket
        this.tokens       = new AtomicLong(maxOrdersPerSecond);
        this.lastRefillNs = new AtomicLong(System.nanoTime());
    }

    @Override
    public void bind(MemorySegment segment) { downstream.bind(segment); }

    @Override
    public boolean push(long orderId, long price, int quantity, byte type, byte side) {
        // cancels always pass through — no exposure added
        if (type == 1) return downstream.push(orderId, price, quantity, type, side);

        // check all limits; short-circuit on first failure
        if (!checkNotional(price, quantity) || !checkRateLimit() || !checkPosition(quantity, side)) {
            rejectedCount.incrementAndGet();
            return false;
        }

        boolean sent = downstream.push(orderId, price, quantity, type, side);
        if (!sent) {
            // queue was full — revert the position update we already applied
            grossPosition.addAndGet(side == 0 ? -quantity : quantity);
        }
        return sent;
    }

    // --- accessors ---

    /** total orders rejected since startup */
    public long rejectedCount() { return rejectedCount.get(); }

    /** current net position (positive = net long, negative = net short) */
    public long grossPosition() { return grossPosition.get(); }

    // --- internal checks ---

    // reject if single-order value exceeds cap
    private boolean checkNotional(long price, int qty) {
        return (price * qty) <= maxNotionalPerOrder;
    }

    // token-bucket refill + consume; returns false if bucket empty
    private boolean checkRateLimit() {
        long now  = System.nanoTime();
        long last = lastRefillNs.get();
        // refill once per second using CAS to avoid races between threads
        if (now - last >= 1_000_000_000L && lastRefillNs.compareAndSet(last, now)) {
            tokens.set(maxOrdersPerSecond);
        }
        // spin-CAS to consume one token
        long cur;
        do {
            cur = tokens.get();
            if (cur <= 0) return false;
        } while (!tokens.compareAndSet(cur, cur - 1));
        return true;
    }

    // update gross position and reject if abs value exceeds limit
    private boolean checkPosition(int qty, byte side) {
        long delta  = (side == 0) ? qty : -qty;  // buy=positive, sell=negative
        long newPos = grossPosition.addAndGet(delta);
        if (Math.abs(newPos) > positionLimit) {
            grossPosition.addAndGet(-delta);  // revert
            return false;
        }
        return true;
    }
}
