# F33 — Navigation responsiveness and BLE stability

This is the F33 responsiveness assignment, not the audit's original F33
release/manufacturing finding. Baselines: firmware `8788f00`, mobile `f287d66`.
Only experimental firmware changed. No hardware was flashed, dependencies
upgraded, protocol formats/UUIDs changed, or mobile/main branches modified.
**Physical responsiveness and the cause of every ESP #2 symptom remain unverified.**

## Findings and evidence

### Confirmed blocking logging path; hardware attribution still a hypothesis

The installed ESP32 Arduino core 3.3.11 `cores/esp32/HWCDC.cpp` sets
`tx_timeout_ms = 100` (line 105), and `HWCDC::write()` allows 20 consecutive
ring-buffer waits (lines 540–623). A connected USB host that stops draining
output can therefore block one write for approximately 2 seconds. Entire
`println` calls can perform multiple writes. This is USB CDC backpressure,
independent of whether a phone is connected over BLE. A genuinely unplugged
CDC device uses a different replacement-buffer path and does not necessarily
exhibit that wait.

Before the fix, `handlePendingClick()` and `countBootButtonClick()` logged before
changing GameManager selection. BLE callbacks logged every payload before
returning to the stack, including its ATT response path. Those are source-backed
blocking paths, reproduced with the driver timeout modeled in native tests.
They predate F03/F04; no evidence attributes all observed regressions to those
commits. The same no-BLE test with a draining USB host was already fast at the
baseline. Whether ESP #2 actually encounters this driver condition, or a slow
renderer instead, requires the measurements below.

