# F34 — Post-F33 system integrity audit

Reviewed baselines: firmware `1c38d8c`, mobile `f287d66`. This report uses
F34-01…07 to distinguish this assignment from the original production audit's
F34 documentation finding. Experimental worktrees only; nested `scrbrd/`
relocation and original audit preserved. No main changes, merges, pushes,
dependency upgrades, or hardware flashing.

## Assessment

**The combined implementation is structurally coherent in the exercised paths;
F33's responsiveness changes remain intact. Production qualification is still
incomplete.** The user reports seamless physical navigation, connectivity, and
standby after F33. This audit independently verified software behavior, not that
physical report or the F34 builds on ESP #2.

The phone fetches and normalizes data, serializes version-1 packets, and sends
ordered transfers. The firmware callback queues bounded payloads; the loop
polls BOOT, validates/stages data, commits owned snapshots, and coalesces redraws.
Committed content remains separate from incomplete staging. No replacement
architecture is warranted by this review.

Six bounded behavioral defects were repaired; one incorrect stack description
was corrected. Existing button timings, layouts, standby/alert policy, periodic
20-second WAKE, fantasy Primary rules, UUIDs, and packet formats are preserved.

## Confirmed defects and corrections

| ID / severity | Root cause and user consequence | Small correction and evidence |
| --- | --- | --- |
| **F34-01 / P1** | Sports/golf captured connection ownership separately per packet; `_sendPacket` returned successful completion for an obsolete native write. Reconnect could deliver chunks/end without a new start or advance a baseline after cancellation. | `mobile: lib/bluetooth_device_transport.dart`, `_sendTransferPackets` and `_sendPacket`: extend existing fantasy generation protection to all logical transfers; reject stale completion including the final packet. A transfer owns writes until completion; an old completion cannot unlock a new transfer. Six failing start/chunk/end reconnect regressions now pass, along with single-write cancellation, fresh-transfer ownership, retry, and existing fantasy interruption tests. |
| **F34-02 / P1** | `SportsOperationGate.runInitialSync` discarded a new startup while an older startup owned the gate, and could overlap an active refresh. A rapid reconnect could remain unsynchronized indefinitely. | `mobile: lib/sports_operation_gate.dart` queues startup ownership, prioritizes it over deferred WAKE, and waits for active refresh completion. `initial_device_sync_coordinator.dart::_runOwned` checks its generation before starting and after destructive/terminal control writes. Repeated reconnect during a blocked fetch now runs only the current queued startup; gate priority/exclusivity tests pass. |
| **F34-03 / P1** | `FantasyLiveObservationCoordinator.syncStartupCategory` captured no connection generation while its builder awaited providers. An old startup could send old scores into a new connection and establish new-session baselines. | `mobile: lib/fantasy_live_observation_coordinator.dart`: capture generation before reconciliation, guard provider startup and the built slate, and skip old-session final baseline work. Failing Sleeper and ESPN blocked-fetch/reconnect tests now pass; current-session scoring/baseline behavior remains covered. |
| **F34-04 / P2** | The same method counted a successfully sent **empty** fantasy slate as an available category. With no configured team and no sports, phone sent `SYNC_COMPLETE`; firmware had no real content and remained connected/loading. | Count nonempty slate content, retaining legacy baseline behavior only for legacy transport. Empty/configured-without-team regressions now return false, allowing initial sync's existing `SYNC_EMPTY` path. The valid empty slate still clears obsolete Fantasy data. Provider-error-versus-empty recovery remains a separate issue below. |
| **F34-05 / P2** | `firmware: GameManager.cpp::clearReceivedFantasyMatchup` always reset the current game/page index, even when removing an unrelated category. NBA game/PGA page/NCAAF selection jumped during a fantasy clear. | Reset only if removing the active category or leaving no categories. Failing storage and cross-repository clear tests now pass. Tests cover Fantasy before/between/after other categories, a PGA page, and a selected event in shared 72-game storage. Active-category fallback and Primary behavior remain unchanged. |
| **F34-06 / P2** | F33's `BluetoothManager.cpp::getReceivedMessage` used a mixed lvalue/temporary ternary return, causing an extra allocating `String` copy after releasing the slot. Arduino `String::copy` silently becomes empty on failed reserve; the loop skips an empty payload without signaling RX loss. Under memory pressure, a fantasy entry could disappear while a valid end still commits. | Return the local directly after rejecting obsolete generation, permitting NRVO/allocation-free move. A 512-byte copy-failure injection failed before and passes after; concurrency/order/epoch tests remain clean. The copy path is confirmed by C++ behavior and installed core `WString.cpp`; allocation failure was simulated, not observed on ESP #2. |
| **F34-07 / P3** | F33 described its producer as Bluedroid, but this installed ESP32-S3 SDK enables **NimBLE**, pinned to core 0. Incorrect assumptions weaken future concurrency review. | Correct firmware comment and F33 validation document. SDK `esp32s3-libs/3.3.11/qio_qspi/include/sdkconfig.h:601` confirms NimBLE. The required single-host-task producer model is unchanged. No stack/dependency replacement. |

