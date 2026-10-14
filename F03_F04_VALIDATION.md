# F03/F04 compatible fixes and ESP #2 validation

Scope: experimental firmware/mobile worktrees only. Packet version, UUIDs,
start/matchup/end shapes, and legacy standalone packets are unchanged.

## Implemented behavior

- Routine refresh preserves the selected fantasy identity, even if non-Primary
  entries reorder. A removed selection falls back to entry zero. Entering Fantasy
  through league navigation still selects Primary. Since the existing contract
  sends Primary first, a changed first identity intentionally selects the new
  Primary; no new navigation command or metadata is introduced.
- Staging does not redraw or activate content. Successful commit redraws once,
  respecting alert overlays and standby. Actual scores/projections become visible
  when the normal renderer resumes.
- Start/end validate version, object/type, UTF-8 and 512-byte size. Active entries
  require a nonempty identity of at most 48 bytes. Invalid entries, capacity
  failures, malformed transactions, explicit cancellation, reset, disconnect and
  30 seconds of inactivity discard staging while preserving committed content
  unless the user explicitly clears/resets it. Late identified entries/end are
  ignored after interruption until a fresh start. Legacy packets without identity
  remain supported outside active staging; legitimate empty completed slates clear
  Fantasy. Repeated identity within a valid slate replaces that entry in place.
- BLE callbacks retain disconnect events across short reconnects. Queued packets
  are tagged internally by connection generation and old-session packets are
  discarded. These tags are internal RAM metadata, not BLE packet changes.
- Mobile prevalidates/snapshots every fantasy packet, identity and capacity before
  start, owns a fantasy send until end, and cancels that send if its connection
  generation changes. A failed write sends no end. Firmware timeout/new start
  handles abandoned staging without adding an abort packet.
- Unchanged ordered fantasy content skips transfers, including display-only and
  projection refresh paths. Successful sends alone advance that local baseline;
  errors retain retry eligibility. Startup forces a send after authoritative reset,
  and reconnect invalidates the baseline. Score observation/alerts still run.

## Integrity limitations requiring review

The existing start/end markers contain no count, transfer identity, or receipt.
A silently dropped middle entry followed by a valid end is indistinguishable from
a deliberately smaller slate. Empty start/end is also a legitimate clear. These
cases cannot be detected completely without a reviewed protocol extension. No
count/ID/acknowledgement fields or new messages were added.

ATT write response still does not prove parser acceptance, commit, or rendering.
Unchanged-content suppression uses successfully completed phone writes, not a
device acknowledgement. Application receipts, idempotent retry/backpressure, and
the pre-existing RX producer/consumer race remain separate audit work; this change
does not claim to resolve F05/F06 or all transport findings. Legacy packets without
identity also cannot be assigned to a particular abandoned slate. Test both paired
experimental commits on ESP #2 for the complete behavior.

## Regression evidence

Before implementation, the actual-parser/native harness failed seven scenarios:
staged redraw/commit, selection, malformed data, timeout, disconnect, short
disconnect/reconnect, and reset. Legacy compatibility passed. Mobile regressions
failed for unchanged writes, prevalidation, and interruption at each of four
packet boundaries. Added coverage also checks failed-write restart, capacity,
identity limits, immutable packet snapshots, duplicate identity, callback queue
generation, Primary changes, baseline retries, overlays and standby.

Validation passed: 104 focused Flutter tests, the complete 577-test Flutter suite,
static analysis with no issues, all existing native firmware tests, eight actual
fantasy-parser scenarios, and session-queue callback tests. Storage/packet/queue
harnesses also passed AddressSanitizer and UndefinedBehaviorSanitizer.

ESP32 compilation passed using Arduino CLI 1.5.1, ESP32 core 3.3.11, ArduinoJson
7.4.3, GFX 1.6.7 and `esp32:esp32:esp32s3`:

| Metric | Compiled usage |
| --- | --- |
| Flash | 779,905 / 1,310,720 bytes (59%) |
| Static RAM | 139,888 / 327,680 bytes (42%) |
| Linker remainder | 187,792 bytes, before runtime BLE/JSON/heap allocations |

Compared with the preceding F24/F32 firmware build, flash increased 1,416 bytes
and static RAM increased 232 bytes. No dependency upgrades were applied.

## ESP #2 steps — physical checks still required

1. Identify ESP #2's USB port and label before flashing; use only the experimental
   firmware image and experimental mobile checkout. Record both commit IDs and
   board/library versions. Never select ESP #1 or a production image by inference.
2. Configure at least three fantasy leagues. Enter Fantasy, select league B using
   single clicks, and trigger repeated refreshes. B must stay selected; changed
   scores must appear at commit. Check phone diagnostics for unchanged-transfer
   skips. Reorder/name-change non-Primary entries and confirm stable identity.
3. Select a different Primary on the phone; confirm the newly first entry is shown.
   Double-click out of Fantasy and cycle back; Primary must be selected. Remove
   the selected matchup and confirm fallback, with other leagues retained.
4. Disconnect/out-of-range and reconnect during start, entry and end writes. Repeat
   with rapid power/Bluetooth changes. Old queued entries/end must not appear in
   the new session; a complete new slate must recover. Check UART receipt/commit
   logs and display, not just the phone's write-success state.
5. Using a test central, send a valid start/entry, then invalid version, missing
   identity/score, invalid UTF-8, oversize packet, or ninth distinct entry, then end.
   Previous content must remain. Repeat with an incomplete slate and wait at least
   30 seconds before a late entry/end; only a new start should permit recovery.
6. Clear/reset during staging; a late end must not resurrect data. Send a legitimate
   empty slate and verify clearing. Confirm an older standalone packet without
   identity still works when no multi-matchup slate blocks legacy replacement.
7. Commit an update during a fantasy alert and in standby. The overlay/backlight
   policy must remain intact; expiry/manual wake must reveal the newest selected
   committed content. Exercise button navigation during receipt.
8. Repeat full eight-matchup slates and rejected transactions for an extended run.
   Record resets, latency, free/minimum heap, largest allocation block and task
   stack watermarks if available. Static linker RAM is not measured runtime heap.

No device was flashed or physically tested by the automated implementation.
