package core;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;

// loads hft_bridge shared lib and resolves extern "C" functions via Project Panama Linker.
// no JNI at all — all calls go through MethodHandle downcall descriptors.
public class FFMBridgeImpl implements HFTBridge, AutoCloseable {

    private final MethodHandle engineCreateHandle;
    private final MethodHandle engineDestroyHandle;
    private final MethodHandle getInboundQueueHandle;
    private final MethodHandle getOutboundQueueHandle;
    private final MethodHandle enginePollHandle;

    // arena scoped to this bridge instance — closes when we call close()
    private final Arena arena;

    public FFMBridgeImpl(String libraryPath) {
        arena = Arena.ofConfined();

        // load the shared library from an absolute path because System.loadLibrary
        // has annoying java.library.path issues during testing
        SymbolLookup lib = SymbolLookup.libraryLookup(Path.of(libraryPath), arena);
        Linker linker = Linker.nativeLinker();

        // TODO: might want to cache these handles statically if we end up
        // creating multiple FFMBridgeImpl instances (shouldn't happen in prod but still)
        engineCreateHandle = linker.downcallHandle(
            lib.find("engine_create").orElseThrow(),
            FunctionDescriptor.of(ValueLayout.ADDRESS)
        );

        engineDestroyHandle = linker.downcallHandle(
            lib.find("engine_destroy").orElseThrow(),
            FunctionDescriptor.ofVoid(ValueLayout.ADDRESS)
        );

        getInboundQueueHandle = linker.downcallHandle(
            lib.find("engine_get_inbound_queue").orElseThrow(),
            FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS)
        );

        getOutboundQueueHandle = linker.downcallHandle(
            lib.find("engine_get_outbound_queue").orElseThrow(),
            FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS)
        );

        enginePollHandle = linker.downcallHandle(
            lib.find("engine_poll").orElseThrow(),
            FunctionDescriptor.ofVoid(ValueLayout.ADDRESS)
        );
    }

    @Override
    public MemorySegment engineCreate() {
        try {
            return (MemorySegment) engineCreateHandle.invokeExact();
        } catch (Throwable e) {
            // wrapping Throwable because MethodHandle.invokeExact is annoying
            throw new RuntimeException("engine_create failed", e);
        }
    }

    @Override
    public void engineDestroy(MemorySegment engine) {
        try {
            engineDestroyHandle.invokeExact(engine);
        } catch (Throwable e) {
            throw new RuntimeException("engine_destroy failed", e);
        }
    }

    @Override
    public MemorySegment getInboundQueue(MemorySegment engine) {
        try {
            return (MemorySegment) getInboundQueueHandle.invokeExact(engine);
        } catch (Throwable e) {
            throw new RuntimeException("engine_get_inbound_queue failed", e);
        }
    }

    @Override
    public MemorySegment getOutboundQueue(MemorySegment engine) {
        try {
            return (MemorySegment) getOutboundQueueHandle.invokeExact(engine);
        } catch (Throwable e) {
            throw new RuntimeException("engine_get_outbound_queue failed", e);
        }
    }

    @Override
    public void enginePoll(MemorySegment engine) {
        try {
            enginePollHandle.invokeExact(engine);
        } catch (Throwable e) {
            throw new RuntimeException("engine_poll failed", e);
        }
    }

    // TODO: add AutoCloseable impl so we can use this in try-with-resources
    public void close() {
        arena.close();
    }
}
