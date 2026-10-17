# SCRBRD bench results — 2026-10-14

Firmware `ee5b0c6` · mobile `9fd25f7`. Tests below apply to these exact local
sources; binaries/checksums/logs: `build/hardware-validation/manifest.json`.

[Session summary and next debugging target](HARDWARE_VALIDATION_SUMMARY.md).

| Preflight | Actual result |
| --- | --- |
| Completed fixes / preserved work | All named fixes are commit ancestors; existing sources/relocation/main refs preserved |
| Mobile regression suite | PASS: 657 tests; static analysis no issues |
| Firmware native checks | PASS: all ten entry points, applicable ASan/UBSan |
| Current phone-to-firmware wire compatibility | PASS in native harness: 18 steps / 112 packets; physical startup/reconnect/cache checks passed, but automatic refresh later stalled after reconnect testing (H01); radio refresh continuity not signed off |
| ESP32-S3 compile | PASS: 780,585 flash bytes; 139,912 static RAM bytes |
| iOS native Release compile | PASS: original unsigned arm64 build; isolated Lab signed Release build 1.0.0 (1006), 25.1 MB |
| ESP #2 identity/port | IDENTIFIED after user confirmation: /dev/cu.usbmodem101, USB serial AC:A7:04:2A:19:94 |
| iPhone installed-app identity | IDENTIFIED: Xcode/Flutter Sports Hub Mobile, com.againstthespread.scrbrd, 1.0.0 (6); default experimental ID collides |
| Side-by-side Lab config/build | APPROVED setup; Firebase Lab registration created, original Firebase app unchanged; matching Lab config applied only in isolated copy; unsigned Release 1.0.0 (1006) PASS |
| Lab signing | PASS after user accepted agreement: signature verified; Lab-specific application identifier and push-capable profile match, target iPhone included |
| ESP #2 firmware flash | APPROVED and PASS: ee5b0c6 uploaded to the confirmed ESP32-S3; all four written regions hash-verified; hard reset completed |
| iOS installation/launch | APPROVED and PASS: Lab 1.0.0 (1006) installed and launched without debugger; subsequent inventory confirms original app 1.0.0 (6) alongside Lab; initial connection/loading subsequently passed by user observation |
| Boot/display confirmation | PASS by user observation: Ready for scores / connection screen after flashing ee5b0c6; phone left disconnected |

Setup observations: a sandboxed CoreDevice probe timed out; an unsandboxed read
identified the paired iPhone. Installed-app reads then failed with CoreDevice
12040 / nested 10003, explicitly reporting the phone locked. These are setup
observations, not demonstrated SCRBRD defects. The user subsequently unlocked
the phone; the read-only app inventory succeeded. ESP #2 then enumerated at the
confirmed USB port. Neither identification authorizes deployment.
Android SDK is absent; it does not
block this iOS-only bench session.

## Physical test results

Firmware upload is complete: `build/hardware-validation/esp2-flash.log`.
Esptool identified ESP32-S3 QFN56 revision 0.2 with MAC ac:a7:04:2a:19:94.
An eight-second postflash serial check captured no characters; do not infer
a runtime failure or successful display initialization from that alone.

