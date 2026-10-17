# Lab 1008 H01 physical validation results

Observed H01 checks passed. The original silent-WAKE failure did not recur. This does not physically verify recovery from an actual silent stream; that condition never occurred during this session.

Candidate `8d9cbba06110b7bcd013dde241f7058e2c6681cd`; Lab build1008; ESP #2 expected firmware `ee5b0c6`, unchanged. Lab was updated in place with explicit approval; original SCRBRD build6 retained. User confirmed enabled categories, ordering and fantasy configurations retained. No code fixes, firmware changes, diagnostic clears or further app installations during testing.

| Check | Result and evidence |
| --- | --- |
| Online baseline | Pass: five raw accepted WAKE callbacks about20seconds apart; completed refreshes and changedMLB transfer |
| Background/foreground returns | Pass for listener preservation and resumed periodic receipt; actual timing varied from prescribed sequence and is recorded in individual analyses |
| Three online reconnects | Pass: native cleanup/enable and fresh receipt confirmed; cycle2 captured99.94seconds uninterrupted; cycle3 included additional returns |
| Offline reconnect | Pass for BLE receipt, terminated failed syncs, cached-catalog retention without SYNC_START, and released gates; visual cache/warning report not supplied |
| Restore internet without BLE reconnect | Pass: sameG9/epoch23; APP RESUME sync3.290seconds,4categories,0failures; eight periodic refreshes and two changedMLB transfers afterward |
| Thirty-minute stability | User reports continued normal operation. Final stability capture34m39s after prior capture, sameG9/epoch23,14raw WAKE callbacks at19.979851–20.010759second intervals, completed operations/gate releases, two MLB transfers and successful reconciliation |
| Fetch-failure follow-up | Four failed cycles recovered automatically on BLE WAKE, without reconnect or foreground-return trigger; eight subsequent BLE cycles successfully fetched all3sports. User reports interneton/no freezing |
| Actual75-second silent-stream recovery | Not physically exercised. Software coverage is recorded in H01_FIX_VALIDATION.md |

## Transient fetch anomaly

Requests188–191 (sports WAKE168–171) failed NFL/NBA/MLB fetches from19:22:10 through19:23:10UTC. Raw BLE WAKE receipt continued. Each operation retained baselines and completed/released its gate. Cause is unknown; the user reports internet remainedon, and this capture does not include error details.

Request192, triggered by BLE WAKE at19:23:30.928518UTC, successfully fetched all3sports by19:23:31.164227UTC. Requests192–196 succeeded before the next foreground return at19:24:55.897966UTC. That return preserved the receiving listener; request197 (APP RESUME), then BLE requests198–200 also succeeded. The symptom resolved automatically. This was not an H01 silent-listener recurrence and required no code change.

## Evidence and limits

Diagnostic session `2026-10-15T18:20:26.177783Z`, transportT1. Final export ends19:25:51.499827UTC (15:25:51EDT), sequence8148, clears0, sameG9/epoch23 as the offline reconnect at18:41:51UTC. This is40m35s after the internet-recovery resume. Accepted raw notifications and completed operations are directly present in retained traces; missing events between exports cannot be reconstructed from counters alone.

The512-event ring evicts older records normally. The final stability tail covers about4m40s; uninterrupted full-hold continuity is user-reported. Exact teams/score/reference values, measured display latency and explicit warning-text observations were not supplied. No physical silence deadline/retry-exhaustion claim is made. Healthy-return preservation and changed-data transfer succeeded; this narrows risk but does not prove the hypothesized native race was the original hardware cause.

Full pasted exports remain in the conversation. Files named `H01-1008-*-analysis.json` are evidence summaries, not verbatim raw exports. `H01-1008-hardware-session.json` records every stage and limitations. No H01 failures were observed; the recovered fetch anomaly remains documented with unknown cause.
