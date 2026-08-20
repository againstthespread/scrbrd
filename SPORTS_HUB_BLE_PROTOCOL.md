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

Game slate packet:

{
  "version": 1,
  "type": "slate",
  "league": "MLB",
  "games": [
    {"id":"101","away":"NYY","home":"BOS","awayScore":4,"homeScore":3,"status":"LIVE","clock":"BOT 7"},
    {"id":"102","away":"LAD","home":"SF","awayScore":2,"homeScore":2,"status":"FINAL","clock":"FINAL"}
  ]
}

Slate rules:
- Legacy one-packet slates contain 1 through 4 games from one league.
- Maximum complete compact UTF-8 packet size: 512 bytes.
- Game fields use the same limits and canonical statuses as a game packet.
- Optional stable `id`: non-empty, maximum 48 UTF-8 bytes.
- The whole slate is validated before replacing the active received slate.
- Version 1 `game` packets and legacy commands remain unchanged.

Preferred chunked slate transfer:

{"version":1,"type":"slate_start","league":"MLB","slateId":"transfer-123","totalGames":15,"totalChunks":4}
{"version":1,"type":"slate_chunk","slateId":"transfer-123","chunkIndex":0,"games":[{"id":"101","away":"NYY","home":"BOS","awayScore":4,"homeScore":3,"status":"LIVE","clock":"BOT 7"}]}
{"version":1,"type":"slate_end","slateId":"transfer-123"}

Chunked-transfer rules:
- A logical slate contains 1 through 20 games from one league.
- Every packet remains at or below 512 compact UTF-8 JSON bytes.
- `slateId` is required, non-empty, and at most 48 UTF-8 bytes.
- Chunks use zero-based indexes and arrive once each in ascending order.
- Chunk games follow the same validation rules and optional event `id` metadata as legacy slate games.
- Only a matching `slate_end` with all declared chunks and games atomically replaces the active slate.
- Invalid or incomplete transfers leave the active slate unchanged.
- In-progress staging expires after 30 seconds; a new valid `slate_start` resets staging.

Received-league storage and navigation:
- The `league` in `slate_start` is the stored slate identity; `slateId` identifies only its transfer.
- A successful `slate_end` replaces only the matching league or appends it if it is new.
- Up to 8 received leagues are stored independently, with up to 20 games each.
- Failed or incomplete transfers never alter any stored league.
- Received leagues retain insertion order.
- BOOT single-click and `NEXT_GAME` cycle games in the active received league.
- BOOT double-click and `NEXT_LEAGUE` cycle received leagues, wrap, and reset the game index to 0.
- Mock leagues remain the fallback only when there are no received leagues.

PGA golf leaderboard transfer:

{"version":1,"type":"golf_start","league":"PGA","transferId":"golf-123","tournamentId":"9001","tournamentName":"BMW Championship","totalGolfers":20,"totalChunks":5}
{"version":1,"type":"golf_chunk","transferId":"golf-123","chunkIndex":0,"golfers":[{"id":"400001","name":"Scottie Scheffler","rank":"1","score":"-8","detail":"F"}]}
{"version":1,"type":"golf_end","transferId":"golf-123"}

Golf rules:
- PGA is stored as dedicated leaderboard content, not team-sport games.
- Transfers contain 1 through 50 golfers; each packet remains at or below 512 UTF-8 bytes.
- Tournament and player identifiers are metadata owned by firmware buffers.
- Golfer rows contain `id`, `name`, official `rank`, normalized `score`, and optional `detail`.
- Only a complete matching `golf_end` atomically replaces PGA. Team leagues are unaffected.
- Single-click and `NEXT_GAME` advance five golfers per page and wrap. League navigation resets the page to 1.