Boot/display smoke check: **PASS by user observation**.
B01: **PASS by user observation** — NBA and MLB loaded almost instantly.
B02: **PASS by user observation** — all requested button/navigation and manual
standby/wake checks worked as expected, including the connected refresh window.
B03: **PARTIAL PASS for three normal disconnect/reconnect/cache cycles** —
cached scores remained available, but a later report identifies an automatic refresh
stall after accumulated reconnects (H01). Refresh continuity is not signed off.
Interruption during loading was not explicitly
reported and remains **NOT EXERCISED**.
B05: **PARTIAL PASS by user observation** — two leagues loaded, the non-Primary
matchup stayed selected for one minute, and switching categories away/back selected
Primary as intended. Providers later confirmed: one ESPN and one Sleeper league.
Intentional Primary switching and both leagues' team names/current totals also
passed by user comparison. Real changed-score rendering and duplicate/unchanged
transfer traffic remain unverified.
B06 sample alert: **PASS by user observation** — sample +12-point alert displayed
and cleared back to the selected matchup. Real scoring events, corrections,
aggregation and delivery retries remain **NOT EXERCISED**.
B07 alert/standby subset: **PASS by user observation** — sample alert temporarily
woke standby, standby was restored afterward, and manual wake returned the normal
matchup. USB monitor and independent-power variations remain **NOT EXERCISED**.
B08: **PASS by user observation** — NBA/MLB/fantasy cached content remained,
an unverified warning appeared during offline reconnection, and automatic recovery
worked after internet returned without another BLE reconnect. Exact recovery time
was not reported. This earlier cache/warning recovery report is retained; live
update continuity needs revalidation in light of the later H01 stall report.
B09: **NOT EXERCISED** — user reported “no third sport” for the proposed
newly enabled/unavailable-category test. No recovery result is claimed for this case.
B04: **PARTIAL, after manual app recovery** — user reported an update taking
“only a few seconds,” but also reported that the board had stopped refreshing
after accumulated disconnect/reconnect testing until Lab was closed/reopened.
Game, before/after values and timing details remain unspecified; see H01.
B10 foreground-return subset: **FAIL by user observation** — Home/return without
force quitting did not recover updates; Lab was then disconnected from BLE and
showed `No refresh diagnostics yet.` Process ID remained 13600 between host
snapshots; no corresponding crash/memory-termination report is currently listed.
Extended soak and B11: **NOT RUN**, deferred while H01 remains open.
This confirms initial loading, reported controls, normal reconnects and the reported
fantasy selection behavior and current fantasy totals comparison; no instrumented
timing, detailed live-event values, independent sports score comparison or detailed
category-order check was reported. A post-recovery update does not qualify
uninterrupted refresh across reconnects.
Natural scoring corrections, missing-stat transitions, NFL week rollover and
same-day empty-to-populated schedules remain NOT EXERCISED until observed or
covered with a separately agreed controlled fixture. Software tests do not count
as device passes.

Update after each small test stage:

| Time / zone | Case | Phone state / internet / USB | Expected | Actual / evidence | Pass, fail or pending |
| --- | --- | --- | --- | --- | --- |
| Recorded 2026-10-14 22:01:53 EDT | B01 | Lab first connection; network/USB not separately re-confirmed | NBA/MLB load without endless startup | User: “nba and mlb, worked great, pretty much instantly loaded”; timing qualitative | PASS by user observation |
| Recorded 2026-10-14 22:03:38 EDT | B02 | Lab connected; requested one-minute refresh window; BLE traffic not captured | Single clicks advance once, double clicks switch categories, hold toggles standby/wake | User: “All controls worked as expected”; no lag/skipped actions reported | PASS by user observation |
| Recorded 2026-10-14 22:08:20 EDT | B03 | Lab manual BLE disconnect/reconnect; ESP stayed powered per test instructions | Three reconnects complete; cached scores remain navigable while disconnected | User: “All three worked; cached scores remained available”; interruption during loading not explicitly reported | PASS for normal cycles; interrupted-loading variation pending |
| Recorded 2026-10-14 22:14:45 EDT | B05 selection | Lab connected; two fantasy leagues; provider(s) unspecified | Non-Primary remains selected for one minute; category return selects Primary | User: “Two leagues loaded; selection behaved as expected” | PASS for selection subset; remaining B05 variations pending |
| Recorded 2026-10-14 22:18:11 EDT | B06 sample / B07 standby | Lab connected; providers confirmed as one ESPN and one Sleeper league | Sample alert clears to selected matchup; alert temporarily wakes standby then restores it; manual wake succeeds | User: “Both alert checks worked as expected”; provider: “One of each” | PASS for sample/standby subset; real events and power variations pending |
| Recorded 2026-10-14 22:21:15 EDT | B08 | Requested Wi-Fi/cellular off, Bluetooth on, offline BLE reconnect, then internet restoration with BLE left connected | Cached NBA/MLB/fantasy remain, unverified warning appears, automatic recovery clears warning | User: “Content remained, unverified warning appeared, automatic recovery worked”; exact duration not reported | PASS by user observation |
| Recorded 2026-10-14 22:26:30 EDT | B09 availability | No additional sport available for proposed test, per user | Add a third enabled category while offline; recover without BLE reconnect | User: “no third sport”; test not run | NOT EXERCISED |
| Recorded 2026-10-14 22:29:14 EDT | B05 Primary/current totals | Lab connected; one ESPN and one Sleeper league | Intentional Primary change selects its matchup; both leagues' team names/current totals agree on Lab/ESP; preferred Primary restored | User: “Primary switching and both score comparisons passed” | PASS for Primary/current totals; real changed-score rendering pending |
| Recorded 2026-10-14 22:29:14 EDT | B04 availability | At least one loaded NBA/MLB game live, per user | Observe a real change reaching ESP automatically | User: “Yes, at least one game is live”; game/change/latency not yet reported | PENDING automatic update observation |
| Recorded 2026-10-14 22:39:40 EDT; follow-up confirmed afterward | H01 / B03–B04 | Lab foreground, apparently connected, no warning; accumulated reconnect tests; exact stall onset/duration unknown | Automatic live updates continue after reconnect without manual app recovery | User reports board stopped refreshing; force quit/relaunch restored updates, then changes took “only a few seconds” | FAIL by user observation; root cause/reproduction pending |
| Recorded 2026-10-14 23:12:24 EDT | H01 / B10 foreground return | Home/return without force quit, per user | Refresh resumes and connection recovers | User: “It remained stuck”; then BLE disconnected and `No refresh diagnostics yet`; user suspects offline BLE connection trigger | FAIL by user observation; trigger unverified |
| — | Remaining B05–B07, B10–B11 | — | See checklist | Paused for H01 evidence capture | Pending |

