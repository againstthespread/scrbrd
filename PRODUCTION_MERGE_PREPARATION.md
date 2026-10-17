# Production hardening merge preparation — October 17, 2026

**Prepared for approval. Neither `main` has been merged.** Lab 1009 is the
user-approved provisional development baseline. This preparation verifies the
accumulated histories and runs software checks; it does not repeat the production
audit or physical validation. No ESP or installed iPhone application was modified.

## Source and rollback references

| Repository | Original `main` / fetched `origin/main` | Validated source | Fix commits above main |
| --- | --- | --- | ---: |
| Firmware | `f5967a8993dcb03e1430002cbc3c3a357559cd39` | `ee5b0c6bba4915002f5f1a325c2b8bac4f449523` | 6 |
| Mobile | `7785c601fb733369b8dd3057722460de6e96d733` | `bcab9097ccc17459e6a8c5948aceb59a8b4437d4` | 16 |

Both repositories have annotated local tags:

- `rollback/main-before-production-hardening-2026-10-17` pins original main.
- `baseline/lab-1009-validated-2026-10-17` pins validated source.

The preparation branch in each repository is
`codex/merge-prep-production-2026-10-17`. It contains every commit from the
validated source history plus documentation/provenance only. Earlier production
fix branch tips are ancestors; none were cherry-picked, squashed or omitted.
The isolated Lab 1008 H01 commit is patch-equivalent to the included H01 fix.
The separate `backup/fantasy-play-correlation` feature history is retained on its
existing local/remote references and is outside this production-fix merge.
Full exact fix commit inventories accompany this report.

Both original main worktrees are clean. Main is an ancestor of the validated source
in each repository, so the intended merges are fast-forwards with no content
conflict resolution. Abort if a main revision or working tree changes before merge.

## Verification results

| Check | Result |
| --- | --- |
| Full Flutter suite | **952 passed**; no skipped tests reported |
| Flutter static analysis | **No issues found** |
| Locked package setup | Offline `pub get --enforce-lockfile` passed; no package update |
| Firmware native suites | **All 10 entrypoints passed**: storage, existing projection regression, alert queue, rendering, sketch runner, BLE queue, fantasy transfer, responsiveness, cache verification, mobile protocol fixture |
| Cross-repository compatibility | **18 scenarios / 112 mobile-generated packets passed** against real firmware parsing/storage/lifecycle with native hardware stubs; UUIDs and advertised name match |
| BLE queue ThreadSanitizer | **Passed**, including 70,000 concurrent packets and 200 full bursts |
| Responsiveness ASan/UBSan | **All 15 scenarios passed** |
| Firmware ESP32-S3 Release | **Passed**; 780,585 flash bytes / 139,912 static RAM bytes |
| Firmware timing variant | **Passed**, `SCRBRD_TIMING=1`; 785,217 flash / 140,608 static RAM bytes |
| iOS Release | **Passed**, generic iphoneos, unsigned; production bundle ID and build 6 retained |
| Dependency/source integrity | Committed Dart/Swift lockfiles and all runtime source remain unchanged during preparation |
| Whitespace and merge ancestry | Passed; final prepared tips must remain fast-forward descendants |

Toolchain: Flutter 3.47.0 / Dart 3.13.0; Xcode 27.0 (27A266a), iPhoneOS 27 SDK;
ESP32 core 3.3.11, ArduinoJson 7.4.3, Arduino GFX 1.6.7; Apple clang 21.0.0.
iOS configuration was generated with Flutter `--config-only`, then compiled using
`xcodebuild` with `-disableAutomaticPackageResolution` and
`-onlyUsePackageVersionsFromResolvedFile`. The completed build has dependency
warnings (Firebase/reactive BLE/Flutter headers; 75 warning lines including
repeated diagnostics), plus skipped AppIntents metadata extraction. No build
errors occurred. Swift 6 language-mode compatibility is not claimed or changed.
The native RunnerTests file is only an empty template; Flutter and firmware
native entrypoints constitute the substantive available software suites.

