#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\GameManager.cpp"
#include "GameManager.h"

/**
 * Return the selected league, falling back to league 0 if needed.
 */
const LeagueData &GameManager::getCurrentLeague()
{
  if (currentLeagueIndex >= MOCK_LEAGUE_COUNT)
  {
    currentLeagueIndex = 0;
  }

  return MOCK_LEAGUES[currentLeagueIndex];
}


/**
 * Return the selected game, falling back to game 0 if needed.
 */
const GameData &GameManager::getCurrentGame()
{
  const LeagueData &league = getCurrentLeague();

  if (currentGameIndex >= league.gameCount)
  {
    currentGameIndex = 0;
  }

  return league.games[currentGameIndex];
}


/**
 * Move to the next game in the current league.
 */
void GameManager::nextGame()
{
  const LeagueData &league = getCurrentLeague();

  currentGameIndex++;

  if (currentGameIndex >= league.gameCount)
  {
    currentGameIndex = 0;
  }
}


/**
 * Move to the previous game in the current league.
 */
void GameManager::previousGame()
{
  const LeagueData &league = getCurrentLeague();

  if (currentGameIndex == 0)
  {
    currentGameIndex = league.gameCount - 1;
  }
  else
  {
    currentGameIndex--;
  }
}


/**
 * Move to the next league and start at game 0.
 */
void GameManager::nextLeague()
{
  currentLeagueIndex++;

  if (currentLeagueIndex >= MOCK_LEAGUE_COUNT)
  {
    currentLeagueIndex = 0;
  }

  currentGameIndex = 0;
}


/**
 * Move to the previous league and start at game 0.
 */
void GameManager::previousLeague()
{
  if (currentLeagueIndex == 0)
  {
    currentLeagueIndex = MOCK_LEAGUE_COUNT - 1;
  }
  else
  {
    currentLeagueIndex--;
  }

  currentGameIndex = 0;
}


const char *GameManager::getCurrentLeagueName()
{
  return getCurrentLeague().name;
}


uint8_t GameManager::getCurrentGameNumber()
{
  getCurrentGame();
  return currentGameIndex + 1;
}


uint8_t GameManager::getCurrentGameCount()
{
  return getCurrentLeague().gameCount;
}


uint8_t GameManager::getCurrentLeagueNumber()
{
  getCurrentLeague();
  return currentLeagueIndex + 1;
}


uint8_t GameManager::getLeagueCount()
{
  return MOCK_LEAGUE_COUNT;
}
