package core;

// mutable event object pre-allocated by LMAX Disruptor ring buffer.
// avoids garbage collection overhead completely on the hot path by reusing instances.
public class MarketDataEvent {
    public long orderId;
    public long price;
    public int quantity;
    public byte type; // 0 = new, 1 = cancel
    public byte side; // 0 = buy, 1 = sell
    
    // factory for the Disruptor to preallocate the ring
    public static final com.lmax.disruptor.EventFactory<MarketDataEvent> FACTORY = MarketDataEvent::new;

    public void clear() {
        orderId = 0;
        price = 0;
        quantity = 0;
        type = 0;
        side = 0;
    }
}
