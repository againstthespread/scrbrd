#!/usr/bin/env python3
"""Actual sketch input/protocol/loop code with an explicitly simulated clock.

USB host backpressure: 2s per write until setup configures nonblocking output.
LCD frame: 40ms; RX dequeue/parse workload: 2ms/packet. These are assumptions,
not physical-device timing measurements. Uses the installed ArduinoJson headers.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'scrbrd.ino').read_text()
def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]

prefix = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>
#include "GameManager.h"
#include "fantasy_projection.h"
using String = std::string;
using std::isfinite;
uint64_t clockUs = 0;
unsigned long millis() { return static_cast<uint32_t>(clockUs / 1000); }
unsigned long micros() { return static_cast<uint32_t>(clockUs); }
void delay(unsigned long ms) { clockUs += ms * 1000; }
struct Serial {
  bool stalled = false;
  unsigned timeout = 100;
  void begin(int) {}
  void setTxTimeoutMs(unsigned ms) { timeout = ms; }
  void writeDelay() { if (stalled && timeout) clockUs += 20 * timeout * 1000; }
  template<class T> void print(T) { writeDelay(); }
  template<class T> void println(T) { writeDelay(); }
  operator bool() const { return false; }
  int availableForWrite() { return 0; }
  size_t write(const uint8_t *, size_t n) { return n; }
} USBSerial;
#define ARDUINO_ISR_ATTR
#define CHANGE 3
void attachInterrupt(int, void (*)(), int) {}
int digitalPinToInterrupt(int pin) { return pin; }
const int HIGH = 1, LOW = 0, LCD_BL = 1, INPUT_PULLUP = 2;
std::vector<std::pair<uint64_t, uint64_t>> buttonPresses;
std::vector<std::pair<uint64_t, bool>> backlights;
int digitalRead(int);
void digitalWrite(int, int value) { backlights.emplace_back(clockUs, value); }
void pinMode(int, int) {}
void setCpuFrequencyMhz(int) {}
int getCpuFrequencyMhz() { return 160; }
void initializeDisplay() {}
struct BluetoothManager {
  bool connected = false, ended = false, lost = false, continuous = false;
  unsigned serviced = 0;
  std::deque<String> messages;
  String traffic;
  void beginBluetooth() {}
  void updateBluetooth() {}
  bool isBluetoothConnected() { return connected; }
  bool takeDisconnectEvent() { bool r = ended; ended = false; return r; }
  bool takeReceiveLossEvent() { bool r = lost; lost = false; return r; }
  bool hasReceivedMessage() { return !messages.empty() || continuous; }
  String getReceivedMessage() {
    clockUs += 2000; ++serviced;
    if (messages.empty()) return traffic;
    auto value = messages.front(); messages.pop_front(); return value;
  }
};
struct Frame { uint64_t at; String league, id; unsigned score; };
std::vector<Frame> frames;
void drawDashboard();
void drawUpdatesPausedWarning() {}
void drawEmptyTodayScreen() { clockUs += 40000; }
void drawConnectedLoadingScreen() { clockUs += 40000; }
void drawConnectScreen() { clockUs += 40000; }
void drawFantasyAlert() { clockUs += 40000; }
'''
globals_ = source[source.index('enum SyncLifecycleState'):source.index('void setDisplayBacklight')]
standby = source[source.index('void setDisplayBacklight'):source.index('uint16_t measuredTextWidth')]
handlers = source[source.index('DeviceDisplayState currentDisplayState()'):source.index('void setup()')]
functions = standby + handlers + function('void setup()') + '\n' + function('void loop()')
# Arduino automatically emits declarations. Match that for the native harness.
prototypes = ';\n'.join(re.findall(r'^((?:void|bool|DeviceDisplayState|const char \*)\s+\w+\([^;{}]*?\))\n\{', functions, re.M)) + ';'
if '#include "responsiveness_timing.h"' in source:
    prefix += '\n#include "responsiveness_timing.h"\n'

tests = r'''
int digitalRead(int) {
  int reading = HIGH;
  uint64_t latestTransition = 0;
  for (const auto &p : buttonPresses) {
    if (clockUs >= p.first && clockUs < p.second) reading = LOW;
    if (clockUs >= p.first) latestTransition = std::max(latestTransition, p.first);
    if (clockUs >= p.second) latestTransition = std::max(latestTransition, p.second);
  }
#if SCRBRD_TIMING
  ResponsivenessTiming::physicalEdgeAt.store(static_cast<uint32_t>(latestTransition));
#else
  (void)latestTransition;
#endif
  return reading;
}
void drawDashboard() {
  clockUs += 40000;
  const auto &g = gameManager.getCurrentGame();
  frames.push_back({clockUs, gameManager.getCurrentLeagueName(), g.eventId, g.awayScore});
}
String game(const char *league, const char *id, unsigned score = 1) {
  JsonDocument doc;
  doc["version"] = 1; doc["type"] = "game"; doc["league"] = league;
  doc["id"] = id; doc["away"] = "Away"; doc["home"] = "Home";
  doc["awayScore"] = score; doc["homeScore"] = 2;
  doc["status"] = "LIVE"; doc["clock"] = "Q1";
  String out; serializeJson(doc, out); return out;
}
uint64_t maxPollGap = 0;
void runUntil(uint64_t end, const std::vector<std::pair<uint64_t, uint64_t>> &presses) {
  buttonPresses = presses;
  while (clockUs < end) {
    const auto start = clockUs;
    loop();
    maxPollGap = std::max(maxPollGap, clockUs - start);
  }
}
void seed() {
  GameData games[2] = {};
  strlcpy(games[0].eventId, "nba-a", sizeof(games[0].eventId));
  strlcpy(games[1].eventId, "nba-b", sizeof(games[1].eventId));
  assert(gameManager.setReceivedSlate("NBA", games, 2));
  assert(gameManager.setReceivedSlate("MLB", games, 2));
  lastLoggedDisplayState = DISPLAY_LIVE_CONTENT;
  frames.clear(); backlights.clear();
}
void printMetrics(const char *scenario) {
  std::printf("%s: frame_count=%zu max_simulated_loop_gap_ms=%.1f last_frame_ms=%.1f serviced=%u standby=%d\n",
    scenario, frames.size(), maxPollGap / 1000.0,
    frames.empty() ? 0 : frames.back().at / 1000.0, bluetoothManager.serviced, deviceStandby);
  const auto selected = std::find_if(frames.begin(), frames.end(), [](const Frame &f) { return f.id == "nba-b"; });
  if (selected != frames.end()) std::printf("  first selected frame ms=%.1f\n", selected->at / 1000.0);
  if (!backlights.empty()) std::printf("  first standby/wake transition ms=%.1f\n", backlights.front().first / 1000.0);
#if SCRBRD_TIMING
  char summary[256]; ResponsivenessTiming::summary(summary, sizeof(summary));
  std::fputs(summary, stdout);
  assert(ResponsivenessTiming::maxima[ResponsivenessTiming::PollGap] <= 60000);
#endif
}
int main(int argc, char **argv) {
  std::setbuf(stdout, nullptr);
  assert(argc == 2); const String scenario = argv[1];
  setup(); clockUs = 0; seed();
  if (scenario == "no_ble" || scenario == "no_ble_healthy" || scenario == "single_traffic") {
    USBSerial.stalled = scenario != "no_ble_healthy";
    bluetoothManager.connected = scenario == "single_traffic";
    lastDisplayedBluetoothConnected = bluetoothManager.connected;
    lastLoggedDisplayState = bluetoothManager.connected ? DISPLAY_LIVE_CONTENT : DISPLAY_LIVE_CONTENT_PAUSED;
    // Sustained staged NBA traffic, not a one-game packet replacing selection.
    bluetoothManager.continuous = scenario == "single_traffic";
    bluetoothManager.traffic = R"({"version":1,"type":"slate_start","league":"NBA","slateId":"s","totalGames":2,"totalChunks":1})";
    runUntil(1500000, {{100000, 260000}}); printMetrics(argv[1]);
    assert(String(gameManager.getCurrentGame().eventId) == "nba-b");
    assert(!frames.empty() && frames.back().id == "nba-b");
    // Physical release + 50ms debounce + 300ms window + bounded scheduling/render.
    assert(frames.back().at <= 700000 && maxPollGap <= 60000);
    if (bluetoothManager.continuous) assert(bluetoothManager.serviced > 20);
  } else if (scenario == "bounce_traffic" || scenario == "triple_traffic") {
    USBSerial.stalled = true; bluetoothManager.connected = true;
    lastDisplayedBluetoothConnected = true; bluetoothManager.continuous = true;
    bluetoothManager.traffic = R"({"version":1,"type":"slate_start","league":"NBA","slateId":"s","totalGames":2,"totalChunks":1})";
    if (scenario == "bounce_traffic") {
      runUntil(1500000, {{100000, 110000}, {124000, 134000}, {148000, 160000}, {180000, 330000}});
      assert(String(gameManager.getCurrentLeagueName()) == "NBA");
      assert(String(gameManager.getCurrentGame().eventId) == "nba-b");
      assert(frames.size() == 1);
    } else {
      runUntil(1700000, {{100000, 230000}, {330000, 460000}, {600000, 740000}});
      assert(String(gameManager.getCurrentLeagueName()) == "MLB");
      assert(String(gameManager.getCurrentGame().eventId) == "nba-b");
      assert(frames.size() == 2 && frames[0].id == "nba-a");
    }
    assert(!deviceStandby && maxPollGap <= 60000);
  } else if (scenario == "double_traffic") {
    USBSerial.stalled = true; bluetoothManager.connected = true;
    lastDisplayedBluetoothConnected = true;
    bluetoothManager.continuous = true;
    bluetoothManager.traffic = R"({"version":1,"type":"slate_start","league":"NBA","slateId":"s","totalGames":2,"totalChunks":1})";
    runUntil(1500000, {{100000, 230000}, {330000, 460000}}); printMetrics(argv[1]);
    assert(String(gameManager.getCurrentLeagueName()) == "MLB");
    assert(!frames.empty() && frames.back().league == "MLB" && frames.back().at <= 580000);
    assert(maxPollGap <= 60000);
  } else if (scenario == "hold_traffic") {
    USBSerial.stalled = true; bluetoothManager.connected = true;
    lastDisplayedBluetoothConnected = true;
    bluetoothManager.continuous = true; bluetoothManager.traffic = game("MLB", "mlb");
    runUntil(1450000, {{100000, 1400000}}); printMetrics(argv[1]);
    assert(deviceStandby && !backlights.empty() && !backlights.back().second);
    assert(backlights.back().first <= 1200000);
    runUntil(3000000, {{1600000, 2900000}});
    assert(!deviceStandby && backlights.back().second);
    assert(maxPollGap <= 60000);
  } else if (scenario == "sync_selection") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    gameManager.nextLeague(); gameManager.nextGame();
    handleBluetoothCommand(game("NBA", "updated", 42));
    assert(String(gameManager.getCurrentLeagueName()) == "MLB");
    GameData refreshed[2] = {};
    strlcpy(refreshed[0].eventId, "nba-b", sizeof(refreshed[0].eventId));
    strlcpy(refreshed[1].eventId, "nba-a", sizeof(refreshed[1].eventId));
    assert(gameManager.setReceivedSlate("MLB", refreshed, 2));
    assert(String(gameManager.getCurrentGame().eventId) == "nba-b");
    handleBluetoothCommand("SYNC_START"); assert(!gameManager.hasReceivedContent());
    handleBluetoothCommand(game("NBA", "new-nba"));
    handleBluetoothCommand(game("MLB", "new-mlb"));
    gameManager.nextLeague();
    handleBluetoothCommand("SYNC_START"); // Duplicate during active initial sync must not reset navigation.
    assert(String(gameManager.getCurrentLeagueName()) == "MLB");
    handleBluetoothCommand("SYNC_COMPLETE");
    assert(String(gameManager.getCurrentLeagueName()) == "MLB");
    // Ordinary refresh: NBA commits first, MLB second, with no user wake needed.
    bluetoothManager.messages.push_back(game("NBA", "new-nba", 50));
    bluetoothManager.messages.push_back(game("MLB", "new-mlb", 60));
    runUntil(clockUs + 150000, {});
    assert(String(gameManager.getCurrentLeagueName()) == "MLB");
    assert(gameManager.getCurrentGame().awayScore == 60);
    assert(!frames.empty() && frames.back().league == "MLB" && frames.back().score == 60);
  } else if (scenario == "interrupted_sports") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    handleBluetoothCommand(R"({"version":1,"type":"slate_start","league":"NBA","slateId":"old","totalGames":1,"totalChunks":1})");
    handleBluetoothCommand(R"({"version":1,"type":"slate_chunk","slateId":"old","chunkIndex":0,"games":[{"id":"invalid-session","away":"Away","home":"Home","awayScore":99,"homeScore":1,"status":"LIVE","clock":"Q1"}]})");
    bluetoothManager.ended = true; handleBluetoothMessages();
    assert(!slateTransferActive);
    handleBluetoothCommand(R"({"version":1,"type":"slate_end","slateId":"old"})");
    assert(String(gameManager.getCurrentGame().eventId) == "nba-a");
  } else if (scenario == "startup_chunked") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    bluetoothManager.messages.push_back("SYNC_START");
    for (auto league : {"NBA", "MLB"}) {
      JsonDocument start, chunk, end;
      start["version"] = 1; start["type"] = "slate_start";
      start["league"] = league; start["slateId"] = league;
      start["totalGames"] = 1; start["totalChunks"] = 1;
      chunk["version"] = 1; chunk["type"] = "slate_chunk";
      chunk["slateId"] = league; chunk["chunkIndex"] = 0;
      JsonDocument entry; deserializeJson(entry, game(league, league, 20));
      chunk["games"].to<JsonArray>().add(entry.as<JsonObject>());
      end["version"] = 1; end["type"] = "slate_end"; end["slateId"] = league;
      for (const auto *doc : {&start, &chunk, &end}) {
        String packet; serializeJson(*doc, packet); bluetoothManager.messages.push_back(packet);
      }
    }
    bluetoothManager.messages.push_back("SYNC_COMPLETE");
    runUntil(400000, {});
    assert(bluetoothManager.messages.empty());
    assert(gameManager.getLeagueCount() == 2);
    assert(String(gameManager.getCurrentLeagueName()) == "NBA");
    gameManager.nextLeague(); drawCurrentScreen();
    assert(frames.back().league == "MLB" && frames.back().id == "MLB");
    // Neither availability nor drawing required a WAKE event.
  } else if (scenario == "fantasy_rx_loss") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    const String entry = R"({"version":1,"type":"fantasy_matchup","identity":"primary","leagueName":"League","userName":"User","opponentName":"Other","userScore":10,"opponentScore":5,"week":1,"status":"LIVE"})";
    handleBluetoothCommand(R"({"version":1,"type":"fantasy_slate_start"})");
    handleBluetoothCommand(entry); handleBluetoothCommand(R"({"version":1,"type":"fantasy_slate_end"})");
    assert(gameManager.getLeagueCount() == 3);
    gameManager.nextLeague(); gameManager.nextLeague();
    handleBluetoothCommand(R"({"version":1,"type":"fantasy_slate_start"})");
    handleBluetoothCommand(entry);
    bluetoothManager.lost = true; handleBluetoothMessages();
    assert(!gameManager.isFantasySlateTransferActive());
    handleBluetoothCommand(R"({"version":1,"type":"fantasy_slate_end"})");
    assert(gameManager.getCurrentFantasyMatchup()->userScore == 10);
    assert(String(gameManager.getCurrentFantasyMatchup()->identity) == "primary");
  } else if (scenario == "rx_loss" || scenario == "incomplete") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    handleBluetoothCommand(R"({"version":1,"type":"slate_start","league":"NBA","slateId":"s","totalGames":2,"totalChunks":2})");
    handleBluetoothCommand(R"({"version":1,"type":"slate_chunk","slateId":"s","chunkIndex":0,"games":[{"id":"staged","away":"Away","home":"Home","awayScore":99,"homeScore":1,"status":"LIVE","clock":"Q1"}]})");
    assert(slateTransferActive);
    if (scenario == "rx_loss") {
      bluetoothManager.lost = true; handleBluetoothMessages(); assert(!slateTransferActive);
    }
    handleBluetoothCommand(R"({"version":1,"type":"slate_end","slateId":"s"})");
    assert(String(gameManager.getCurrentGame().eventId) == "nba-a");
    // A malformed chunk must never leak partial state into committed storage.
    handleBluetoothCommand(R"({"version":1,"type":"slate_chunk","slateId":"s","chunkIndex":1,"games":[{"id":"bad"}]})");
    handleBluetoothCommand(R"({"version":1,"type":"slate_end","slateId":"s"})");
    assert(String(gameManager.getCurrentGame().eventId) == "nba-a");
    handleBluetoothCommand(R"({"version":1,"type":"golf_start","league":"PGA","transferId":"g","tournamentId":"t","tournamentName":"Golf","totalGolfers":1,"totalChunks":1})");
    assert(golfTransferActive);
    bluetoothManager.ended = true; handleBluetoothMessages(); assert(!golfTransferActive);
    assert(gameManager.getLeagueCount() == 2);
  } else if (scenario == "active_refresh_input") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    USBSerial.stalled = true; bluetoothManager.continuous = true;
    // Valid complete two-game slates force score refresh and redraw pressure.
    bluetoothManager.traffic = R"({"version":1,"type":"slate","league":"NBA","games":[{"id":"nba-a","away":"A","home":"B","awayScore":1,"homeScore":2,"status":"LIVE","clock":"Q1"},{"id":"nba-b","away":"C","home":"D","awayScore":3,"homeScore":4,"status":"LIVE","clock":"Q1"}]})";
    runUntil(1000000, {{100000, 260000}}); printMetrics(argv[1]);
    assert(String(gameManager.getCurrentGame().eventId) == "nba-b");
    assert(std::any_of(frames.begin(), frames.end(), [](const Frame &f) { return f.id == "nba-b" && f.at <= 700000; }));
    // Two long holds under repeated valid active-slate commits toggle once each.
    runUntil(2450000, {{1100000, 2400000}}); assert(deviceStandby);
    runUntil(4000000, {{2600000, 3900000}}); assert(!deviceStandby);
    assert(maxPollGap <= 60000);
  } else if (scenario == "coalesce") {
    bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
    for (int i = 0; i < 4; ++i) bluetoothManager.messages.push_back(game("NBA", "nba-a", i));
    runUntil(150000, {}); printMetrics(argv[1]);
    assert(bluetoothManager.messages.empty());
    assert(frames.size() < 4);
    assert(frames.back().score == 3);
  } else { assert(false); }
}
'''
include = os.environ.get('ARDUINOJSON_INCLUDE', str(Path.home() / 'Documents/Arduino/libraries/ArduinoJson/src'))
scenarios = ['no_ble_healthy', 'no_ble', 'single_traffic', 'double_traffic', 'bounce_traffic', 'triple_traffic', 'hold_traffic', 'sync_selection', 'startup_chunked', 'interrupted_sports', 'rx_loss', 'fantasy_rx_loss', 'incomplete', 'active_refresh_input', 'coalesce']
def build_harness(directory, test_source=tests):
    cpp = Path(directory) / 'test.cpp'
    cpp.write_text(prefix + globals_ + prototypes + '\n' + functions + test_source)
    binary = Path(directory) / 'test'
    flags = os.environ.get('NATIVE_SANITIZERS', '').split() + os.environ.get('NATIVE_TIMING', '').split()
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-missing-field-initializers', *flags, '-I' + str(root / 'test/native'),
                    '-I' + str(root), '-I' + include, str(cpp), str(root / 'GameManager.cpp'),
                    '-o', str(binary)], check=True)
    return binary


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='responsiveness-native-') as directory:
        binary = build_harness(directory)
        failures = [case for case in scenarios if subprocess.run([str(binary), case]).returncode]
        if failures: raise SystemExit('Failed scenarios: ' + ', '.join(failures))
    print('Simulated loop/input timing, synchronization, and transfer tests passed.')
