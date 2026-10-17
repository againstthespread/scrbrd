# SCRBRD hardware validation summary — October 14, 2026

Scope: the session beginning with “Software checks pass for firmware `ee5b0c6`
and mobile `9fd25f7`, including all listed fixes.” All times below are EDT.

**Outcome:** approved experimental deployment completed and several physical
checks passed. **Hardware sign-off remains incomplete because H01, an automatic
refresh/connection recovery failure, is open.** General connection cycling and
extended testing are paused for focused debugging.

**Verified baseline**

- Firmware `ee5b0c6`, branch `codex/firmware-production-fixes`, in `scrbrd-codex`;
  mobile `9fd25f7`, branch `codex/mobile-production-fixes`, in `scrbrd-mobile-codex`.
  Ancestry includes the named completed fixes; exact mappings are in the manifest.
- Previously completed checks: **657 Flutter tests**, static analysis with no
  issues, **ten native firmware test entry points** with applicable ASan/UBSan,
  and a current-mobile/real-firmware-parser fixture covering **18 steps/112 packets**.
- ESP32-S3 build passed: **780,585/1,310,720 bytes flash** and
  **139,912/327,680 bytes static RAM**. Remaining linker RAM is not measured free heap.
- Original unsigned iOS Release build passed; the separate Lab Release build,
  code signature, push entitlement and device provisioning also passed.

**Deployment and preservation**

1. Identified ESP #2 at `/dev/cu.usbmodem101`, serial `AC:A7:04:2A:19:94`, with
   ESP #1 off. Explicitly approved flash of `ee5b0c6` succeeded; all four written
   regions passed hash verification. User confirmed the ready/connection screen.
2. Identified the working iPhone app as `com.againstthespread.scrbrd`, 1.0.0 (6).
   Installing the default experimental build would have replaced it.
3. With approval, created separate Lab Firebase/Apple registrations and matching
   configuration: Firebase project `scrbrd-45109`, Apple team `37Y2R545AR`.
   The original Firebase app was preserved. Identity/config changes were confined
   to an isolated build copy, preserving both source worktrees.
4. Apple signing initially required its updated Program License Agreement. The
   user accepted it, then signing succeeded with the matching iPhone profile.
5. After separate installation/launch approval, installed **SCRBRD Lab 1.0.0 (1006)**,
   **`com.againstthespread.scrbrd.lab`**, as Release without a debugger. Device
   inventory confirmed Lab alongside original build 6. No uninstall was performed.
6. Source hashes and HEAD/main refs were rechecked at summary preparation. The
   existing firmware relocation into `scrbrd/` and unrelated files remain intact;
   mobile is clean. No application-logic/dependency/protocol changes, new source
   commits, merges or pushes were made during this validation session.

**Physical results — user observations unless specified**

| Check | Result and qualification |
| --- | --- |
| NBA/MLB startup | PASS: loaded almost instantly |
| Buttons, category navigation, manual standby/wake | PASS, including a connected refresh window |
| Three normal reconnect cycles | Connection/cache subset PASS; cached scores remained. Later H01 blocks refresh-continuity sign-off |
| Fantasy selection | PASS subset: ESPN and Sleeper tested; non-Primary selection survived one minute; category return intentionally selected Primary |
| Primary changes/current totals | PASS: expected matchup selected; both leagues' team names/current totals matched Lab and ESP |
| Sample Chase +12-point alert | PASS subset: appeared, expired and returned to selected matchup |
| Alert during standby | PASS subset: temporary wake, return to standby and manual wake worked |
| Offline reconnect/internet restoration | Cache, unverified warning and recovery initially reported PASS; continuing updates need revalidation due to H01 |
| Live sports update | A few seconds reported after force-quit/relaunch recovery; exact game/change not recorded; continuous operation unqualified |
| Missing-category recovery | NOT EXERCISED: no suitable third sport available |
| Home/foreground recovery | FAIL: remained stuck, then BLE disconnected and diagnostic history appeared empty |
| Extended soak/midnight/PGA rollover | NOT RUN; deferred |

**H01 — provisional P1, root cause unconfirmed**

After accumulated reconnect/offline testing, updates stopped while Lab was
foregrounded, appeared connected and showed no warning. Force quitting and
relaunching restored updates, at least temporarily. A later Home/return without
force quit did not recover updates; Lab became disconnected and showed
`No refresh diagnostics yet.` Connecting BLE while offline is the user's
**plausible, unverified trigger hypothesis**.

Evidence preserved:

- Twenty diagnostic entries end at **10:52:05 PM: initial sync ownership released**.
  The preceding projection/missing-projection entries do not establish the cause.
  Ownership release is logged in `finally`, so it does not prove successful sync.
- A 45-second receive-only serial capture caught fresh firmware WAKE-send logs at
  **11:04:31.198890 PM** and **11:04:51.200660 PM**, twenty seconds apart. Initial
  buffered lines were excluded. This confirms send attempts, not iPhone receipt.
  Opening the USB reader can affect timing and was recorded as a test condition.
- Lab PID remained **13600** across the later Home/return snapshots. No matching
  crash or memory-termination report was listed near the failure. Empty history
  alone does not establish a process restart.
- Exact onset, game/event values, full league configuration and notification/task
  state remain unknown. Later logs include additional Sleeper configurations;
  the earlier comparison covered two matchups. Some reproduction attempts included
  app closure/reopening, so a clean trigger sequence is not established.

**Next work**

See the subsequent [focused H01 investigation](H01_DIAGNOSTIC_INVESTIGATION.md)
for source evidence, the leading hypothesis and the exact missing trace.

Trace a healthy online connection and an offline BLE connection/failed startup,
including firmware WAKE attempts, phone receipt/subscription health, initial-sync
retry/operation ownership, provider results, BLE writes and lifecycle transitions.
Preserve the stalled state before recovery. No H01 fix has been implemented.

After H01 is understood/addressed, rerun reconnect/offline/foreground tests, then
complete missing-category recovery, interrupted transfers over radio, real fantasy
score changes/corrections/aggregation, MLB bases/outs, independent-power variations,
extended soak, midnight/timezone/PGA rollover and historical-date preservation.
Lab APNs/FCM delivery remains unqualified; the BLE sample alert does not test it.

**Local records**

- [Setup and exact commands](HARDWARE_SETUP.md)
- [Staged checklist](HARDWARE_VALIDATION.md)
- [Chronological results and H01](HARDWARE_VALIDATION_RESULTS.md)
- Ignored `build/hardware-validation/`: manifest/checksums, firmware binaries and
  ELF/map, signed `SCRBRD-Lab.app`, build/test/flash/install logs, observations,
  `hardware-failures.json`, diagnostic text, serial capture and process/crash-index
  evidence. Artifacts and validation Markdown documents remain local/uncommitted.
- Isolated Lab source: `/tmp/scrbrd-hardware-validation/ios-source` (temporary).
  The signed app is saved in the persistent artifact folder above.
