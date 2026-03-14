package core;

import org.junit.jupiter.api.*;
import org.junit.jupiter.api.TestMethodOrder;
import org.junit.jupiter.api.MethodOrderer.OrderAnnotation;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

// integration test for the full Java -> C++ message path.
// requires hft_bridge.so to be built and on the path.
// TODO: make LIB_PATH configurable via -Dhft.bridge.path system property
@TestMethodOrder(OrderAnnotation.class)
class FFMBridgeTest {

    private static final String LIB_PATH = "../cpp-engine/build/libhft_bridge.so";

    private static FFMBridgeImpl bridge;
    private static MemorySegment engine;
    private static NativeSPSCQueueImpl inboundQueue;

    @BeforeAll
    static void setup() {
        bridge = new FFMBridgeImpl(LIB_PATH);
        engine = bridge.engineCreate();
        Assertions.assertNotNull(engine, "engine_create returned null");
        Assertions.assertNotEquals(0L, engine.address(), "engine pointer is null");

        // bind our Java queue wrapper to the inbound queue raw memory
        inboundQueue = new NativeSPSCQueueImpl();
        inboundQueue.bind(bridge.getInboundQueue(engine));
    }

    @AfterAll
    static void teardown() {
        if (engine != null) bridge.engineDestroy(engine);
        if (bridge != null) bridge.close();
    }

    @Test
    @Order(1)
    void pushOneOrderAndPoll() {
        // push a single buy limit order
        boolean pushed = inboundQueue.push(
            1L,       // order_id
            10050L,   // price = 100.50
            100,      // quantity
            (byte) 0, // type = new order
            (byte) 0  // side = buy
        );
        Assertions.assertTrue(pushed, "push returned false - queue is full?");

        // poll once — the engine should consume the message and add order to the book
        bridge.enginePoll(engine);
    }

    @Test
    @Order(2)
    void pushCrossingOrderAndExpectMatch() {
        // push a sell at the same price — this should match the buy from the previous test
        boolean pushed = inboundQueue.push(
            2L,
            10050L,
            100,
            (byte) 0, // new order
            (byte) 1  // side = sell
        );
        Assertions.assertTrue(pushed, "sell push failed");
        bridge.enginePoll(engine);

        // TODO: once outbound TradeMsg queue is wired up in C++, read from
        // getOutboundQueue() here and assert that a TradeMsg was emitted.
        // for now, just asserting it didn't crash is good enough.
    }

    @Test
    @Order(3)
    void cancelNonExistentOrderIsNoOp() {
        // cancelling an order that doesn't exist should not crash the engine
        boolean pushed = inboundQueue.push(
            9999L,
            0L,
            0,
            (byte) 1, // type = cancel
            (byte) 0
        );
        Assertions.assertTrue(pushed);
        bridge.enginePoll(engine);
        // if we get here without a segfault, good
    }

    @Test
    @Order(4)
    void pushManyOrdersDoesNotOverflowQueue() {
        // cap at 500 to leave headroom — queue capacity is 1024
        // and previous tests already used some slots
        int pushed = 0;
        for (int i = 100; i < 600; i++) {
            boolean ok = inboundQueue.push(
                (long) i,
                10000L + i,
                10,
                (byte) 0,
                (byte) (i % 2 == 0 ? 0 : 1)
            );
            if (!ok) break;
            pushed++;
        }

        // drain the queue so we don't leave it in a dirty state for other tests
        for (int i = 0; i < pushed; i++) {
            bridge.enginePoll(engine);
        }

        Assertions.assertEquals(500, pushed, "expected all 500 pushes to succeed");
    }
}
