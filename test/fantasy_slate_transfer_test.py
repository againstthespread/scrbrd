#!/usr/bin/env python3
"""Run actual fantasy packet/commit/lifecycle code with native hardware stubs.

Requires c++ and ArduinoJson headers (ARDUINOJSON_INCLUDE may override the path).
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'scrbrd.ino').read_text()


def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


prefix = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "GameManager.h"
#include "fantasy_projection.h"
using String = std::string;
using std::isfinite;
struct Serial { template<class T> void print(T) {} template<class T> void println(T) {} } USBSerial;
unsigned long now = 100;
unsigned long millis() { return static_cast<uint32_t>(now); }
const int LCD_BL = 1, HIGH = 1, LOW = 0;
void digitalWrite(int, int) {}
struct Bluetooth {
  bool connected = true, ended = false;
  std::vector<String> messages;
  void updateBluetooth() {}
  bool isBluetoothConnected() { return connected; }
  bool takeDisconnectEvent() { bool result = ended; ended = false; return result; }
  bool hasReceivedMessage() { return !messages.empty(); }
  String getReceivedMessage() { auto result = messages.front(); messages.erase(messages.begin()); return result; }
} bluetoothManager;
int draws = 0;
float displayedScore = -1;
void drawCurrentScreen();
void drawFantasyAlert() {}
void drawDashboard();
void drawUpdatesPausedWarning() {}
void drawEmptyTodayScreen() { ++draws; }
void drawConnectedLoadingScreen() { ++draws; }
void drawConnectScreen() { ++draws; }
bool slateTransferActive = false, golfTransferActive = false;
void clearStagingSlate(const char *) {}
void clearGolfStaging(const char *) {}
void expireStagingSlateIfNeeded() {}
void expireGolfStagingIfNeeded() {}
bool handleGamePacket(const String &) { return false; }
bool handleFantasyAlertPacket(const String &) { return false; }
bool handleSlatePacket(const String &) { return false; }
bool handleSlateStartPacket(const String &) { return false; }
bool handleSlateChunkPacket(const String &) { return false; }
bool handleSlateEndPacket(const String &) { return false; }
bool handleGolfStartPacket(const String &) { return false; }
bool handleGolfChunkPacket(const String &) { return false; }
bool handleGolfEndPacket(const String &) { return false; }
'''
enums = source[source.index('enum SyncLifecycleState'):source.index('const uint8_t BOOT_BUTTON_PIN')]
data = source[source.index('const unsigned long FANTASY_ALERT_DURATION_MS'):source.index('BluetoothManager bluetoothManager;')]
globals_ = source[source.index('bool deviceStandby'):source.index('// Chunked slate staging')]
signatures = [
    'void setDisplayBacklight(bool enabled)', 'void restoreStandbyAfterFantasyAlert()',
    'DeviceDisplayState currentDisplayState()', 'const char *displayStateDiagnostic(',
    'void drawCurrentScreen()', 'void startFantasyAlert(', 'void finishFantasyAlert()',
    'void expireFantasyAlertIfNeeded()', 'void markRealContentActivated()',
    'bool isValidUtf8(', 'void rejectGamePacket(', 'bool readRequiredText(',
    'bool readSlateTransferDocument(', 'bool handleFantasyMatchupPacket(',
    'bool handleFantasyClearPacket(',
]
for optional in ['void expireFantasySlateIfNeeded()', 'bool handleFantasySlateControlPacket(']:
    if optional in source:
        signatures.append(optional)
signatures += ['void handleBluetoothCommand(', 'void handleBluetoothMessages()']
functions = '\n'.join(function(sig) for sig in signatures)

tests = r'''
void drawDashboard() {
  ++draws;
  const auto *matchup = gameManager.getCurrentFantasyMatchup();
  if (matchup) displayedScore = matchup->userScore;
}
const String start = R"({"version":1,"type":"fantasy_slate_start"})";
const String end = R"({"version":1,"type":"fantasy_slate_end"})";
String entry(const char *id, float score = 10) {
  JsonDocument doc;
  doc["version"] = 1; doc["type"] = "fantasy_matchup";
  if (id) doc["identity"] = id;
  doc["leagueName"] = "League"; doc["userName"] = "User";
  doc["opponentName"] = "Opponent"; doc["userScore"] = score;
  doc["opponentScore"] = 5; doc["week"] = 1; doc["status"] = "LIVE";
  String result; serializeJson(doc, result); return result;
}
void command(const String &packet) { handleBluetoothCommand(packet); }
void slate(std::initializer_list<const char *> ids) {
  command(start); for (auto id : ids) command(entry(id)); command(end);
}
void expectSelected(const char *id, float score = 10) {
  const auto *m = gameManager.getCurrentFantasyMatchup();
  assert(m && std::strcmp(m->identity, id) == 0 && m->userScore == score);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const String scenario = argv[1];
  lastDisplayedBluetoothConnected = true;
  if (scenario == "render") {
    slate({"A"}); drawCurrentScreen(); draws = 0;
    command(start); command(entry("A", 20));
    assert(draws == 0 && displayedScore == 10);
    command(end); assert(draws == 1 && displayedScore == 20);
  } else if (scenario == "selection") {
    slate({"A", "B", "C"}); gameManager.nextGame(); expectSelected("B");
    for (int i = 0; i < 3; ++i) {
      command(start); command(entry("A")); command(entry("C"));
      command(entry("B", 20)); command(end); expectSelected("B", 20);
    }
    slate({"A", "C"}); expectSelected("A");
    gameManager.nextGame(); slate({"C", "A"}); expectSelected("C");
    GameData game = {}; assert(gameManager.setReceivedSlate("NFL", &game, 1));
    gameManager.nextGame(); expectSelected("A");
    gameManager.nextLeague(); gameManager.nextLeague(); expectSelected("C");
  } else if (scenario == "invalid") {
    const std::vector<String> invalid = {
      entry(""), entry(nullptr), R"({"version":1,"type":"fantasy_matchup","identity":"B"})",
      R"({"version":999,"type":"fantasy_matchup"})", "{broken",
      entry("B") + String(513, ' '), entry("B") + String(1, char(0xFF)),
      R"({"version":999,"type":"fantasy_slate_end"})",
      R"({"type":"fantasy_slate_end"})",
      R"({"version":999,"type":"fantasy_clear"})",
      end + String(513, ' '), end + String(1, char(0xFF)),
    };
    for (const auto &bad : invalid) {
      slate({"A"}); command(start); command(entry("B", 30));
      command(bad); command(entry("C", 40)); command(end);
      expectSelected("A");
    }
    for (const auto &bad : std::vector<String>{
      R"({"version":999,"type":"fantasy_slate_start"})",
      R"({"type":"fantasy_slate_start"})", start + String(513, ' '),
      start + String(1, char(0xFF))}) {
      slate({"A"}); command(bad); command(end); expectSelected("A");
    }
    slate({"A"}); command(start);
    for (int i = 0; i < 9; ++i) command(entry(std::to_string(i).c_str()));
    command(end); expectSelected("A");
    command(start); command(entry("B", 20)); command(entry("B", 30));
    command(end); expectSelected("B", 30); assert(gameManager.getCurrentGameCount() == 1);
  } else if (scenario == "timeout") {
    slate({"A"}); command(start); command(entry("B", 20));
    now += 30000; handleBluetoothMessages();
    assert(!gameManager.isFantasySlateTransferActive());
    command(entry("C", 30)); command(end); expectSelected("A");
    slate({"B"}); expectSelected("B");
    // Accepted activity extends inactivity deadline; arithmetic wraps safely.
    now = 0xFFFFFF00UL; command(start); now += 29999;
    command(entry("C")); handleBluetoothMessages();
    assert(gameManager.isFantasySlateTransferActive()); command(end); expectSelected("C");
  } else if (scenario == "disconnect") {
    slate({"A"}); command(start); command(entry("B", 20));
    bluetoothManager.connected = false; handleBluetoothMessages();
    assert(!gameManager.isFantasySlateTransferActive()); expectSelected("A");
    bluetoothManager.connected = true; handleBluetoothMessages();
    command(entry("C", 30)); command(end); expectSelected("A");
    slate({"B"}); expectSelected("B");
  } else if (scenario == "rapid_disconnect") {
    slate({"A"}); command(start); command(entry("B", 20));
    authoritativeSyncSessionActive = true;
    bluetoothManager.ended = true; handleBluetoothMessages();
    assert(!gameManager.isFantasySlateTransferActive()); command(end); expectSelected("A");
    assert(!authoritativeSyncSessionActive);
    command("SYNC_START"); assert(!gameManager.hasReceivedContent());
  } else if (scenario == "reset") {
    for (auto reset : {String("SYNC_START"), String(R"({"version":1,"type":"fantasy_clear"})")}) {
      authoritativeSyncSessionActive = false;
      slate({"A"}); command(start); command(entry("B", 20)); command(reset);
      assert(!gameManager.isFantasySlateTransferActive());
      command(end); assert(!gameManager.hasReceivedContent());
      assert(!gameManager.commitFantasySlate());
    }
    slate({"A"}); command(start); command(entry("B"));
    gameManager.clearReceivedContent(); assert(!gameManager.isFantasySlateTransferActive());
    command(end); assert(!gameManager.hasReceivedContent());
  } else if (scenario == "compatibility") {
    command(entry(nullptr, 15)); expectSelected("legacy", 15);
    command(start); command(entry("B"));
    command(R"({"version":1,"type":"game","league":"NFL"})");
    command(R"({"version":1,"type":"fantasy_alert"})");
    command("NEXT_GAME");
    assert(gameManager.isFantasySlateTransferActive());
    command(end); expectSelected("B");
    command(start); command(entry("B")); command(start);
    command(entry("C", 25)); command(end); expectSelected("C", 25);
    command(start); command(end); assert(!gameManager.hasReceivedContent());
    command(entry(nullptr, 30)); expectSelected("legacy", 30);
    slate({"A", "B"}); command(entry(nullptr, 50)); expectSelected("A");
    // Committed data updates while overlays/standby defer rendering.
    fantasyAlertActive = true; draws = 0;
    command(start); command(entry("A", 40)); command(entry("B")); command(end);
    expectSelected("A", 40); assert(draws == 0);
    fantasyAlertActive = false; drawCurrentScreen(); assert(displayedScore == 40);
    deviceStandby = true; draws = 0; slate({"A", "B"}); assert(draws == 0);
    deviceStandby = false; drawCurrentScreen(); assert(displayedScore == 10);
  } else { assert(false); }
  std::puts(argv[1]);
}
'''
include = os.environ.get('ARDUINOJSON_INCLUDE', str(Path.home() / 'Documents/Arduino/libraries/ArduinoJson/src'))
scenarios = ['render', 'selection', 'invalid', 'timeout', 'disconnect', 'rapid_disconnect', 'reset', 'compatibility']
with tempfile.TemporaryDirectory(prefix='fantasy-slate-native-') as directory:
    cpp = Path(directory) / 'test.cpp'
    cpp.write_text(prefix + enums + data + globals_ + functions + tests)
    executable = Path(directory) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-missing-field-initializers', '-I' + str(root / 'test/native'),
                    '-I' + str(root), '-I' + include, str(cpp),
                    str(root / 'GameManager.cpp'), '-o', str(executable)], check=True)
    failed = [scenario for scenario in scenarios
              if subprocess.run([str(executable), scenario]).returncode != 0]
    if failed:
        raise SystemExit('Failed fantasy slate scenarios: ' + ', '.join(failed))
print('Fantasy slate parser, commit/render, selection, cancellation, and compatibility passed.')
