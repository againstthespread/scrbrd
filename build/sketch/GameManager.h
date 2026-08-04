#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\GameManager.h"
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
  const GameData &getCurrentGame();

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

  const LeagueData &getCurrentLeague();
};

#endif
