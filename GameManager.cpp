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
  if (receivedGameActive)
  {
    return receivedGame;
  }

  const LeagueData &league = getCurrentLeague();

  if (currentGameIndex >= league.gameCount)
  {
    currentGameIndex = 0;
  }

  return league.games[currentGameIndex];
}


/**
 * Store one validated BLE game packet in owned memory.
 */
void GameManager::setReceivedGame(
  const char *league,
  const char *away,
  const char *home,
  uint8_t awayScore,
  uint8_t homeScore,
  const char *status,
  const char *clock
)
{
  strlcpy(receivedLeague, league, sizeof(receivedLeague));
  strlcpy(receivedAway, away, sizeof(receivedAway));
  strlcpy(receivedHome, home, sizeof(receivedHome));
  strlcpy(receivedStatus, status, sizeof(receivedStatus));
  strlcpy(receivedClock, clock, sizeof(receivedClock));
  receivedGame.awayScore = awayScore;
  receivedGame.homeScore = homeScore;
  receivedGameActive = true;
}


void GameManager::leaveReceivedGame()
{
  receivedGameActive = false;
}


/**
 * Move to the next game in the current league.
 */
void GameManager::nextGame()
{
  leaveReceivedGame();
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
  leaveReceivedGame();
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
  leaveReceivedGame();
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
  leaveReceivedGame();
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
  if (receivedGameActive)
  {
    return receivedLeague;
  }

  return getCurrentLeague().name;
}


uint8_t GameManager::getCurrentGameNumber()
{
  if (receivedGameActive)
  {
    return 1;
  }

  getCurrentGame();
  return currentGameIndex + 1;
}


uint8_t GameManager::getCurrentGameCount()
{
  if (receivedGameActive)
  {
    return 1;
  }

  return getCurrentLeague().gameCount;
}


uint8_t GameManager::getCurrentLeagueNumber()
{
  if (receivedGameActive)
  {
    return 1;
  }

  getCurrentLeague();
  return currentLeagueIndex + 1;
}


uint8_t GameManager::getLeagueCount()
{
  if (receivedGameActive)
  {
    return 1;
  }

  return MOCK_LEAGUE_COUNT;
}
