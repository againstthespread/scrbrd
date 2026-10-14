#!/usr/bin/env python3
"""Consume the fixture exported by the mobile protocol compatibility test.

Usage: python3 test/mobile_protocol_compatibility_test.py /tmp/scrbrd-fixture.json
Runs real firmware parsing/storage/lifecycle; graphics and BLE are native stubs.
"""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from responsiveness_test import build_harness, root, tests

if len(sys.argv) != 2:
    raise SystemExit('Supply the mobile-generated protocol fixture JSON path.')
fixture = Path(sys.argv[1]).resolve()
values = json.loads(fixture.read_text())
header = (root / 'BluetoothManager.h').read_text()
assert values['advertisingName'] == re.search(r'BLEDevice::init\("([^"]+)"', (root / 'BluetoothManager.cpp').read_text()).group(1)
for field, name in [('service', 'BLUETOOTH_SERVICE_UUID'), ('write', 'BLUETOOTH_RX_CHARACTERISTIC_UUID'), ('wake', 'BLUETOOTH_WAKE_CHARACTERISTIC_UUID')]:
    assert values[field] == re.search(name + r'\[\] = "([^"]+)"', header).group(1)
main = r'''
#include <fstream>
#include <iterator>
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream file(argv[1]);
  const String input((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  JsonDocument fixture; assert(!deserializeJson(fixture, input));
  setup(); bluetoothManager.connected = true; lastDisplayedBluetoothConnected = true;
  unsigned steps = 0, packets = 0;
  for (JsonObjectConst step : fixture["steps"].as<JsonArrayConst>()) {
    const String previousLeague = gameManager.getCurrentLeagueName();
    const String previousEvent = gameManager.getCurrentGame().eventId;
    const int priorFrames = frames.size();
    if (step["standby"].as<bool>()) enterStandby();
    for (JsonArrayConst bytes : step["packets"].as<JsonArrayConst>()) {
      String packet;
      for (int value : bytes) packet.push_back(static_cast<char>(value));
      assert(packet.size() <= 512); handleBluetoothCommand(packet); ++packets;
    }
    if (step["timeout"].as<bool>()) {
      clockUs += 30001000; handleBluetoothMessages();
      assert(!slateTransferActive && !golfTransferActive && !gameManager.isFantasySlateTransferActive());
    }
    if (step["preserveSelection"].as<bool>()) {
      assert(previousLeague == gameManager.getCurrentLeagueName());
      assert(previousEvent == gameManager.getCurrentGame().eventId);
    }
    if (step["standby"].as<bool>()) {
      assert(static_cast<int>(frames.size()) == priorFrames);
      exitStandby(); assert(!frames.empty());
    }
    if (step["select"].is<const char *>()) {
      const char *league = step["select"].as<const char *>();
      for (unsigned attempts = 0; strcmp(gameManager.getCurrentLeagueName(), league) != 0; ++attempts) {
        assert(attempts < 8); gameManager.nextLeague();
      }
    }
    for (unsigned move = 0; move < step["nextGames"].as<unsigned>(); ++move) gameManager.nextGame();
    assert(gameManager.getLeagueCount() == step["leagueCount"].as<unsigned>());
    if (gameManager.isCurrentLeagueGolf()) {
      assert(gameManager.getCurrentGolfPageCount() == step["gameCount"].as<unsigned>());
    } else if (gameManager.isCurrentLeagueFantasy()) {
      const String identity = gameManager.getCurrentFantasyMatchup()->identity;
      unsigned count = 0;
      do { gameManager.nextGame(); assert(++count <= 8); }
      while (identity != gameManager.getCurrentFantasyMatchup()->identity);
      assert(count == step["gameCount"].as<unsigned>());
    } else assert(gameManager.getCurrentGameCount() == step["gameCount"].as<unsigned>());
    if (step["eventId"].is<const char *>()) assert(strcmp(gameManager.getCurrentGame().eventId, step["eventId"].as<const char *>()) == 0);
    if (step["score"].is<int>()) assert(gameManager.getCurrentGame().awayScore == step["score"].as<int>());
    if (step["baseball"].as<bool>()) assert(gameManager.getCurrentGame().hasBaseballState && gameManager.getCurrentGame().outs == 2);
    if (step["football"].as<bool>()) assert(gameManager.getCurrentGame().hasFootballState && gameManager.getCurrentGame().down == 3);
    if (step["fantasyIdentity"].is<const char *>()) {
      const auto *matchup = gameManager.getCurrentFantasyMatchup();
      assert(matchup && strcmp(matchup->identity, step["fantasyIdentity"].as<const char *>()) == 0);
      assert(matchup->hasUserProjectedScore && matchup->userProjectedScore == 22.5f);
    }
    ++steps;
  }
  std::printf("Cross-repository UUID/packet/lifecycle compatibility: %u steps, %u mobile-fixture packets passed (including negative tests).\n", steps, packets);
}
'''
with tempfile.TemporaryDirectory(prefix='mobile-firmware-native-') as directory:
    binary = build_harness(directory, tests[:tests.index('int main(')] + main)
    subprocess.run([str(binary), str(fixture)], check=True)
