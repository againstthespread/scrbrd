# Sports Hub BLE Protocol

Protocol version: 1

Advertising name:
Peter Sports Hub

Service UUID:
<EXISTING ESP32 SERVICE UUID>

Writable characteristic UUID:
<EXISTING ESP32 CHARACTERISTIC UUID>

Temporary wake-notification characteristic UUID:
d8f6a9b2-7a5e-4e8c-9f2a-2b2f5b6c1001

Encoding:
UTF-8 JSON

Game packet:

{
  "version": 1,
  "type": "game",
  "league": "NFL",
  "away": "BUF",
  "home": "NE",
  "awayScore": 17,
  "homeScore": 24,
  "status": "LIVE",
  "clock": "Q4 8:31"
}

Canonical statuses:
UPCOMING
LIVE
FINAL