Fixed locations: [`scrbrd.ino:2475`](scrbrd.ino#L2475) (`setup`, zero TX wait),
[`BluetoothManager.cpp:61`](BluetoothManager.cpp#L61) (`onWrite`, no payload
logging), and [`scrbrd.ino:2356`](scrbrd.ino#L2356) (single/double click path).
Logs may be dropped under USB backpressure; correctness must not depend on
successful logging. No new input task or button timing constants were added.

### Confirmed RX data race and ordering failure

The baseline callback and loop both incremented/decremented a shared volatile
queue count without synchronization. A two-thread test of the actual callback
and consumer functions reproduced ThreadSanitizer data-race reports in
`onWrite`, `hasReceivedMessage`, and `getReceivedMessage`, followed by a packet
ordering assertion failure. This was not a simulated RF loss test.

[`BluetoothManager.cpp:23`](BluetoothManager.cpp#L23) now uses a bounded SPSC
ring: Bluedroid's single host callback task is the producer; Arduino's loop is
the only consumer. Acquire/release indices transfer slot ownership after writes
and reads finish. There is one spare slot and still **56 usable slots**. Connection,
advertising, and wake-time state shared with callbacks is atomic. The native
20,000-packet producer/consumer test passes with ThreadSanitizer. This relies on
the existing Bluedroid single-producer callback model; a future stack with
parallel writers needs a fresh review.

Oversized, empty, allocation-failed, or queue-full writes signal known RX loss
and invalidate older queued generations. Packet size remains the documented
512-byte limit. Native tests cover capacity, bounds, session isolation, and
recovery. No ATT acceptance/commit acknowledgement was added.

### Confirmed loop contention and staging lifecycle gaps

The baseline serviced BLE before polling the button, rendered synchronously
from commands/commits, and processed one queued packet per loop. It had no
upper bound on the logging waits described above. Full refresh frames also
consumed input polling time; tests exercise sustained complete valid slates,
not just no-op traffic.

[`scrbrd.ino:2504`](scrbrd.ino#L2504) polls before BLE, between packets, and before
and after display work. [`handleBluetoothMessages`](scrbrd.ino#L2309) processes
at most four packets, stopping after a 6 ms elapsed budget; an individual packet
cannot be preempted by that budget. [`flushScreenRedraw`](scrbrd.ino#L184)
coalesces background updates to one frame per iteration, defers background
frames while a click/hold is being classified, and prioritizes navigation and
wake redraws. Stored scores and periodic synchronization continue during input
and standby. Rendering remains synchronous and existing layouts are preserved.

The baseline cancelled only fantasy staging on disconnect. A completed old
sports staging buffer could survive a disconnect/reconnect and accept a late
end. The new [`serviceBluetoothTransferInterruptions`](scrbrd.ino#L2293) cancels
sports, golf, and fantasy staging on disconnect or known RX loss, including
interruptions noticed during a packet batch. Committed caches remain intact.

### Selection and category availability: behavior verified, no rewrite needed

[`GameManager::setReceivedSlate`](GameManager.cpp#L128) already preserves the
selected category and an available event ID during score refresh/reordering.
Regression coverage verifies this behavior. Routine refresh sends no
`SYNC_START`; mobile's `InitialDeviceSyncCoordinator._runOwned` sends it on
initial synchronization. Initial `SYNC_START` intentionally clears categories
and indices, while duplicate starts in an active sync are ignored. That
existing behavior remains; ordinary NBA/MLB refresh does not select another
category. F03 Primary fantasy behavior is unchanged.

Native startup tests deliver actual NBA and MLB start/chunk/end packets and
`SYNC_COMPLETE`; both categories become available without any WAKE or button
interaction. Incomplete/malformed transfers preserve the prior committed slate.
This establishes the firmware behavior for delivered valid packets; it does
not prove the phone fetched and delivered both categories in the reported
hardware session. Queue races and callback stalls are plausible contributors
to those missing-category symptoms; their incidence on ESP #2 was not measured.

## Reproducible timing evidence — simulation only

`test/responsiveness_test.py` runs actual sketch button, loop, packet parsing,
transfer lifecycle, and GameManager code. It replaces the LCD with a **40 ms
frame**, charges **2 ms per RX packet** as a workload assumption, and models
**2 seconds per backpressured serial write** until setup configures zero wait.
GPIO reads observe a time-indexed physical press script at every actual poll.
No real BLE radio, RTOS scheduling, SPI bus, USB host, or panel timing is measured.

Single press: 100–260 ms. Double: 100–230 ms and 330–460 ms. Hold:
100–1400 ms, followed by another hold to verify wake and no repeat toggling.

| Scenario | `8788f00` baseline | F33 result |
| --- | --- | --- |
| No BLE, draining USB | Visible single navigation 390 ms after release | 390 ms; unchanged |
| No BLE, stalled USB | Visible single navigation 2,390 ms after release; 2,050 ms loop gap | 390 ms; 50 ms loop gap |
| Continuous staged traffic, stalled USB | Click missed; 12,012 ms loop gap | Visible single navigation 394 ms after release; 56 ms loop gap |
| Continuous valid active-slate refresh, stalled USB | Click missed; 6,052 ms loop gap | Visible single navigation 394 ms after release; 56 ms loop gap |
| Double click with continuous traffic | Missed | Visible league change 98 ms after second release |
| Hold with continuous traffic | Missed | Standby at 1,168 ms absolute; 1,068 ms after raw press; wake verified |
| Four queued score packets | Three consumed by 150 ms, three frames | All consumed by 98 ms, two coalesced frames |

With timing instrumentation enabled, maximum **button poll interval is 40 ms**
in these simulations (the whole-loop interval is a separate measurement).
Classification-to-navigation is 0 simulated ms; navigation-to-complete-frame is
40–44 ms. Parse/commit CPU work has no separate cost in this virtual clock;
zero reported parse/commit values are **not measured ESP32 runtimes**.

The preserved single-click behavior includes **50 ms release debounce plus the
300 ms double-click window**, followed by scheduling and rendering. From the
debounced completed click, the modeled response is about 340–344 ms. A strict
300 ms deadline from physical release is not achieved by this unchanged timing
contract. We did not shorten the window or claim a hardware latency target.
Actual panel timing and GPIO-to-pixel response must be quantified on ESP #2.

Final regression suite has 13 scenarios. Against the untouched baseline snapshot,
10 fail and 3 pass; all 13 pass after the fixes. Healthy baseline, startup, and
selection tests deliberately pass before the fix, distinguishing existing valid
behavior from defects. Baseline and final logs were retained under
`/tmp/scrbrd-f33/`; these temporary artifacts are not committed.

## Verification executed

- Native GameManager storage/navigation tests and optional fantasy projection tests.
- Existing fantasy FIFO/standby/sync, scoreboard layout, cwd-independent runner,
  session queue, and all eight fantasy-transfer scenarios.
- All 13 responsiveness/synchronization scenarios, with timing off and on.
- AddressSanitizer + UndefinedBehaviorSanitizer: storage, projection, actual
  fantasy-transfer parser/lifecycle, responsiveness, and RX queue tests.
- ThreadSanitizer: actual callback/consumer code, 20,000 packets without queue
  capacity pressure, plus overflow/oversize/session recovery checks. Before:
  races and ordering failure; after: clean. Arduino `String` is represented by
  native `std::string`; hardware heap/RTOS behavior still requires bench testing.
- ESP32-S3 compile, normal and `SCRBRD_TIMING=1`, using existing core 3.3.11,
  ArduinoJson 7.4.3, GFX Library for Arduino 1.6.7. No dependency changes.

| Build | Flash / 1,310,720 bytes | Static RAM / 327,680 bytes | Linker RAM remainder |
| --- | ---: | ---: | ---: |
| Baseline `8788f00` | 779,905 | 139,888 | 187,792 |
| Normal F33 | 780,345 (59%) | 139,912 (42%) | 187,768 |
| Development timing | 784,965 (59%) | 140,608 (42%) | 187,072 |

Normal-build delta: **+440 bytes flash, +24 bytes static RAM**. Remainder is not
measured runtime free heap; BLE, Strings, parser allocations, and task stacks
still consume memory dynamically.

## Commands for the existing relocated experimental worktree

The uncommitted `scrbrd/` relocation is preserved. From the worktree root:

```sh
cd scrbrd
c++ -std=c++17 -Itest/native -I. test/game_manager_storage_test.cpp GameManager.cpp -o /tmp/scrbrd-storage
/tmp/scrbrd-storage
c++ -std=c++17 -Itest/native -I. -I"$HOME/Documents/Arduino/libraries/ArduinoJson/src" test/fantasy_projection_test.cpp -o /tmp/scrbrd-projection
/tmp/scrbrd-projection
python3 test/fantasy_alert_queue_test.py
python3 test/scoreboard_render_test.py
python3 test/test_sketch_test_runner.py
python3 test/fantasy_slate_transfer_test.py
python3 test/bluetooth_session_queue_test.py
python3 test/responsiveness_test.py
NATIVE_TIMING=-DSCRBRD_TIMING=1 NATIVE_SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 test/responsiveness_test.py
NATIVE_SANITIZERS='-fsanitize=address,undefined -g' python3 test/fantasy_slate_transfer_test.py
NATIVE_SANITIZERS='-fsanitize=thread -g' python3 test/bluetooth_session_queue_test.py
cd ..
arduino-cli compile --fqbn esp32:esp32:esp32s3 --build-path /tmp/scrbrd-f33/build-release scrbrd
arduino-cli compile --fqbn esp32:esp32:esp32s3 --build-property compiler.cpp.extra_flags=-DSCRBRD_TIMING=1 --build-path /tmp/scrbrd-f33/build-timing scrbrd
```

The CLI used locally was bundled in Arduino IDE at
`/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`.
Nothing was uploaded. F33 changes are committed at the repository's existing
tracked paths; the working-tree relocation, audit document, and `.DS_Store`
remain local and uncommitted.

## Exact manual checks on ESP #2

After a separately authorized installation by the hardware owner, use the
normal build for UX checks and the timing build for measurements. No flash was
performed as part of this task.

1. Record existing baseline symptoms on video if the old firmware remains
   installed. Record power source and whether a USB serial monitor is draining
   output. Do not attribute a stall to USB without measuring it.
2. Test with BLE disconnected and cached NBA/MLB data, then connected with both
   categories enabled. For each, perform 20 single clicks, 20 double clicks, and
   10 holds of at least 1.2 seconds to enter/wake standby. Check one action per
   gesture, wrap behavior, and immediate navigation without a later wake.
3. Repeat connected tests through at least five 20-second WAKE refresh cycles;
   also hold the button across a refresh and during large multi-packet slates.
   Confirm periodic synchronization continues while in standby.
4. On a fresh connection, confirm both NBA and MLB become available after app
   transfer completion without touching BOOT. Select a specific MLB game, wait
   for score changes/refreshes and NBA-first arrivals, and confirm the category
   and that game's event ID remain selected. Repeat with NBA selected and reordered
   slates. An initial new-session `SYNC_START` still intentionally resets navigation.
5. Interrupt NBA, MLB, PGA, and fantasy transfers with disconnects/reconnects
   at start/middle/end. Deliver incomplete/malformed transfers with a development
   sender if available. Confirm previous committed data remains navigable and
   a fresh valid transfer recovers. Test queue pressure with a burst sender and
   confirm a detected loss aborts staging rather than showing partial fantasy data.
6. Verify fantasy Primary selection, repeated refreshes, alert FIFO, alerts
   temporarily waking standby, and restoration of standby when alerts finish.
7. With USB attached, test a draining monitor, closed monitor, and a host that
   explicitly stops reading a full buffer. Repeat using power without a serial
   host. Logs can be dropped; gestures and BLE writes must continue working.
8. In the timing build collect the `F33 us max ...` line emitted at most once
   every five seconds (only when USB has room). Reboot between test conditions
   to reset maxima. Capture `ble`, `pkt`, `parse`, `commit`, `screen`, `render`,
   `poll`, `edge`, `class_nav`, and `nav_frame`. `edge` is latest GPIO ISR timestamp
   to polling recognition; IRQ service latency and overwritten bounce transitions
   are not measured. Use a logic analyzer on GPIO0 and display SPI plus high-frame-rate
   video for actual electrical-edge-to-visible-pixel timing. Compare physical
   release and debounced completion separately against the 300 ms target.

## Unresolved limitations

The physical renderer/SPI duration, BLE callback/notification scheduling, RF
loss, iOS lifecycle/provider delays, and USB condition on ESP #2 remain unmeasured.
The 6 ms RX budget limits batches, not one parsing operation, BLE stack calls,
or one synchronous display frame. A frame lasting hundreds of milliseconds can
still miss short gestures; the timing build exposes that limitation. Any such
measured bottleneck warrants a targeted follow-up instead of claiming a fix.

The compatible fantasy protocol still cannot detect a silently missing middle
entry followed by a valid end when no local RX-loss signal occurred. There is
no new count/transaction ID/ACK, and the phone may mark a rejected transfer as
sent. F05-style remote acceptance/retry and initial-sync offline cache replacement
are outside these fixes. No periodic sync path was disabled to hide latency.
