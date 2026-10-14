# F06 — Firmware Bluetooth RX queue concurrency

## Current implementation and scope

The audit describes the unsynchronized shared count/String queue at `f5967a8`.
F33 commit `1c38d8c` already replaced it with the current task-safe bounded SPSC
ring in `BluetoothManager.cpp`. F34 commit `fba7263` removed an allocating copy
on dequeue. Inspection and the existing ThreadSanitizer test passed before this
assignment's changes. Retain these completed fixes; no replacement queue or
runtime firmware change was needed. This commit extends regression coverage and
records F06 qualification only.

- The configured ESP32 core 3.3.11 uses one NimBLE host task (core 0) for write
  callbacks. Arduino's loop is the only consumer. This single-producer,
  single-consumer ownership is required; adding another producer/consumer needs
  a synchronization review.
- Producer release-publishes a slot only after its String and generation are
  ready. Consumer acquire-observes publication, moves the String, then
  release-publishes that the slot is reusable. The producer acquire-observes that
  release before reusing a slot. There is no shared read/modify/write count.
- Capacity remains 56 usable packets with one spare ring slot; packet limit is
  512 bytes. Connection/generation, disconnect/loss events, advertising and WAKE
  timestamp are atomic. Dequeue transfers String ownership without another copy.
- Callbacks never wait for queue space, parse packets, render, or log payloads.
  They copy a bounded payload using the existing String API. The ESP32 compiler
  uses IDF atomic helpers where needed; do not assume every atomic operation is
  always hardware lock-free. No application lock encloses String allocation.
- Full-queue, empty-payload, oversize and allocation-failed writes set a loss event and
  advance the generation. Previously queued generations are discarded; the loop
  cancels staging and preserves committed content. Fresh transfers can recover.
  Overflow intentionally invalidates pending work rather than overwriting slots.
- BLE UUIDs, packets, FIFO ordering, WAKE behavior, dependencies and main are
  unchanged. The user's uncommitted source relocation and unrelated files remain.

## Regression coverage and results

`test/bluetooth_session_queue_test.py` compiles the actual callback/consumer
functions with native BLE/Arduino stubs. Existing tests remain, expanded with:

- 70,000 concurrent packets per run: 20,000 FIFO, 20,000 with rapid reconnects,
  and 30,000 variable-size heap-backed packets including 512-byte values.
- Retained dequeued Strings checked after their original slots are reused twice.
- 200 exactly-full 56-packet bursts (11,200 packets), alternating valid FIFO
  delivery with overflow, repeated rejection/event coalescing and clean recovery.
- Injected payload-allocation failure; concurrent connection/status/WAKE access.
  The native allocator model does not simulate the ESP32 allocator or scheduler.

Passed: expanded queue normally, ThreadSanitizer (including five additional
repeated stress runs, no reported races), and
AddressSanitizer/UndefinedBehaviorSanitizer. All nine native firmware entry points
passed: storage, projections, alerts, scoreboard, sketch runner, fantasy transfers,
RX queue, responsiveness and mobile-fixture compatibility (18 steps/112 packet
operations). Sanitizers also passed storage, projection, fantasy-transfer,
responsiveness and compatibility harnesses. Existing RX-loss scenarios verify
that staged data cannot replace committed data after a loss event.

Reproduce from this relocated worktree's root:

```sh
python3 scrbrd/test/bluetooth_session_queue_test.py
NATIVE_SANITIZERS='-fsanitize=thread -g' python3 scrbrd/test/bluetooth_session_queue_test.py
NATIVE_SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 scrbrd/test/bluetooth_session_queue_test.py
arduino-cli compile --fqbn esp32:esp32:esp32s3 scrbrd
```

ESP32-S3 compilation passed before and after using core 3.3.11, ArduinoJson 7.4.3
and GFX 1.6.7; no dependencies changed.

| Metric | Before (`fba7263`) | After F06 | Change |
| --- | ---: | ---: | ---: |
| Flash | 780,373 / 1,310,720 bytes (59%) | 780,373 bytes (59%) | 0 |
| Static RAM | 139,912 / 327,680 bytes (42%) | 139,912 bytes (42%) | 0 |
| Linker remainder | 187,768 bytes | 187,768 bytes | 0 |

Linker remainder excludes runtime BLE/JSON/heap allocations; this is not a free
heap measurement. Logs are under `/tmp/scrbrd-f06/` (temporary, not committed).

## ESP #2 physical verification still required

No hardware was flashed or tested during this assignment. A later authorized
bench run should send mixed short/512-byte bursts while navigating/rendering;
check ordering, queue saturation/recovery, callback/ATT latency and disconnects
under load. Repeat incomplete sports/golf/fantasy transfers during overflow and
reconnect; committed content must remain and a fresh complete transfer must work.
Soak with USB attached/detached and monitor resets, minimum free heap, largest
allocation block and task stack watermarks. Native sanitizers cannot qualify RF,
ESP32 allocator timing, cross-core RTOS scheduling or physical display behavior.
