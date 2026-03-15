package core;

import agents.MarketMakerAgent;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;

class SimulationEngineTest {

    private SimulationEngine engine;
    private Path tempCsv;

    private static String resolveBridgePath() {
        String ext = System.getProperty("os.name", "").toLowerCase().contains("win") ? ".dll" : ".so";
        String lib = "libhft_bridge" + ext;
        return Path.of("../cpp-engine/build", lib).toAbsolutePath().toString();
    }

    @BeforeEach
    void setup() throws IOException {
        engine = new SimulationEngine(resolveBridgePath());
        tempCsv = Files.createTempFile("market_data", ".csv");
        // write fake market data: orderId, price, quantity, type(new=0), side(0=buy, 1=sell)
        Files.writeString(tempCsv, "1,10050,10,0,0\n2,10050,10,0,1\n");
    }

    @AfterEach
    void teardown() throws IOException {
        engine.shutdown();
        if (tempCsv != null) {
            Files.deleteIfExists(tempCsv);
        }
    }

    @Test
    void testSimulationOrchestration() {
        assertDoesNotThrow(() -> {
            MarketMakerAgent mm = new MarketMakerAgent("mm1", 10050, 10, 50);
            engine.addAgent(mm);

            engine.start();

            // pump some external data through disruptor
            engine.runReplay(tempCsv.toString());

            // let it run a bit
            Thread.sleep(100);
        });
    }
}