## Open physical failure H01 — automatic refresh stalls after reconnect testing

[Focused diagnostic investigation and proposed fix](H01_DIAGNOSTIC_INVESTIGATION.md)
traces the notification, retry, gate and lifecycle boundaries. Silent WAKE delivery
loss is the leading hypothesis; a pre-gate pending operation remains an alternative.
The precise stopping point is not established by the preserved panel alone.

**User-observed; provisional P1; root cause and repeatability unverified.**
Firmware `ee5b0c6`, mobile `9fd25f7`, Lab 1.0.0 (1006), iPhone 16 / iOS 26.6.2.

The user reported that the board was not refreshing after the accumulated
connect/disconnect/reconnect tests, and that closing and reopening Lab restored
updates. Subsequent update delay was described as “only a few seconds.”
Automatic updates should continue or recover after reconnect without requiring
manual app recovery. This blocks sign-off for unattended refresh/reconnection.

Follow-up: the user confirmed Lab was **on-screen**, appeared **connected**, and
there was **no warning** while updates were stuck. Recovery was **force quitting
via the app switcher, then relaunching**, rather than merely returning to the foreground.

The original stalled state had already been recovered when reported. Unknowns:
exact game/change, duration/onset, actual transport/subscription state despite the
connected appearance, and whether both phone and ESP warning areas were checked.
No root cause is assigned to firmware, subscriptions,
scheduling, provider fetching or BLE delivery without supporting traces.

Next evidence: preserve the current Refresh diagnostics block as a post-relaunch
baseline, then attempt a controlled reconnect reproduction with a known live game.
The in-app history is an in-memory ring of 20 entries, so pre-force-quit entries
are unavailable from that screen after relaunch.
If the stall recurs, capture diagnostics and display state before recovery.
Controlled reproduction H01-R1: preserve the existing diagnostic text first. If
already stalled, capture that state without reconnecting. Otherwise, display a
known live game with Lab foreground/unlocked, disconnect/reconnect once, allow
loading to finish, then observe automatic changes for two minutes against a live
reference. Repeat up to three cycles only while updates continue. Stop at the
first stall and capture diagnostic text, game/score/clock and connection/warning
status before recovery. This was the initial reproduction plan; later reports
include another stalled observation and an intervening app closure. A clean,
fully timestamped trigger sequence remains unestablished. Further cycling is paused.
No implementation change, reflash or reinstall has been performed for H01.
Structured record: `build/hardware-validation/hardware-failures.json`.

Further user report (recorded 11:02:44 PM EDT): ESP is currently behind/stuck;
the user also described normal refresh after closing the app, followed by no
updates while sitting on the diagnostics screen. The sequence includes an app
closure/reopen, so it is not a clean uninterrupted reconnect reproduction; exact
timing and the newest diagnostic timestamp remain unspecified. Preserve this as
a repeated stalled observation rather than assuming the original snapshot is current.

