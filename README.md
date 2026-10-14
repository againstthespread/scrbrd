# SCRBRD Firmware

This repository contains the ESP32-S3 firmware for the SCRBRD physical
scoreboard. It receives normalized sports and fantasy content from the companion
Flutter app over BLE, validates and stages transfers, stores the active content
in RAM, and renders scoreboards, golf leaderboards, fantasy matchups, and
temporary fantasy scoring alerts.

The companion app is in
[scrbrd-mobile](https://github.com/againstthespread/scrbrd-mobile).

## Hardware

The current build targets the generic Arduino ESP32-S3 board
(`esp32:esp32:esp32s3`) and an SPI ST7789 display in 240 x 280 portrait mode.
`pin_config.h` is the source of truth for the verified display wiring:

| Signal | ESP32-S3 GPIO |
| --- | ---: |
| TFT MOSI | 39 |
| TFT SCK | 38 |
| TFT CS | 14 |
| TFT DC | 10 |
| TFT reset | 3 |
| TFT backlight | 13 |
| BOOT button | 0 |

The BOOT button uses the internal pull-up and is active low. USB serial logging
runs at 115200 baud. Do not assume another ESP32-S3 display module uses the same
pinout.

## BLE and display behavior

The firmware is a BLE peripheral advertising as `Peter Sports Hub`. The mobile
app writes short control messages or compact UTF-8 JSON to one characteristic;
the firmware sends a `WAKE` notification every 20 seconds while connected so the
app can refresh sports and fantasy data. Multi-packet sports, PGA, and fantasy
updates are staged and committed atomically. Cached content remains navigable
after disconnect and is marked `UPDATES PAUSED`.

See [SPORTS_HUB_BLE_PROTOCOL.md](SPORTS_HUB_BLE_PROTOCOL.md) for UUIDs, packet
schemas, size limits, transfer rules, and session lifecycle details.

The BOOT button and equivalent BLE navigation commands behave as follows:

- Single-click / `NEXT_GAME`: advance the current game, the next five-player PGA
  page, or the next fantasy matchup; wrap at the end.
- Double-click / `NEXT_LEAGUE`: advance to the next received category, reset its
  game/page/matchup position, and wrap at the end.
- Long-press (one second): toggle display standby by switching the backlight.
  Fantasy alerts temporarily wake a standby display and restore standby after
  the queued alerts finish.

Team-sport views show actual scores and live clock/status, plus MLB base/out and
NFL/NCAAF possession/down state when supplied. PGA is rendered in pages of five
golfers. Fantasy is one top-level category containing up to eight ordered
matchups; actual scores are primary and optional projections appear as secondary
`PROJ` lines (`PROJ --` when unavailable). Temporary scoring alerts overlay the
current screen for about seven seconds and use a 16-entry FIFO waiting queue
without replacing the persistent fantasy slate.

## Build and flash

The most recently verified local build used:

- ESP32 Arduino core 3.3.11
- ArduinoJson 7.4.3
- GFX Library for Arduino 1.6.7 (`Arduino_GFX_Library.h`)

Install Arduino CLI, then install the board core and libraries:

```sh
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli lib install "ArduinoJson@7.4.3" "GFX Library for Arduino@1.6.7"
```

From the repository root, compile and upload with:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3 .
arduino-cli board list
arduino-cli upload --fqbn esp32:esp32:esp32s3 -p <PORT> .
```

The same board target and libraries can be selected in Arduino IDE if preferred.

## Source and tests

- `scrbrd.ino` owns display rendering, protocol parsing, staged transfers,
  sync/display state, fantasy alerts, and BOOT-button handling.
- `BluetoothManager.h` / `BluetoothManager.cpp` own advertising, the writable
  characteristic, periodic wake notifications, and the incoming message queue.
- `GameManager.h` / `GameManager.cpp` own received-category storage and
  navigation for team sports, PGA, and fantasy slates.
- `game_data.h` defines display data structures and offline mock data;
  `fantasy_projection.h` validates and formats optional projected totals.
- `pin_config.h` defines the display dimensions, colors, and GPIO map.
- `test/game_manager_storage_test.cpp` covers native storage/navigation behavior;
  `test/fantasy_projection_test.cpp` covers optional projection parsing and
  formatting; `test/fantasy_alert_queue_test.py` exercises the real alert,
  standby, sync, and queue functions through a generated native harness.
- `test/scoreboard_render_test.py` exercises the real scoreboard renderer's
  situational-state availability and clock/team/score layout with drawing stubs;
  `test/test_sketch_test_runner.py` checks the alert runner from another directory.

Run the self-contained native tests from the repository root with a C++17
compiler on `PATH`:

```sh
c++ -std=c++17 -Itest/native -I. \
  test/game_manager_storage_test.cpp GameManager.cpp \
  -o /tmp/scrbrd-game-manager-test
/tmp/scrbrd-game-manager-test
python3 test/fantasy_alert_queue_test.py
python3 test/scoreboard_render_test.py
python3 test/test_sketch_test_runner.py
```

`test/fantasy_projection_test.cpp` additionally requires the ArduinoJson headers;
the full Arduino compile above validates its firmware integration.
