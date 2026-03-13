package core;

import java.lang.foreign.MemoryLayout;
import java.lang.invoke.VarHandle;

// C++ TradeMsg struct layout:
// uint64 maker_order_id, uint64 taker_order_id, uint64 price, uint32 quantity
public interface TradeMsgLayout {

    // To be implemented in /IMPLEMENT phase
    MemoryLayout STRUCT_LAYOUT = null;

}
