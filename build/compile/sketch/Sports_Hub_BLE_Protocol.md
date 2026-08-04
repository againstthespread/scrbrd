#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\Sports_Hub_BLE_Protocol.md"
# Sports Hub BLE Protocol

Protocol version: 1

Advertising name:
Peter Sports Hub

Service UUID:
<EXISTING ESP32 SERVICE UUID>

Writable characteristic UUID:
<EXISTING ESP32 CHARACTERISTIC UUID>

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