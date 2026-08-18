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
  if (receivedSlateActive)
  {
    return receivedGames[receivedGameIndex];
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
  GameData game = {};
  strlcpy(game.awayTeam, away, sizeof(game.awayTeam));
  strlcpy(game.homeTeam, home, sizeof(game.homeTeam));
  strlcpy(game.status, status, sizeof(game.status));
  strlcpy(game.clock, clock, sizeof(game.clock));
  game.awayScore = awayScore;
  game.homeScore = homeScore;
  setReceivedSlate(league, &game, 1);
}


void GameManager::setReceivedSlate(
  const char *league,
  const GameData *games,
  uint8_t gameCount
)
{
  strlcpy(receivedLeague, league, sizeof(receivedLeague));
  for (uint8_t index = 0; index < gameCount; index++)
  {
    receivedGames[index] = games[index];
  }
  receivedGameCount = gameCount;
  receivedGameIndex = 0;
  receivedSlateActive = gameCount > 0;
}


void GameManager::leaveReceivedSlate()
{
  receivedSlateActive = false;
  receivedGameCount = 0;
  receivedGameIndex = 0;
}


/**
 * Move to the next game in the current league.
 */
void GameManager::nextGame()
{
  if (receivedSlateActive)
  {
    receivedGameIndex = (receivedGameIndex + 1) % receivedGameCount;
    return;
  }
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
  if (receivedSlateActive)
  {
    receivedGameIndex = receivedGameIndex == 0
      ? receivedGameCount - 1
      : receivedGameIndex - 1;
    return;
  }
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
  leaveReceivedSlate();
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
  leaveReceivedSlate();
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
  if (receivedSlateActive)
  {
    return receivedLeague;
  }

  return getCurrentLeague().name;
}


uint8_t GameManager::getCurrentGameNumber()
{
  if (receivedSlateActive)
  {
    return receivedGameIndex + 1;
  }

  getCurrentGame();
  return currentGameIndex + 1;
}


uint8_t GameManager::getCurrentGameCount()
{
  if (receivedSlateActive)
  {
    return receivedGameCount;
  }

  return getCurrentLeague().gameCount;
}


uint8_t GameManager::getCurrentLeagueNumber()
{
  if (receivedSlateActive)
  {
    return 1;
  }

  getCurrentLeague();
  return currentLeagueIndex + 1;
}


uint8_t GameManager::getLeagueCount()
{
  if (receivedSlateActive)
  {
    return 1;
  }

  return MOCK_LEAGUE_COUNT;
}