Useful current source entry points:
[RX consumer](BluetoothManager.cpp#L211),
[Fantasy removal](GameManager.cpp#L404),
[mobile transport](/Users/peterdeluca/Projects/_worktrees/scrbrd-mobile-codex/lib/bluetooth_device_transport.dart:331),
[startup gate](/Users/peterdeluca/Projects/_worktrees/scrbrd-mobile-codex/lib/sports_operation_gate.dart:22),
[startup coordinator](/Users/peterdeluca/Projects/_worktrees/scrbrd-mobile-codex/lib/initial_device_sync_coordinator.dart:100),
[fantasy startup](/Users/peterdeluca/Projects/_worktrees/scrbrd-mobile-codex/lib/fantasy_live_observation_coordinator.dart:459).

## F33 and integration checks

- **Concurrency:** acquire/release tail publication and head release transfer
  sole ownership of `String` slots and generation tags. The spare slot retains
  56 usable entries. No shared mutable count remains. Connection/advertising/
  wake-time callback state is atomic. No queue race, deadlock, or corruption was
  reproduced after fixes in the native model. Assumption: one BLE host producer
  and one loop consumer; future parallel callback writers need renewed review.
- **Overflow/session lifecycle:** oversized/empty/enqueue-allocation failures
  and overflow invalidate queued generations and signal cancellation. Loop
  cancels sports/golf/fantasy staging while preserving committed caches.
  Tests cover full capacity, wrap/reuse, ordinary/rapid reconnect, and recovery.
  The dequeue generation check is not a transaction-wide lock against a callback
  occurring later during parsing/commit; hardware race qualification remains necessary.
- **Buttons/rendering:** F33 loop and renderer scheduling are unchanged.
  Polling precedes RX, occurs between bounded packet batches, and brackets
  rendering. Dirty updates survive standby/overlays. Background frames defer
  during gesture classification; navigation/wake retain priority. Single/double
  clicks, holds, bounce, and three-click sequences pass under sustained traffic.
  Alert FIFO, expiry, overflow/wrap, temporary standby wake, and restoration pass.
- **Data/selection:** NBA, MLB, NFL, NCAAF, golf, and fantasy serialized transfers
  agree on sizes, fields, versions and order. Refresh preserves available event
  identity and selected category; intentional Primary change/league navigation
  remains distinct. Empty Fantasy clear now preserves unrelated selection.
  Generic NHL packets also work in firmware/serializer tests, but **NHL has no
  mobile `SportsLeague`/provider/category integration**; this audit adds no feature.
- **Initial/routine sync:** enabled supported categories are sent in configured
  order when providers succeed. Routine canonical sports/golf comparisons and
  ordered fantasy equality skip unchanged data. Initial `SYNC_START` still
  intentionally resets the catalog; ordinary refresh does not send it.
- **Scheduling:** firmware WAKE remains every 20 seconds while connected, with
  rollover/disconnect guards tested. Phone gate keeps one coalesced follow-up
  refresh, prioritizes queued startup, and isolates sports/fantasy failures.
  No self-generated WAKE echo loop was found. A continuously arriving WAKE can
  keep expensive refresh work busy, but does not grow an unbounded refresh queue.
- **Failures/memory:** malformed/count-mismatched/incomplete transfers do not
  replace previously committed storage in covered cases; expiry/reset/disconnect
  and fresh-transfer retry pass. No native ASan/UBSan fault or leak was detected.
  Native strings/graphics/radio stubs do not qualify ESP32 heap, stacks or RF.

## Remaining production priorities — not silently “fixed”

| Priority / classification | Evidence and likely impact | Next action / why deferred |
| --- | --- | --- |
| **R1 / P1 / confirmed protocol acceptance gap** | Firmware ATT response acknowledges enqueue/callback completion, not validation/commit. Phone records delivered baselines after write futures. Overflow, parser OOM or capacity rejection can leave stale content suppressed as “unchanged.” Countless fantasy envelopes cannot detect an unreported missing middle entry; the alert FIFO can reject a backlog that the phone marks delivered. | Review backward-compatible receipts, complete-transfer identity/counts, retry/idempotency and backpressure together. New packet/status contracts are explicitly outside automatic changes. Keep prior F03/F04 guarantees limited to detectable interruptions/invalid packets. |
| **R2 / P1 / confirmed stale-data and startup recovery gaps** | `LiveRefreshCoordinator.refreshTrackedSessionOnce` enumerates only delivered `TrackedDeviceSession` entries and reuses their original date/tournament. Initially failed/empty categories are not rediscovered; midnight/tournament rollover is absent. Initial sync sends destructive `SYNC_START` before provider success, and the fantasy builder collapses provider failure into omitted/empty entries. | Define consumer “today” versus historical Developer Tools intent, desired categories versus delivery baselines, and empty versus unavailable results. Then implement bounded discovery/rollover/recovery and offline-cache preservation. Changing catalog reconciliation/empty semantics without that contract risks removing working/historical content. |
| **R3 / P1 / confirmed alert aggregation gap; presentation decision required** | `FantasyLiveObservationCoordinator._drainPendingAlertsAcrossSessions` groups by canonical player, labels `group.length` as leagues, presents the first delta/totals, and consumes all members. A failed-delivery backlog with two transitions in one league can say “2 leagues” and omit a transition. Existing tests intentionally accept first-value cross-league presentation. | Agree on distinct-league/event grouping, corrections and score presentation, and pacing into the device's 16-entry waiting FIFO. Preserve present behavior until that delivery/presentation policy is reviewed; do not introduce more unacknowledged bursts as an isolated patch. |
| **R4 / P2 / hardware-dependent uncertainty** | Golf chunk parsing allocates `GolfLeaderboardRow[50]` (5,850 local bytes); configured loop stack is 8,192 bytes. Runtime heap fragmentation/free blocks, stack margin, SPI frame duration, NimBLE scheduling and iOS background behavior were not measured here. iOS declares `bluetooth-central`; capability declaration is not a soak result. | Qualified ESP/iOS bench review: stack high-water marks, free/minimum/largest heap blocks, sustained mixed traffic, weak RF, locked/background phone and interrupted API calls. Optimize only measured bottlenecks. The per-batch 6 ms budget cannot preempt one slow parser/stack call/frame. |

No newly proven P0 crash, buffer overrun, uncontrolled traffic loop or persistent
queue leak was found. “Not reproduced” does not establish absence on hardware.
R1–R3 are confirmed limitations, not speculative hardware findings; R4 is an
uncertainty. R1 requires protocol review; R2/R3 require meaningful state/policy
choices, so this assignment documents them instead of changing working contracts.

## Executed verification and exact counts

- **Flutter:** 591/591 full-suite tests passed; static analysis: **no issues**.
  Baseline source snapshot `f287d66` replayed against final focused expectations:
  16 failures among 130 transport/startup/multileague/gate tests. This includes
  correcting existing stale-success and empty-content expectations, not only new tests.
- **Firmware:** all **9 entry points** passed: 2 C++ binaries (storage,
  projections) and 7 Python runners (alert lifecycle, scoreboard, sketch runner,
  fantasy transfers, RX queue, input/loop, mobile compatibility). Subcoverage:
  **8** fantasy-transfer scenarios, **15** input/loop scenarios, **10** scoreboard
  cases, **1** Python unittest, and two concurrent **20,000**-packet sweeps
  (40,000 total; second sweep includes rapid reconnects). Capacity, simulated
  copy failure and 20-second WAKE/rollover checks are additional assertions.
- **Cross-repository:** advertising name/three UUIDs, **18 steps and 112 packet
  operations** generated in the mobile test and consumed by real firmware
  parsing/storage/lifecycle code. Includes one deliberately malformed end and
  incomplete-transfer negative tests; 72 games, 50 golfers, eight fantasy entries,
  MLB/NFL state, stable selection, Primary, standby/wake and timeout.
- **Sanitizers:** ASan + UBSan passed storage/projection, actual fantasy parser,
  input/timing, RX ownership and cross-repository harnesses. TSan passed the RX
  callback/consumer code, both 20,000-packet sweeps and session/overflow recovery.
  Allocation failure uses a native proxy matching Arduino `String` copy failure;
  this is not a physical allocator stress result.
- Baseline firmware snapshot `1c38d8c` failed the new storage, RX-copy-failure and
  cross-repository selection checks. Final versions pass. Logs/artifacts are in
  `/tmp/scrbrd-f34/`, not committed.

Both final ESP32-S3 variants compiled with installed core 3.3.11, ArduinoJson
7.4.3, GFX 1.6.7; no upload:

| Build | Flash / 1,310,720 | Static RAM / 327,680 | Linker RAM remainder |
| --- | ---: | ---: | ---: |
| Normal | 780,373 (59%) | 139,912 (42%) | 187,768 |
| `SCRBRD_TIMING=1` | 784,997 (59%) | 140,608 (42%) | 187,072 |

Compared with F33: normal flash **+28 bytes**, static RAM **unchanged**.
Linker remainder is not measured runtime heap. The F33 modeled response remains
390–394 ms after physical single-click release, 98 ms after double-click release,
and 40 ms maximum poll gap for a simulated 40 ms LCD. These are not new physical
measurements or proof of a strict 300 ms physical-release target.

## Reproduction and ESP #2 qualification

From the mobile experimental root, export and test without dependency updates:

```sh
SCRBRD_PROTOCOL_FIXTURE=/tmp/scrbrd-protocol.json flutter test --no-pub test/firmware_protocol_compatibility_test.dart
flutter test --no-pub
flutter analyze --no-pub
```

From the firmware experimental root (existing nested layout):

```sh
python3 scrbrd/test/mobile_protocol_compatibility_test.py /tmp/scrbrd-protocol.json
NATIVE_SANITIZERS='-fsanitize=address,undefined -g' python3 scrbrd/test/mobile_protocol_compatibility_test.py /tmp/scrbrd-protocol.json
NATIVE_SANITIZERS='-fsanitize=thread -g' python3 scrbrd/test/bluetooth_session_queue_test.py
arduino-cli compile --fqbn esp32:esp32:esp32s3 --build-path /tmp/scrbrd-f34/build-release scrbrd
arduino-cli compile --fqbn esp32:esp32:esp32s3 --build-property compiler.cpp.extra_flags=-DSCRBRD_TIMING=1 --build-path /tmp/scrbrd-f34/build-timing scrbrd
```

Native storage/projection commands and remaining runners are in README/F33
validation. After a **separately authorized installation by the hardware owner**:

1. Repeat F33's gesture/standby checks through mixed NBA/MLB/NFL/NCAAF/PGA/fantasy
   refreshes; compare responsiveness with the previously working F33 image.
2. Interrupt sports, golf and fantasy at start/middle/end and during provider
   fetch, including rapid reconnect before the old future completes. Confirm
   current startup finishes and fresh data replaces only complete valid slates.
3. Clear Fantasy while viewing NBA game >1, NCAAF game >20, and PGA page >1.
   Confirm that selection survives; repeat Primary changes and category cycling.
4. Refresh scores during standby and alert overlays, then wake/expire them.
   Confirm latest data appears without another BLE event and alert FIFO policy remains.
5. Run an overnight/weak-RF/phone-background soak, including 72-game and 50-golfer
   transfers. Record actual GPIO-to-pixel timing, reset reasons, heap and stack
   minima; check R1–R3 explicitly rather than treating clean tests as qualification.

Fixes and test/report changes are committed separately per repository. No push
or merge is authorized by this assignment. Relocation, original audit and
`.DS_Store` remain local and uncommitted; no existing working files were discarded.
