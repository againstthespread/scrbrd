#!/usr/bin/env python3
"""Exercise real display-state and disconnect handling without radio/graphics."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'scrbrd.ino').read_text()


def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


prefix = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "GameManager.h"
GameManager gameManager;
struct Bluetooth {
  bool connected = true, ended = false, lost = false;
  bool isBluetoothConnected() { return connected; }
  bool takeDisconnectEvent() { const bool result = ended; ended = false; return result; }
  bool takeReceiveLossEvent() { const bool result = lost; lost = false; return result; }
} bluetoothManager;
bool slateTransferActive = false, golfTransferActive = false;
bool authoritativeSyncSessionActive = false;
void clearStagingSlate(const char *) { slateTransferActive = false; }
void clearGolfStaging(const char *) { golfTransferActive = false; }
void drawCurrentScreen();
'''
enums = source[source.index('enum SyncLifecycleState'):source.index('const uint8_t BOOT_BUTTON_PIN')]
functions = '\n'.join(function(sig) for sig in [
    'DeviceDisplayState currentDisplayState()',
    'void serviceBluetoothTransferInterruptions()',
])
tests = r'''
int redraws = 0;
DeviceDisplayState renderedState = DISPLAY_UNKNOWN;
void drawCurrentScreen() { ++redraws; renderedState = currentDisplayState(); }
void interrupt(bool loss = false, bool rapid = false) {
  const int previousRedraws = redraws;
  bluetoothManager.connected = loss || rapid;
  bluetoothManager.ended = !loss;
  bluetoothManager.lost = loss;
  serviceBluetoothTransferInterruptions();
  bluetoothManager.connected = true;
  serviceBluetoothTransferInterruptions();
  assert(redraws == previousRedraws + 1);
  assert(renderedState == DISPLAY_LIVE_CONTENT_PAUSED);
}
int main() {
  GameData game = {};
  strcpy(game.eventId, "cached");
  assert(gameManager.setReceivedSlate("NFL", &game, 1));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  interrupt(false, true);
  assert(gameManager.hasReceivedContent());
  // A transport reconnect has not reverified the provider's cached scores.
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  syncLifecycleState = SYNC_COMPLETED;
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.setReceivedSlate("NFL", &game, 1));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  assert(gameManager.setReceivedSlate("NBA", &game, 1));
  interrupt(true);
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.setReceivedSlate("NFL", &game, 1));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  gameManager.nextLeague();
  assert(strcmp(gameManager.getCurrentLeagueName(), "NBA") == 0);
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(!gameManager.setReceivedSlate("NBA", &game, 0));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.setReceivedSlate("NBA", &game, 1));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);

  GolfLeaderboardRow golfer = {};
  assert(gameManager.setReceivedGolfLeaderboard("PGA", "t", "Tournament", &golfer, 1));
  gameManager.nextLeague();
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  interrupt();
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(!gameManager.setReceivedGolfLeaderboard("PGA", "t", "Tournament", &golfer, 0));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.setReceivedGolfLeaderboard("PGA", "t", "Tournament", &golfer, 1));
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);

  FantasyMatchupData fantasy = {};
  strcpy(fantasy.identity, "a"); fantasy.userScore = 10;
  assert(gameManager.beginFantasySlate()); assert(gameManager.stageFantasyMatchup(fantasy));
  strcpy(fantasy.identity, "b"); assert(gameManager.stageFantasyMatchup(fantasy));
  assert(gameManager.commitFantasySlate()); gameManager.nextLeague();
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  interrupt(); gameManager.nextGame();
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  gameManager.nextLeague(); gameManager.previousLeague();
  assert(gameManager.isCurrentLeagueFantasy());
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.beginFantasySlate()); fantasy.userScore = 99;
  assert(gameManager.stageFantasyMatchup(fantasy));
  assert(gameManager.getCurrentFantasyMatchup()->userScore == 10);
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  gameManager.cancelFantasySlate();
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT_PAUSED);
  assert(gameManager.beginFantasySlate()); assert(gameManager.stageFantasyMatchup(fantasy));
  assert(gameManager.commitFantasySlate());
  assert(currentDisplayState() == DISPLAY_LIVE_CONTENT);
  gameManager.clearReceivedContent();
  assert(!gameManager.hasReceivedContent());
  syncLifecycleState = SYNC_COMPLETED_EMPTY;
  assert(currentDisplayState() == DISPLAY_NO_GAMES_TODAY);
  std::puts("Cached verification: reconnect, RX loss, per-category commit, golf, fantasy navigation/staging and empty reset passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='cached-verification-native-') as directory:
    directory = Path(directory)
    cpp = directory / 'test.cpp'
    cpp.write_text(prefix + enums + '\nSyncLifecycleState syncLifecycleState = SYNC_WAITING;\n' + functions + tests)
    binary = directory / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-missing-field-initializers',
                    *os.environ.get('NATIVE_SANITIZERS', '').split(),
                    '-I' + str(root / 'test/native'), '-I' + str(root),
                    str(cpp), str(root / 'GameManager.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
