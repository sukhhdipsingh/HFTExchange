package core;

// Immutable value object representing a matched trade.
// Populated from C++ TradeMsg struct via NativeOutboundQueueImpl.
// All values are in raw ticks — caller converts if needed.
public record TradeMsg(
    long makerOrderId,
    long takerOrderId,
    long price,
    int  quantity
) {}
