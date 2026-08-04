#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\game_data.h"
/**
 * Mock sports data for Peter's Sports Hub.
 *
 * This file is intentionally offline-only. Future live data can replace this
 * source without changing the display drawing code.
 */

#ifndef GAME_DATA_H
#define GAME_DATA_H

#include <Arduino.h>

struct GameData
{
  const char *status;
  const char *awayTeam;
  const char *homeTeam;
  uint8_t awayScore;
  uint8_t homeScore;
  const char *clock;
};

struct LeagueData
{
  const char *name;
  const GameData *games;
  uint8_t gameCount;
};

const GameData NFL_GAMES[] = {
  { "LIVE", "PATRIOTS", "BILLS", 24, 17, "Q4   8:31" },
  { "LIVE", "COWBOYS", "EAGLES", 14, 21, "Q3   2:18" },
  { "FINAL", "CHIEFS", "BRONCOS", 31, 20, "FINAL" },
};

const GameData NBA_GAMES[] = {
  { "LIVE", "CELTICS", "KNICKS", 88, 82, "Q4   6:44" },
  { "HALF", "LAKERS", "SUNS", 55, 59, "HALF" },
  { "FINAL", "BULLS", "HEAT", 102, 98, "FINAL" },
};

const GameData MLB_GAMES[] = {
  { "LIVE", "YANKEES", "RED SOX", 4, 3, "BOT 7" },
  { "LIVE", "DODGERS", "GIANTS", 2, 2, "TOP 5" },
  { "FINAL", "METS", "BRAVES", 6, 8, "FINAL" },
};

const LeagueData MOCK_LEAGUES[] = {
  { "NFL", NFL_GAMES, sizeof(NFL_GAMES) / sizeof(NFL_GAMES[0]) },
  { "NBA", NBA_GAMES, sizeof(NBA_GAMES) / sizeof(NBA_GAMES[0]) },
  { "MLB", MLB_GAMES, sizeof(MLB_GAMES) / sizeof(MLB_GAMES[0]) },
};

const uint8_t MOCK_LEAGUE_COUNT = sizeof(MOCK_LEAGUES) / sizeof(MOCK_LEAGUES[0]);

#endif