Read-only host checks reidentified ESP #2 at `/dev/cu.usbmodem101`, serial
`AC:A7:04:2A:19:94`, and found the Lab process running (PID 13600). A running
process alone does not establish notification-subscription or refresh health.
A 45-second receive-only serial capture completed from **11:04:15 to 11:05:00 PM
EDT**, with no transmitted commands, explicit DTR/RTS control, reset or flash.
Opening the USB reader can affect USB timing and is recorded as a test condition.
The capture contains 326 bytes. Its first 256-byte chunk includes buffered/partial
older WAKE logs; these are not counted as current sends. Two subsequent complete
`BLE WAKE: wake notification sent.` lines arrived at **11:04:31.198890 PM** and
**11:04:51.200660 PM**, 20.00177 seconds apart. This confirms the firmware service
continues reaching its notification send/log path. It does not establish over-air
delivery, iPhone subscription health or execution of the phone's refresh handler.
Whether USB reader opening changed the observed app behavior remains unreported.
Evidence: `build/hardware-validation/H01-serial-capture-01.bin`, matching `.txt`
and `.json` chunk timestamps. The reader was closed after capture.

Foreground-return variant result, recorded 11:12:24 PM EDT: user reported **“It
remained stuck”**, then **“no refresh diagnostics yet”** and no Bluetooth connection
after going Home and returning, without swiping Lab away. The user suspects
connecting BLE while internet is unavailable is the trigger. This is a plausible,
**unverified hypothesis**, separate from the observed failure.

Read-only process comparison found PID **13600 before and after** that check.
Crash-file metadata lists only a Runner report from September 18 and a newest
JetsamEvent from **7:24:12 PM EDT on October 14**, before this Lab installation and
the failure. No corresponding report is currently listed; old unrelated reports
were not fetched. Availability can be delayed, so this does not rule out every
termination scenario, but the empty UI history is not evidence of a process
restart between these snapshots. Evidence: `H01-lab-process-after-home.json`,
`H01-runner-crash-index.json` and `H01-jetsam-crash-index.json` in the artifact folder.

**Validation status:** pause further connection cycling and extended sign-off.
Focused follow-up should trace offline BLE connection/failed initial sync, retry
ownership, notification subscription and lifecycle recovery together with exact
game changes. Successful subset checks remain recorded; unattended refresh,
reconnection and foreground recovery are not qualified. No implementation changes
were made during this validation.

First supplied diagnostic capture: `build/hardware-validation/H01-diagnostics-225205.txt`
(20 entries, newest first, 10:52:04–10:52:05 PM; received 10:54:37 PM EDT).
Newest entry is `initial sync ownership released`; the other entries describe
Sleeper projection work, including missing projections. Owner release is logged
from a `finally` block and does not establish successful content verification.
The excerpt has no later live-refresh started/completed/deferred entries. This
alone does not establish which part of the refresh path failed or confirm that
the controlled attempt has stalled. Missing projections are not evidence of the
root cause. Next check: whether the latest timestamp advances with Lab kept
foregrounded for another minute, and whether a real reference change is absent
from the current device display. No reconnect/restart is needed for this check.

Earlier user reports remain in the chronological table above; H01 qualifies their
scope. Connection/cache success and a cleared warning alone do not establish
continuing live updates.

## Additional failure record template

ID/time + phone zone; firmware/mobile hashes; iPhone iOS and foreground/locked
state; ESP power/USB/monitor state; league/matchup/date; minimal reproduction;
expected versus actual; last good update and observed delay; logs/video reference;
recovery attempted/outcome; reproducible yes/no. Do not put credentials or push
tokens in shared notes. Preserve evidence before restarting. Record hypotheses
separately from observations; implement no fixes during this validation.

## Resolved setup blocker A01 — Apple agreement prevented Lab provisioning

Observed during the authorized signing build: Xcode reports `PLA Update available`
and denies access to the membership resource until the updated Program License
Agreement is accepted. It then cannot find a matching push-capable Lab profile;
the existing wildcard profile lacks `aps-environment`. The unsigned Lab Release
build passes, so this is an account/provisioning blocker, not a demonstrated
Flutter compile or SCRBRD runtime failure.

Resolution: the user reported accepting the agreement; the authorized signing
retry passed. The signed Lab app and saved copy both passed signature validation.
Its Lab-specific development profile includes the target iPhone and push
entitlement, expiring October 15, 2027. The subsequent Lab installation and launch
were explicitly approved and completed. Device inventory then confirmed original
app 1.0.0 (6) alongside Lab 1.0.0 (1006). No agreement was accepted by the assistant and no
capability was removed. Initial blocker evidence:
`build/hardware-validation/lab-ios-build.log`; unsigned compile evidence:
`build/hardware-validation/lab-ios-unsigned-build.log`; successful retry and
identity/profile evidence: `build/hardware-validation/lab-ios-signing-retry.log`
and `build/hardware-validation/lab-signed-verification.json`.
