# F07 — Preserve cached content through failed reconnection

Scope: the experimental firmware and mobile worktrees, preserving prior fixes,
BLE version 1, packet formats, UUIDs and the existing destructive `SYNC_START`.
The mobile preparation/retry changes are described in its `F07_VALIDATION.md`.

## Firmware behavior

`GameManager` tracks verification separately for each received sports, golf and
Fantasy category. The main-loop interruption handler invalidates verification
on disconnect or RX loss without clearing committed data, requesting a redraw
when the selected content first loses verification. A valid replacement
commit verifies only its category. Fantasy navigation, cancelled staging and
sync terminal commands cannot verify cached scores. The dashboard shows
`UPDATES PAUSED` while disconnected and `CACHED / UNVERIFIED` while connected
until the selected category is replaced. Existing alert/standby behavior remains.

No radio callback work, transfer semantics, dependency or production branch was
changed. Existing source relocation, `.DS_Store` and the untracked audit remain.
The verification flag fits existing structure padding on the measured build.

## Regression and build results

Before implementation, the actual display-state/disconnect harness reproduced
cached scores becoming live on reconnection without replacement data. The new
`test/cached_content_verification_test.py` covers reconnect, RX loss, independent
category verification, rejected replacements, golf, Fantasy selection/navigation,
staging/cancellation/commit and intentional empty/reset state. The warning redraw
request is checked even after a rapid reconnect or RX loss, without requiring
an observed Bluetooth state change; repeated invalidation does not redraw.

All ten native entry points passed: storage, projection, alerts, scoreboard,
sketch runner, fantasy transfers, BLE queue, responsiveness, cached verification
and the mobile packet fixture (18 steps, 112 packet operations). Applicable
storage/projection/transfer/queue/responsiveness/cache/compatibility harnesses
also passed AddressSanitizer/UndefinedBehaviorSanitizer. Responsiveness passed
with timing instrumentation enabled. The queue stress includes 70,000 concurrent
packets and 200 full bursts; simulated timing is not a hardware measurement.

ESP32-S3 compilation passed before and after with `esp32:esp32:esp32s3`, ESP32
core 3.3.11, ArduinoJson 7.4.3 and GFX 1.6.7; no dependencies changed.

| Metric | Before (`3f132b0`) | After F07 | Change |
| --- | ---: | ---: | ---: |
| Flash | 780,373 / 1,310,720 bytes (59%) | 780,585 bytes (59%) | +212 bytes |
| Static RAM | 139,912 / 327,680 bytes (42%) | 139,912 bytes (42%) | 0 |
| Linker remainder | 187,768 bytes | 187,768 bytes | 0 |

Linker remainder excludes runtime BLE/JSON/heap allocations. Reproduce from the
currently relocated worktree root:

```sh
python3 scrbrd/test/cached_content_verification_test.py
NATIVE_SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 scrbrd/test/cached_content_verification_test.py
arduino-cli compile --fqbn esp32:esp32:esp32s3 scrbrd
```

Validation logs/build artifacts are in `/tmp/scrbrd-f07/` (temporary, uncommitted).
The mobile complete suite passed 635 tests and static analysis reported no issues.

## Compatibility boundary requiring separate protocol review

`SYNC_START` still immediately clears content. Prefetching prevents provider
failure from triggering that reset, but a BLE failure after it cannot roll back
the whole catalog. ATT success is not an application commit receipt. Atomic
whole-catalog replacement, freshness timestamps/receipts, selective sports clears
and per-matchup freshness in a partially available Fantasy slate require a
separately reviewed protocol design. No such messages were introduced. The app
uses existing WAKE, foreground and FCM triggers for coalesced retry while running;
this does not promise iOS execution when suspended or terminated. Older flashed
firmware retains cache with the new app but cannot show the new unverified label.

## ESP #2 physical verification still required

No hardware was flashed or tested. In a later authorized bench run:

1. Record both experimental commit IDs and positively identify ESP #2. Load
   sports, PGA and multiple Fantasy matchups; note scores and selected matchup.
2. Disconnect BLE, disable internet while leaving Bluetooth available, then
   reconnect. Content must remain navigable; the phone reports incomplete updates
   and the selected cached category shows `CACHED / UNVERIFIED`, without loading
   forever. Disconnect again: the label must become `UPDATES PAUSED`.
3. Fail just one provider. Verify other complete sports/PGA categories update,
   only their warning clears, and a failed Fantasy member preserves the whole
   committed Fantasy slate. Navigate between categories and Fantasy entries.
4. Restore connectivity without reconnecting BLE. Check a WAKE and foreground
   return independently trigger recovery, one reset/transfer at a time, and new
   scores become visible. Repeat offline/online transitions and rapid WAKE bursts.
5. Disable/remove Fantasy during a sports failure; verify intentional Fantasy
   clearing. Verify known-empty/disabled sports clear once providers recover;
   all-known-empty schedules must show the intentional empty state.
6. Repeat interrupted staging, RX saturation and recovery while navigating and in
   standby/alert overlays. Only valid category commits may clear its warning.
   Check UART/device display, not only successful phone writes; record resets,
   minimum heap and stack watermarks during a reconnect soak.
7. Pair the new app with a previously flashed version-1 device: no incompatible
   commands should appear and offline cache must survive, while acknowledging
   that its old UI lacks the per-category unverified indicator.
