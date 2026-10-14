#!/usr/bin/env python3
"""Exercise the real scoreboard renderer with native drawing stubs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'scrbrd.ino').read_text()
start = source.index('void drawScoreboard(const GameData &game)\n{')
end = source.index('\n}', start) + 2
renderer = source[start:end]
constants = source[source.index('const int16_t TEAM_CARD_X'):
                   source.index('// Production placeholder:')]

harness = r'''
#include <algorithm>
#include <cassert>
#include <string>
#include <vector>
#include "GameManager.h"
#include "responsiveness_timing.h"
#include "pin_config.h"
using std::max;
class String : public std::string {
public:
  using std::string::string;
  String(int value) : std::string(std::to_string(value)) {}
  String(const std::string &value) : std::string(value) {}
};
struct Graphics {
  template<class... T> void fillRoundRect(T...) {}
  template<class... T> void drawRoundRect(T...) {}
  template<class... T> void drawFastHLine(T...) {}
  void setTextColor(uint16_t) {}
  void setTextSize(uint8_t) {}
  void setCursor(int, int) {}
  void print(const String &) {}
} graphics;
Graphics *gfx = &graphics;
GameManager gameManager;
const uint16_t COLOR_DARK_GRAY = 0, COLOR_LIGHT_BLUE = 0;
const uint16_t COLOR_GRAY = 0, COLOR_STATUS_BG = 0;
int baseballDraws = 0, footballDraws = 0;
int clockLeft = 0, clockRight = 0;
std::vector<int> teamRows, scoreRows;
uint16_t measuredTextWidth(const String &text, uint8_t) { return text.size(); }
String truncateTextToWidth(const String &text, uint16_t, uint8_t) { return text; }
void drawTextCenteredInRegion(const String &, int16_t left, int16_t right,
                              int16_t y, uint8_t, uint16_t) {
  if (y == 176) { clockLeft = left; clockRight = right; }
}
void drawTeamName(const char *, int y) { teamRows.push_back(y); }
void drawScore(uint8_t, uint16_t, int y) { scoreRows.push_back(y); }
void drawBaseballSituation(const GameData &) { ++baseballDraws; }
void drawFootballSituation(const GameData &) { ++footballDraws; }
'''

tests = r'''
void check(const char *league, const char *status, bool baseballAvailable,
           bool occupiedBases, uint8_t outs, bool footballAvailable,
           bool expectBaseball, bool expectFootball) {
  GameData game = {};
  strlcpy(game.status, status, sizeof(game.status));
  strlcpy(game.clock, "BOT 7", sizeof(game.clock));
  game.hasBaseballState = baseballAvailable;
  game.runnerOnFirst = game.runnerOnSecond = game.runnerOnThird = occupiedBases;
  game.outs = outs;
  game.hasFootballState = footballAvailable;
  gameManager.clearReceivedContent();
  assert(gameManager.setReceivedSlate(league, &game, 1));
  baseballDraws = footballDraws = 0;
  clockLeft = clockRight = 0;
  teamRows.clear(); scoreRows.clear();
  drawScoreboard(gameManager.getCurrentGame());
  assert(baseballDraws == (expectBaseball ? 1 : 0));
  assert(footballDraws == (expectFootball ? 1 : 0));
  assert(clockLeft == CARD_INNER_LEFT);
  assert(clockRight == (expectBaseball ? 168 : CARD_INNER_RIGHT));
  assert(teamRows == (std::vector<int>{112, 144}));
  assert(scoreRows == (std::vector<int>{107, 139}));
}
int main() {
  // Legacy/no-state MLB packets keep the full-width clock and normal rows.
  check("MLB", "LIVE", false, false, 0, false, false, false);
  // Stale field values must not override the explicit availability flag.
  check("MLB", "LIVE", false, true, 2, false, false, false);
  // Empty bases and zero outs are legitimate when state IS available.
  check("MLB", "LIVE", true, false, 0, false, true, false);
  check("MLB", "LIVE", true, true, 2, false, true, false);
  check("MLB", "UPCOMING", true, true, 2, false, false, false);
  check("MLB", "FINAL", true, true, 2, false, false, false);
  check("NBA", "LIVE", true, true, 2, false, false, false);
  // Existing football rendering and clock width remain unchanged.
  check("NFL", "LIVE", true, true, 2, true, false, true);
  check("NCAAF", "LIVE", false, false, 0, true, false, true);
  check("NFL", "LIVE", false, false, 0, false, false, false);
}
'''

with tempfile.TemporaryDirectory(prefix='scoreboard-native-') as directory:
    cpp = Path(directory) / 'test.cpp'
    cpp.write_text(harness + constants + renderer + tests)
    executable = Path(directory) / 'test'
    subprocess.run([
        'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-Wno-missing-field-initializers', '-I' + str(root / 'test/native'),
        '-I' + str(root), str(cpp), str(root / 'GameManager.cpp'),
        '-o', str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
print('Scoreboard availability, clock layout, and football rendering passed.')
