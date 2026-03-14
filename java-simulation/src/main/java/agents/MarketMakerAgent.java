package agents;

import core.NativeSPSCQueue;
import java.util.concurrent.atomic.AtomicBoolean;

// a basic market maker agent. posts both a bid and an ask around a mid price,
// and reprices them periodically when the market moves.
// this is not a real strategy — just a simulation driver for the matching engine.
public class MarketMakerAgent implements TradingAgent {

    private final String name;
    private final long midPrice;    // mid price in ticks
    private final long spread;      // half-spread in ticks
    private final int  quantity;    // order size per side

    private NativeSPSCQueue queue;
    private Thread thread;
    private final AtomicBoolean running = new AtomicBoolean(false);

    // TODO: add a configurable tick interval once we have market data replay
    private static final long SLEEP_NS = 1_000_000L; // 1ms between reprice cycles for now

    private long nextOrderId = 1000L;

    public MarketMakerAgent(String name, long midPrice, long spread, int quantity) {
        this.name     = name;
        this.midPrice = midPrice;
        this.spread   = spread;
        this.quantity = quantity;
    }

    @Override
    public void bindQueue(NativeSPSCQueue queue) {
        this.queue = queue;
    }

    @Override
    public void start() {
        if (queue == null) throw new IllegalStateException("queue not bound before start");
        running.set(true);

        // platform thread pinned is handled externally via thread affinity.
        // TODO: look into Processor.setAffinity() or taskset via ProcessBuilder at launch
        thread = Thread.ofPlatform()
            .name("agent-" + name)
            .start(this::run);
    }

    @Override
    public void shutdown() {
        running.set(false);
        if (thread != null) {
            thread.interrupt();
            try {
                thread.join(500);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private void run() {
        while (running.get()) {
            postQuotes();
            // busy-spin would be better here but this is a simulation, not prod
            try { Thread.sleep(0, (int) SLEEP_NS); } catch (InterruptedException e) { break; }
        }
    }

    private void postQuotes() {
        if (queue == null) return;

        long bid = midPrice - spread;
        long ask = midPrice + spread;

        // post buy side
        queue.push(nextOrderId++, bid, quantity, (byte) 0, (byte) 0);

        // post sell side
        queue.push(nextOrderId++, ask, quantity, (byte) 0, (byte) 1);
    }
}
