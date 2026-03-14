package core;

import java.lang.foreign.MemoryLayout;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;

// C++ IncomingMsg struct layout (24 bytes):
// uint64 order_id, uint64 price, uint32 quantity, uint8 type, uint8 side, uint8[2] pad
// TODO: double check padding if we ever change the C++ struct
public interface IncomingMsgLayout {

    MemoryLayout STRUCT_LAYOUT = MemoryLayout.structLayout(
        ValueLayout.JAVA_LONG.withName("order_id"),
        ValueLayout.JAVA_LONG.withName("price"),
        ValueLayout.JAVA_INT.withName("quantity"),
        ValueLayout.JAVA_BYTE.withName("type"),
        ValueLayout.JAVA_BYTE.withName("side"),
        MemoryLayout.paddingLayout(2)
    );

    // per-field access handles
    VarHandle ORDER_ID_HANDLE  = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("order_id"));
    VarHandle PRICE_HANDLE     = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("price"));
    VarHandle QUANTITY_HANDLE  = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("quantity"));
    VarHandle TYPE_HANDLE      = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("type"));
    VarHandle SIDE_HANDLE      = STRUCT_LAYOUT.varHandle(MemoryLayout.PathElement.groupElement("side"));
}
