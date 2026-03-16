package core;

import agents.TradingAgent;
import com.lmax.disruptor.BlockingWaitStrategy;
import com.lmax.disruptor.RingBuffer;
import com.lmax.disruptor.dsl.Disruptor;
import com.lmax.disruptor.dsl.ProducerType;
import com.lmax.disruptor.util.DaemonThreadFactory;
import risk.RiskGate;

import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

// Orchestrates the trading agents, market data replay, and the C++ matching engine.
// Uses Disruptor to multiplex market data and passes orders via FFM bridge.
public class SimulationEngine {

    private final FFMBridgeImpl bridge;
    private final MemorySegment enginePtr;

    // Raw SPSC queue (C++ memory) wrapped for multi-producer access
    private final SynchronizedQueue rawSyncQueue;

    // Risk gate sits in front of the raw queue; all agent orders flow through it.
    private final RiskGate riskGate;

    private final List<TradingAgent> agents = new ArrayList<>();
    private final AtomicBoolean running = new AtomicBoolean(false);
    private Thread pollThread;

    private Disruptor<MarketDataEvent> disruptor;
    private RingBuffer<MarketDataEvent> ringBuffer;

    public SimulationEngine(String bridgeLibPath) {
        this.bridge = new FFMBridgeImpl(bridgeLibPath);
        this.enginePtr = bridge.engineCreate();
        if (this.enginePtr == null || this.enginePtr.address() == 0) {
            throw new IllegalStateException("Failed to create native engine");
        }

        NativeSPSCQueueImpl rawQueue = new NativeSPSCQueueImpl();
        rawQueue.bind(bridge.getInboundQueue(enginePtr));

        // wrap the SPSC queue to support multiple agents pushing concurrently
        this.rawSyncQueue = new SynchronizedQueue(rawQueue);

        // pre-trade risk gate — conservative defaults for simulation
        // notional cap: 10M ticks, 500 orders/sec, 100k gross position
        this.riskGate = new RiskGate(rawSyncQueue, 10_000_000L, 500, 100_000);

        setupDisruptor();
    }

    private void setupDisruptor() {
        int bufferSize = 4096; // must be power of 2
        disruptor = new Disruptor<>(
            MarketDataEvent.FACTORY,
            bufferSize,
            DaemonThreadFactory.INSTANCE,
            ProducerType.SINGLE, // MarketDataReplay parser is single producer
            new BlockingWaitStrategy()
        );

        // Disruptor consumer polls the RingBuffer and pushes to the Native queue
        disruptor.handleEventsWith((event, sequence, endOfBatch) -> {
            boolean pushed = false;
            // retry until queue has space
            while (running.get() && !pushed) {
                pushed = inboundQueue.push(event.orderId, event.price, event.quantity, event.type, event.side);
                if (!pushed) {
                    Thread.yield();
                }
            }
        });

        ringBuffer = disruptor.start();
    }

    public void addAgent(TradingAgent agent) {
        // agents write through the risk gate, not directly to the raw queue
        agent.bindQueue(riskGate);
        agents.add(agent);
    }

    public RiskGate riskGate() {
        return riskGate;
    }

    public void start() {
        running.set(true);
        
        // start engine polling thread (drains C++ queue and runs matching logic)
        pollThread = Thread.ofPlatform().name("engine-poller").start(() -> {
            while (running.get()) {
                bridge.enginePoll(enginePtr);
                // sleep a bit to avoid maxing out a core completely if idle
                try {
                    Thread.sleep(1); 
                } catch (InterruptedException e) {
                    break;
                }
            }
        });

        // start agents
        for (TradingAgent agent : agents) {
            agent.start();
        }
    }

    public void runReplay(String csvPath) {
        System.out.println("Starting market data replay from: " + csvPath);
        MarketDataReplay.pump(csvPath, ringBuffer);
        System.out.println("Market data replay complete.");
    }

    public void shutdown() {
        running.set(false);
        for (TradingAgent agent : agents) {
            agent.shutdown();
        }
        if (pollThread != null) {
            pollThread.interrupt();
            try {
                pollThread.join(1000);
            } catch (InterruptedException ignored) {}
        }
        if (disruptor != null) {
            disruptor.shutdown();
        }
        if (enginePtr != null) {
            bridge.engineDestroy(enginePtr);
        }
        bridge.close();
    }

    // Since the native queue is SPSC and multiple agents (plus the Disruptor event handler)
    // are writing to it, we need an MPSC wrapper. We use standard Java synchronization to
    // keep it simple for simulation purposes.
    private static class SynchronizedQueue implements NativeSPSCQueue {
        private final NativeSPSCQueue delegate;

        public SynchronizedQueue(NativeSPSCQueue delegate) {
            this.delegate = delegate;
        }

        @Override
        public void bind(MemorySegment segment) {
            delegate.bind(segment);
        }

        @Override
        public synchronized boolean push(long orderId, long price, int quantity, byte type, byte side) {
            return delegate.push(orderId, price, quantity, type, side);
        }
    }
}
