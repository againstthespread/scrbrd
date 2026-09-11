#!/usr/bin/env python3
"""Run actual sketch queue/standby/display functions with native hardware stubs.

Usage: python3 test/fantasy_alert_queue_test.py
Requires c++ on PATH. Packet parsing is covered by the ESP32 compile; this
harness exercises the display lifecycle and real GameManager storage.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'SportsHub.ino').read_text()

def function(signature):
    start = source.index(signature + '\n{')
    end = source.index('\n}', start) + 2
    return source[start:end]

prefix = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "GameManager.h"
using String = std::string;
struct Serial { template<class T> void print(T) {} template<class T> void println(T) {} } USBSerial;
unsigned long now = 0;
unsigned long millis() { return now; }
std::vector<bool> backlights;
const int LCD_BL = 1, HIGH = 1, LOW = 0;
void digitalWrite(int, int value) { backlights.push_back(value); }
int normalDraws = 0;
std::vector<std::string> displayed;
void drawCurrentScreen();
void drawFantasyAlert();
void drawDashboard() { ++normalDraws; }
void drawUpdatesPausedWarning() {}
void drawEmptyTodayScreen() { ++normalDraws; }
void drawConnectedLoadingScreen() { ++normalDraws; }
void drawConnectScreen() { ++normalDraws; }
'''
enums = source[source.index('enum SyncLifecycleState'):source.index('const uint8_t BOOT_BUTTON_PIN')]
data = source[source.index('const unsigned long FANTASY_ALERT_DURATION_MS'):source.index('BluetoothManager bluetoothManager;')]
globals_ = source[source.index('bool deviceStandby'):source.index('// Chunked slate staging')]
stubs = r'''
DeviceDisplayState currentDisplayState() { return DISPLAY_LIVE_CONTENT; }
const char *displayStateDiagnostic(DeviceDisplayState) { return "test"; }
void drawFantasyAlert() { displayed.push_back(fantasyAlert.player); }
bool slateTransferActive = false, golfTransferActive = false;
void clearStagingSlate(const char *) {}
void clearGolfStaging(const char *) {}
'''
functions = '\n'.join(function(sig) for sig in [
    'void setDisplayBacklight(bool enabled)', 'void enterStandby()',
    'void exitStandby()', 'void restoreStandbyAfterFantasyAlert()',
    'void drawCurrentScreen()', 'void startFantasyAlert(const FantasyAlertData &next)',
    'bool enqueueFantasyAlert(const FantasyAlertData &next)',
    'void finishFantasyAlert()', 'void expireFantasyAlertIfNeeded()',
    'void markRealContentActivated()',
])
# Exercise the actual authoritative control branch without ArduinoJson hardware.
commands = function('void handleBluetoothCommand(const String &message)')
commands = commands[:commands.index('  JsonDocument document;')] + '\n}'
tests = r'''
FantasyAlertData alert(const char *name) {
  FantasyAlertData result = {};
  strlcpy(result.player, name, sizeof(result.player));
  return result;
}
void reset() {
  fantasyAlertActive = false;
  fantasyAlertQueueHead = fantasyAlertQueueCount = 0;
  deviceStandby = fantasyAlertWokeDisplayFromStandby = false;
  authoritativeSyncSessionActive = false;
  now = 100;
  normalDraws = 0;
  backlights.clear(); displayed.clear();
}
void expire() { now += FANTASY_ALERT_DURATION_MS; expireFantasyAlertIfNeeded(); }
int main() {
  reset();
  enqueueFantasyAlert(alert("A"));
  now += 1000;
  enqueueFantasyAlert(alert("B")); enqueueFantasyAlert(alert("C"));
  assert(displayed == std::vector<std::string>{"A"});
  assert(fantasyAlertStartedAt == 100);
  now = 7099; expireFantasyAlertIfNeeded();
  assert(displayed.size() == 1);
  now = 7100; expireFantasyAlertIfNeeded();
  assert(displayed.back() == "B" && fantasyAlertStartedAt == now);
  now += 6999; expireFantasyAlertIfNeeded();
  assert(displayed.size() == 2);
  ++now; expireFantasyAlertIfNeeded();
  assert(displayed == (std::vector<std::string>{"A", "B", "C"}));
  expire(); assert(!fantasyAlertActive && normalDraws == 1);

  for (bool manualExit : {false, true}) {
    reset(); enterStandby();
    enqueueFantasyAlert(alert("A")); enqueueFantasyAlert(alert("B")); enqueueFantasyAlert(alert("C"));
    assert(backlights == (std::vector<bool>{false, true}));
    if (manualExit) exitStandby();
    const auto writes = backlights.size();
    expire(); expire();
    assert(backlights.size() == writes && backlights.back());
    expire();
    assert(backlights.back() == manualExit);
    assert(deviceStandby != manualExit);
  }

  // Overflow keeps all accepted alerts; exercise ring wrap on multiple cycles.
  reset();
  for (int cycle = 0; cycle < 3; ++cycle) {
    enqueueFantasyAlert(alert("active"));
    for (int i = 0; i < FANTASY_ALERT_QUEUE_CAPACITY; ++i)
      assert(enqueueFantasyAlert(alert(std::to_string(i).c_str())));
    assert(!enqueueFantasyAlert(alert("DROP")));
    assert(std::string(fantasyAlert.player) == "active");
    for (int i = 0; i < FANTASY_ALERT_QUEUE_CAPACITY; ++i) {
      expire(); assert(displayed.back() == std::to_string(i));
    }
    expire(); assert(!fantasyAlertActive);
  }

  reset(); enterStandby();
  enqueueFantasyAlert(alert("A")); enqueueFantasyAlert(alert("B")); enqueueFantasyAlert(alert("C"));
  handleBluetoothCommand("SYNC_START");
  assert(displayed == (std::vector<std::string>{"A", "B"}));
  assert(fantasyAlertQueueCount == 1 && backlights.back());
  handleBluetoothCommand("SYNC_START"); // Duplicate must not cancel B.
  assert(displayed.size() == 2);
  expire(); expire(); assert(!backlights.back());

  reset();
  enqueueFantasyAlert(alert("A")); enqueueFantasyAlert(alert("B"));
  GameData game = {};
  strlcpy(game.eventId, "live-game", sizeof(game.eventId));
  assert(gameManager.setReceivedSlate("NFL", &game, 1));
  markRealContentActivated();
  assert(gameManager.hasReceivedContent());
  assert(normalDraws == 0 && displayed == std::vector<std::string>{"A"});
  assert(fantasyAlertStartedAt == 100 && fantasyAlertQueueCount == 1);
  expire(); expire(); assert(normalDraws == 1);
}
'''
with tempfile.TemporaryDirectory(prefix='fantasy-native-') as directory:
    cpp = Path(directory) / 'test.cpp'
    cpp.write_text(prefix + enums + data + globals_ + stubs + functions + commands + tests)
    executable = Path(directory) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
                    '-I' + str(root / 'test/native'), '-I' + str(root),
                    str(cpp), str(root / 'GameManager.cpp'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('Fantasy FIFO, timers, overflow/wrap, standby, sync cancellation, and sports storage passed.')
