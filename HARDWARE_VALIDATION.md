# SCRBRD integrated bench check — ESP #2 + iPhone

Run one stage at a time. **PAUSED for H01 evidence capture:** the user reported
automatic refresh stopped after accumulated reconnects and resumed only after
force quitting/relaunching Lab. It was on-screen, apparently connected, with no
warning during the stall. Builds, boot/display, controls, connection/cache checks,
fantasy selection/Primary/current totals, sample alerts and standby behavior have
passed their reported subsets. Automatic refresh across reconnects is not signed
off. A few-seconds live update was reported after manual recovery. Remaining
variations stay pending; B09 is unexercised because no third sport was available.
Home/return without force quit later failed to recover updates and left BLE
disconnected with empty diagnostics. Host process ID was unchanged. Offline BLE
connection is a suspected trigger, not a confirmed cause. Further cycling/soak
tests are deferred for focused H01 debugging.
Firmware `ee5b0c6`; mobile `9fd25f7`. Use these local experimental worktrees,
not the older remote branch tips. [Setup/commands](HARDWARE_SETUP.md) ·
[Results and failures](HARDWARE_VALIDATION_RESULTS.md).

## 1. Identify, then obtain installation approval

- [x] Power off ESP #1; connect only the physically labelled ESP #2 by data USB.
      Confirm its port by unplug/replug. Unlock/connect the iPhone; identify the
      working app's bundle ID. No upload/install is authorized by doing this.
- [x] Review the exact firmware image/port and the iOS installation route.
      ESP #2 flash and separate Lab installation/launch were explicitly approved
      and completed. Device inventory confirms both Lab build 1006 and original
      build 6 remain installed with distinct bundle IDs.
- [ ] After approved installation, close/disconnect the old app. Configure only
      two sports categories for B01. Add two selected fantasy matchups when reaching
      stage 3; record their order/Primary. Allow Bluetooth. Record iOS notification
      permission.

## 2. First connection and buttons — about 5 minutes

- [x] **B01:** First connection/startup passed by user observation: NBA and MLB
      loaded almost instantly. Detailed order/score comparison remains unverified.
      Expected: one coherent startup, phone and
      device show matching content, configured category order, no endless loading.
      No available games is an empty result, not a failure; a provider error is not
      proof of emptiness. Preserve a photo of the first successful slate.
- [x] **B02:** All requested controls passed by user observation. Single click
      advances game/page/matchup; double click advances
      category; hold about one second toggles standby. A single click intentionally
      waits for the 300 ms double-click window. Try ten clicks with no BLE traffic,
      then while refreshing. Record multi-second lag or skipped/double actions.
- [x] **B03, normal cycles:** Three disconnect/reconnect cycles passed by user
      observation for connection/cache; cached scores remained available.
- [ ] **B03, refresh continuity — H01:** Later automatic refresh stall reported
      after accumulated reconnects; closing/reopening Lab recovered it. Capture
      evidence and reproduce before signing off continuous updates.
- [ ] **B03, interrupted loading:** Disconnect/reconnect during loading. This
      variation has not been confirmed. Cached data should remain during BLE disconnection and read
      `UPDATES PAUSED`; no old/interrupted transfer may corrupt the next session.
      Rebooting the ESP clears RAM cache and is a separate test.

## 3. Scores and Fantasy — about 10 minutes, plus real event observation

- [ ] **B04:** Pick a genuinely live sports game. Compare phone, ESP and provider
      (a few-seconds update was reported after manual Lab recovery; H01 remains open)
      at a recorded time; observe a real score/status change without manually
      sending it. It must reach the display automatically. For MLB, confirm bases/
      outs appear only with valid live state and disappear for unavailable/final
      state. An unchanged/final game cannot prove live-update correctness.
- [x] **B05, selection:** Two leagues loaded; non-Primary matchup stayed selected
      for one minute; switching categories away/back intentionally selected Primary.
      Passed by user observation; one ESPN and one Sleeper league confirmed later.
- [x] **B05, Primary/current totals:** Intentional Primary switching and restoration
      selected the expected matchup; team names/current totals agreed for both leagues,
      by user observation.
- [ ] **B05, remaining:** Verify real changed scores render after commit and any
      overlay ends.
      Check no duplicate matchups/categories and no repeated unchanged slate transfers.
- [x] **B06, sample alert:** Sample +12-point alert appeared and cleared back to
      the selected matchup as expected, by user observation. The sample alert
      does not change real scores or prove provider/aggregation correctness.
- [ ] **B06, real events:**
      Observe real consecutive scoring events, totals, unique league counts and
      negative corrections when available. Different scoring deltas stay distinct;
      unchanged polling must not replay alerts. Mark unavailable cases NOT EXERCISED.
- [x] **B07, sample alert/standby:** Alert temporarily woke standby, restored
      standby afterward, and manual wake returned the normal matchup, by user observation.
- [ ] **B07, power variations:** Repeat button/navigation checks with USB monitor open,
      closed, and with USB unplugged while independently powered, if supported.

## 4. Offline and missing data — about 5–10 minutes

- [x] **B08:** Passed by user observation: cached content remained, unverified
      warning appeared, and automatic recovery worked without another BLE reconnect.
      Exact recovery time was not reported. With good cached content, disable Wi-Fi and cellular data **only**,
      leaving Bluetooth on. Disconnect/reconnect BLE. Wait for provider requests
      to settle; the cache remains navigable and phone reports incomplete/unverified
      updates. Updated firmware keeps cached categories unverified after reconnect.
      Restore internet without reconnecting BLE: WAKE/foreground recovery resumes.
      Record request latency rather than treating one slow request as a freeze.
- [ ] **B09, NOT EXERCISED:** User reported no third sport available for the
      proposed offline-enable/recovery test. Start with an enabled provider unavailable, then restore it. The
      missing category must appear without reconnecting; healthy peers must survive.
      For an initially genuinely empty category, observe it becoming populated:
      discovery runs every five minutes on refresh triggers. If no schedule changes
      during the bench session, leave this case pending. Known-empty schedules must
      remain distinct from errors; do not remove real account data to force a case.

## 5. Longer stability — 30 minutes, then 2 hours/overnight

- [ ] **B10, foreground return failed; soak deferred:** Home/return without force
      quit did not recover updates; BLE disconnected and diagnostics were empty.
      Keep this failure separate from the unexecuted extended soak. Once H01 is
      addressed, leave connected; lock/background/unlock the phone periodically.
      Foreground return must recover refresh subscription/work without duplicate
      operations. Repeat normal navigation/standby and one offline/online cycle.
      Record freezes, resets, stale displays and connection/refresh gaps with times.
- [ ] **B11:** Cross local midnight without reconnecting: consumer games advance
      on the next executing trigger; new PGA selection is discovered. An explicit
      historical Developer Tools date/tournament stays pinned for that connection.
      Repeat after changing time zone only if convenient; record phone local zone.
      Reconnect intentionally restores normal configured consumer startup.

Stop at the first reproducible failure, capture evidence before resetting, and add
it to the results log. Do not begin code fixes during this validation. Provider
week rollover, temporary missing player statistics, real corrections and differing
cross-league scoring require suitable live events or a later controlled fixture;
passing the sample alert does not qualify F09/F10/F11 end to end. This checklist
consolidates F03/F04/F06/F07/F08/F11/F18/F37 and F33/F34 validation notes; their
software-test passes are not physical-device sign-off.
