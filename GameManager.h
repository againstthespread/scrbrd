/**
 * GameManager keeps track of which mock game is selected.
 */

#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <Arduino.h>
#include "game_data.h"

class GameManager
{
public:
  static const uint8_t MAX_RECEIVED_SLATE_GAMES = 20;

  const GameData &getCurrentGame();

  void setReceivedGame(
    const char *league,
    const char *away,
    const char *home,
    uint8_t awayScore,
    uint8_t homeScore,
    const char *status,
    const char *clock
  );
  void setReceivedSlate(
    const char *league,
    const GameData *games,
    uint8_t gameCount
  );

  void nextGame();
  void previousGame();
  void nextLeague();
  void previousLeague();

  const char *getCurrentLeagueName();
  uint8_t getCurrentGameNumber();
  uint8_t getCurrentGameCount();
  uint8_t getCurrentLeagueNumber();
  uint8_t getLeagueCount();

private:
  uint8_t currentLeagueIndex = 0;
  uint8_t currentGameIndex = 0;
  bool receivedSlateActive = false;
  char receivedLeague[13] = "";
  GameData receivedGames[MAX_RECEIVED_SLATE_GAMES] = {};
  uint8_t receivedGameCount = 0;
  uint8_t receivedGameIndex = 0;

  const LeagueData &getCurrentLeague();
  void leaveReceivedSlate();
};

#endif
