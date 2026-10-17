# H01 focused diagnostic investigation

Reviewed firmware `ee5b0c6`, mobile `9fd25f7`, the hardware summary and preserved
H01 diagnostics. No application code, dependencies, device state or protocol was
changed. No tests were run.

**Conclusion:** the leading explanation is a silently ineffective WAKE notification
subscription/delivery path after reconnection. **Confidence: moderate; root cause
not proven.** The confirmed code gap is that an open but silent subscription is
treated as healthy indefinitely. A retry/discovery future blocking before normal
gate entry remains the principal alternative. Existing logs cannot choose between
these two paths conclusively.

**Trace and evidence**

| Pipeline boundary | Evidence and interpretation |
| --- | --- |
| ESP schedules WAKE → BLE notify | `scrbrd/BluetoothManager.cpp:160–183` calls `notify()` every 20 seconds while its connection flag is true, then logs unconditionally. Fresh capture lines at 11:04:31 and 11:04:51 PM EDT confirm send attempts, not successful subscription/over-air delivery. |
| BLE stream → phone WAKE controller | `bluetooth_device_transport.dart`, `_listenForWake` (603–650), accepts only current generation/epoch/characteristic events and forwards them to `wakeNotifications`. Health becomes `listening` immediately after `listen()`, before a first received event. No last-received timestamp or silence deadline exists. |
| Subscription recovery | `_wakeSubscriptionFailed` (653–679) handles error/done with bounded 1/2/4-second retries; `_cancelWakeStream` (701–709) bounds cancellation at three seconds. These mechanisms already exist. A stream that stays open without data invokes neither error nor done, producing neither recovery nor the failed-subscription warning. |
| WAKE → automatic refresh | `connection_screen.dart`, `_handleBleWakeNotification` (286–294), logs receipt only through `debugPrint`. `_requestAutomaticRefresh` (296–327) checks connection/disposal, then awaits failed-startup retry or due discovery **before** calling `SportsOperationGate.requestLiveRefresh`. Those pre-gate paths can bypass normal live-refresh messages. |
| Sync retry → operation gate | `initial_device_sync_coordinator.dart:132–174` coalesces an existing run; `_needsRetry` is set from failures at 470. The gate logs ownership/deferral/live-start events. The saved newest entry is ownership released at 10:52:05 PM, from the gate's `finally` block (`sports_operation_gate.dart:35–45`). This weakens a still-held initial-sync-owner explanation for that attempt, but does not prove successful sync or exclude a later pending operation. |
| Live refresh → provider/write | `live_refresh_coordinator.dart:43–81` logs fetch/send stages and releases its local busy flag in `finally`. Default ESPN HTTP requests have a 10-second timeout; Sleeper requests have 12 seconds. Aggregate work and BLE writes are not equivalently bounded: `_sendPacket` awaits the write at `bluetooth_device_transport.dart:406` without a local deadline. No saved trace proves a hung request/write occurred. |
| Foreground return | `connection_screen.dart:272–283` requests resubscription and one refresh, independently, only when the cached transport state is physically connected. There is no reconnect action there when already disconnected. The callback does not clear diagnostic history or intentionally disconnect. |

Paths above are under `../scrbrd-mobile-codex/lib/` unless firmware is specified.

**Confirmed versus unresolved**

- User-observed: stale display while foreground/apparently connected, temporary
  recovery after force quit/relaunch, then failed Home/return recovery with BLE
  disconnected and an empty diagnostic panel.
- Host-confirmed: firmware send/log path continued during the serial capture;
  later Lab PID remained 13600 across process snapshots. No corresponding crash
  report was listed. These snapshots do not establish Flutter State/isolate identity.
- The 20-entry panel omits raw WAKE receipt, subscription/connection transitions,
  lifecycle transitions and initial-sync detail: these use console-only logging.
  Projection messages fill most saved entries. `projection_missing` is not evidence
  that projection processing caused this stall.
- Empty history remains unexplained. `RefreshDiagnosticHistory` starts empty for
  each `ConnectionScreen` State; its UI also exposes `clear()`. The foreground
  callback itself clears neither history nor connection. State recreation/history
  clearing cannot be distinguished with the available evidence.
- Offline BLE connection is a plausible trigger from the user's sequence, not a
  demonstrated cause. Hardware and phone captures are from different points in an
  interrupted sequence, not a synchronized end-to-end trace.

**Narrow fix recommendation, subject to confirming the boundary**

For silent notification loss, add a generation-scoped, foreground-aware WAKE
freshness deadline in the existing transport. After several missed expected
20-second intervals plus margin, use bounded resubscription; confirm recovery by
receiving a notification, not merely creating a listener. Retain bounded retry
ownership, cancel deadlines on disconnect/dispose, give foreground return a fresh
grace interval and expose failure when recovery is exhausted. If reconnection is
needed, preserve cached content. No new BLE packets or firmware rewrite is needed.

Do not reset the refresh gate or remove sync ownership based solely on these logs.
If tracing instead identifies an unresolved retry/write future, fix that specific
operation with a bounded deadline and generation-aware cleanup. A Dart timeout
alone does not cancel underlying work; stale completion must remain unable to write
into a replacement session or release another owner's lock.

**Minimum additional capture**

First capture existing console output from a fresh Lab launch, alongside timestamped
ESP serial output. Existing code already prints subscription listening/error,
physical connection changes, raw WAKE receipt, lifecycle and initial-sync detail.
This may identify the missing boundary without rebuilding.

If that is insufficient, add one bounded diagnostic sink containing:

1. Wall/monotonic time, connection generation, WAKE epoch, subscription health,
   subscribe/cancel/error/done and **last received WAKE time**; log events before
   generation rejection as well as accepted events.
2. Each automatic-refresh entry/exit and chosen branch: disconnected/disposed,
   `needsRetry`, discovery or live gate. Record running-sync generation/age,
   gate owner, queued startup count, live-active and pending-WAKE state.
3. Provider and BLE-write start/end/error/deadline, with operation IDs and durations.
   Include lifecycle transitions and `ConnectionScreen` init/dispose/history-clear
   events with State/isolate identity. Preserve more than the current 20 entries;
   avoid payloads, credentials and player-by-player projection noise.

Controlled reproduction: keep the same device, categories and recorded league
configuration; establish online live updates, then disable Wi-Fi/cellular while
keeping BLE on, disconnect/reconnect once offline, allow startup to settle, restore
internet without another reconnect and observe a known score/status change. Capture
the stall before force quitting. Test Home/return separately after capture.

Decision: no phone WAKE receipt despite fresh ESP attempts → investigate subscription/
CCCD delivery; receipt without refresh-branch exit → retry/discovery await; gate
deferral → owner/queued work; fetch completes but write does not → ATT/write path;
write completes but screen stays stale → firmware receive/commit/render path.
