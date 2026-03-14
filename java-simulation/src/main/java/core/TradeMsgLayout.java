package core;

import java.lang.foreign.MemoryLayout;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;

// C++ TradeMsg struct layout:
// uint64 maker_order_id, uint64 taker_order_id, uint64 price, uint32 quantity, uint32 pad = 32 bytes
public interface TradeMsgLayout {

    MemoryLayout STRUCT_LAYOUT = MemoryLayout.structLayout(
        ValueLayout.JAVA_LONG.withName("maker_order_id"),
        ValueLayout.JAVA_LONG.withName("taker_order_id"),
        ValueLayout.JAVA_LONG.withName("price"),
        ValueLayout.JAVA_INT.withName("quantity"),
        MemoryLayout.paddingLayout(4)
    );

    VarHandle MAKER_ORDER_ID_HANDLE = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("maker_order_id"));
    VarHandle TAKER_ORDER_ID_HANDLE = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("taker_order_id"));
    VarHandle PRICE_HANDLE          = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("price"));
    VarHandle QUANTITY_HANDLE       = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("quantity"));
}
