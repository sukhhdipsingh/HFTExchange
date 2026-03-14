package agents;

import core.NativeSPSCQueue;

// represents a single trading agent that runs on a pinned platform thread.
// agents read market signals and push orders into the inbound queue.
// no virtual threads on the hot path — we pin to a specific core.
public interface TradingAgent {

    // start this agent's thread. returns immediately, does not block.
    void start();

    // graceful shutdown — drain any pending orders and stop the thread
    void shutdown();

    // inject the queue this agent writes orders into
    void bindQueue(NativeSPSCQueue queue);
}
