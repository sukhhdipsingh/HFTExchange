package core;

import com.lmax.disruptor.RingBuffer;
import java.io.IOException;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

// zero-allocation CSV reader using memory-mapped OS page cache buffer.
// parses standard ASCII numbers directly from bytes into the Disruptor ring buffer.
// format expected: orderId,price,quantity,type,side
public class MarketDataReplay {
    
    // starts the parser pump. blocks the calling thread until EOF.
    public static void pump(String csvPath, RingBuffer<MarketDataEvent> ringBuffer) {
        try (FileChannel channel = FileChannel.open(Path.of(csvPath), StandardOpenOption.READ)) {
            // map entire file into memory. in prod we'd map in chunks if huge.
            long size = channel.size();
            MappedByteBuffer buffer = channel.map(FileChannel.MapMode.READ_ONLY, 0, size);
            
            long currentOrderId = 0;
            long currentPrice = 0;
            int currentQuantity = 0;
            byte currentType = 0;
            byte currentSide = 0;

            int col = 0;
            boolean inLine = false;

            // Simple fast ASCII parser: format "orderId,price,quantity,type,side\n"
            // no string allocations, no regex, no object garbage.
            while (buffer.hasRemaining()) {
                byte b = buffer.get();
                if (b == '\n' || b == '\r') {
                    if (inLine) {
                        publishEvent(ringBuffer, currentOrderId, currentPrice, currentQuantity, currentType, currentSide);
                        // reset for next line
                        currentOrderId = 0; currentPrice = 0; currentQuantity = 0; currentType = 0; currentSide = 0;
                        col = 0;
                        inLine = false;
                    }
                } else if (b == ',') {
                    col++;
                } else if (b >= '0' && b <= '9') {
                    inLine = true;
                    long digit = b - '0';
                    switch (col) {
                        case 0 -> currentOrderId = currentOrderId * 10 + digit;
                        case 1 -> currentPrice = currentPrice * 10 + digit;
                        case 2 -> currentQuantity = (int)(currentQuantity * 10 + digit);
                        case 3 -> currentType = (byte) digit;
                        case 4 -> currentSide = (byte) digit;
                    }
                }
            }
            
            // push last line if file doesn't end with newline
            if (inLine) {
                publishEvent(ringBuffer, currentOrderId, currentPrice, currentQuantity, currentType, currentSide);
            }
            
        } catch (IOException e) {
            throw new RuntimeException("Failed to read market data file: " + csvPath, e);
        }
    }

    private static void publishEvent(RingBuffer<MarketDataEvent> ringBuffer, long orderId, long price, int qty, byte type, byte side) {
        if (ringBuffer == null) return;
        long sequence = ringBuffer.next();
        try {
            MarketDataEvent event = ringBuffer.get(sequence);
            event.orderId = orderId;
            event.price = price;
            event.quantity = qty;
            event.type = type;
            event.side = side;
        } finally {
            ringBuffer.publish(sequence);
        }
    }
}