## Preservation and layout

Original experimental worktrees were snapshotted before preparation: porcelain
status, staged/unstaged binary diffs, tar archives and SHA256 lists. Their original
statuses, diffs and untracked-file hashes were rechecked unchanged. Git bundles
preserve rollback/validated history; prepared-history bundles are also retained.
Logs, manifests, archives and bundles are in:

`/Users/peterdeluca/Projects/_worktrees/scrbrd-codex/build/merge-preparation-2026-10-17`

The firmware experimental worktree has a pre-existing move into nested `scrbrd/`.
All 29 tracked files match the validated commit at their original/relocated paths.
**That uncommitted layout is preserved separately and is excluded from this merge.**
Prepared firmware keeps the committed root sketch layout. The Arduino build uses a
byte-identical source copy in a folder named `scrbrd`, as required for `scrbrd.ino`.
The relocated copy, unrelated `.DS_Store`, original validation docs and ignored
build artifacts remain in place; nothing was reset, cleaned, overwritten or staged
in the original worktree. Hardware documents and the original production audit
are copied unchanged into the preparation branch for durable documentation.

The mobile experimental worktree's only untracked file was the integrated hardware
validation report; its identical copy is included. The separate Lab 1008 worktree
retains three Lab-only configuration edits, additionally archived with a diff and
hash list. The legacy firmware worktree `SportsHub-fantasy-slate` has a stale `.git`
pointer to its former parent repository name. Its 12 tracked files match branch
`fantasy-multi-league-browsing` and are additionally archived. Its pointer/build
artifacts were left untouched; no worktree repair or prune was performed. This
administrative issue does not affect either clean main worktree or prepared merge.

## Lab 1009 provenance and remaining limitations

[LAB_1009_BASELINE.md](LAB_1009_BASELINE.md) records the exact validated source and
local artifact directory. The signed Lab 1.0.0 (1009) app, Lab-only configuration,
source archive, build/signing logs and Lab 1008 rollback remain preserved locally.
All 90 Lab 1009 artifact hashes and all 290 source archive files were reverified.
Mobile `validation/lab-1009/` commits portable source/build metadata, checksums and
sampled stability evidence. The production configuration remains 1.0.0+6 with the
original production bundle ID; the merge does not recreate or reinstall Lab 1009.
Historical validation reports are retained verbatim; the integrated Lab 1009 report
and this report provide the newer status where earlier reports say checks pending.

No new regression was found in the executed software checks. The recorded Sleeper
projection discrepancy **F28 remains deferred pending commercial data licensing**.
No projection calculation was investigated or changed here. Previously committed
validated changes are retained intact; existing regression tests ran unchanged.
Principal hardware checks passed, including qualitative live NCAAF responsiveness
and sampled stability checkpoints 30 minutes apart. Exact NBA/MLB live latency,
active-transfer disconnect with genuine queue contention, silent-WAKE fault,
real scoring-alert failure/retry, overnight/power variants and long-term resource
behavior are not established. BLE writes are not application acknowledgments.
See the mobile `INTEGRATED_HARDWARE_VALIDATION.md` for precise physical coverage.
This establishes a development baseline, not unrestricted production qualification.

## Approval and exact merge procedure

Review this branch and the persisted `merge-actions.json` for the final full commit
IDs. On explicit user approval only, first check each original main worktree is
clean and still at the original revision above; recheck remote main has not moved.
Then run `git merge --ff-only <prepared-full-commit-id>` in:

- Firmware: `/Users/peterdeluca/Projects/scrbrd`
- Mobile: `/Users/peterdeluca/Projects/scrbrd-mobile`

These are two independent local fast-forwards. Stop on any unexpected status,
changed base or merge refusal. Do not auto-resolve or force anything. No push,
deployment, app installation or firmware upload is included in these actions.
