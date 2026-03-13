package core;

import java.lang.foreign.MemoryLayout;
import java.lang.invoke.VarHandle;

// C++ IncomingMsg struct layout (24 bytes):
// uint64 order_id, uint64 price, uint32 quantity, uint8 type, uint8 side, uint8[2] pad
public interface IncomingMsgLayout {
    
    // filled in /IMPLEMENT phase
    MemoryLayout STRUCT_LAYOUT = null;

    // per-field access handles
    VarHandle ORDER_ID_HANDLE = null;
    VarHandle PRICE_HANDLE = null;
    VarHandle QUANTITY_HANDLE = null;
    VarHandle TYPE_HANDLE = null;
    VarHandle SIDE_HANDLE = null;
}
