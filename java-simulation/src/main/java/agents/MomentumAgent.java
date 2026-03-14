package agents;

import core.NativeSPSCQueue;
import java.util.concurrent.atomic.AtomicBoolean;

// momentum agent — chases price direction by sending aggressive market-like orders.
// looks at the last N mid-price samples and if trending up, sends buys. if down, sends sells.
// extremely simplified — purely for generating realistic order flow in simulation.
public class MomentumAgent implements TradingAgent {

    private final String name;
    private final int quantity;

    private NativeSPSCQueue queue;
    private Thread thread;
    private final AtomicBoolean running = new AtomicBoolean(false);

    // ring buffer of recent mid-price samples to detect direction
    private final long[] samples;
    private int sampleHead = 0;
    private int sampleCount = 0;

    private long nextOrderId = 9000L;

    // TODO: this should eventually read from MarketDataReplay, not a hardcoded price
    private long currentPrice = 10000L;

    public MomentumAgent(String name, int quantity, int lookback) {
        this.name     = name;
        this.quantity = quantity;
        this.samples  = new long[lookback];
    }

    @Override
    public void bindQueue(NativeSPSCQueue queue) {
        this.queue = queue;
    }

    @Override
    public void start() {
        if (queue == null) throw new IllegalStateException("queue not bound");
        running.set(true);
        thread = Thread.ofPlatform().name("agent-" + name).start(this::run);
    }

    @Override
    public void shutdown() {
        running.set(false);
        if (thread != null) {
            thread.interrupt();
            try { thread.join(500); } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private void run() {
        while (running.get()) {
            // simulate price walk for now
            currentPrice += (Math.random() > 0.5 ? 1 : -1);
            recordSample(currentPrice);
            
            byte direction = detectTrend();
            if (direction >= 0) {
                // trend detected — send aggressive order
                queue.push(nextOrderId++, currentPrice, quantity, (byte) 0, direction);
            }
            
            try { Thread.sleep(1); } catch (InterruptedException e) { break; }
        }
    }

    private void recordSample(long price) {
        samples[sampleHead % samples.length] = price;
        sampleHead++;
        if (sampleCount < samples.length) sampleCount++;
    }

    // returns 0 (buy) if trending up, 1 (sell) if trending down, -1 if not enough data
    private byte detectTrend() {
        if (sampleCount < samples.length) return -1;
        long oldest = samples[(sampleHead) % samples.length];
        long newest = samples[(sampleHead - 1) % samples.length];
        if (newest > oldest) return 0;
        if (newest < oldest) return 1;
        return -1;
    }
}
