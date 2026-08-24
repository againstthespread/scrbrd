#include "GameManager.h"

bool GameManager::hasReceivedContent() const
{
  return receivedLeagueCount > 0;
}

void GameManager::clearReceivedContent()
{
  for (uint8_t index = 0; index < receivedLeagueCount; index++)
  {
    receivedLeagues[index] = {};
  }
  receivedLeagueCount = 0;
  currentReceivedLeagueIndex = 0;
  currentReceivedGameIndex = 0;
  largeSlateOwnerIndex = -1;
  lastSlateError = nullptr;
}

GameData *GameManager::gamesForReceivedLeague(uint8_t leagueIndex)
{
  return receivedLeagues[leagueIndex].usesLargeSlateStorage
    ? largeSlateGames
    : receivedLeagues[leagueIndex].games;
}

const GameData *GameManager::gamesForReceivedLeague(uint8_t leagueIndex) const
{
  return receivedLeagues[leagueIndex].usesLargeSlateStorage
    ? largeSlateGames
    : receivedLeagues[leagueIndex].games;
}

void GameManager::releaseLargeSlateIfOwnedBy(uint8_t leagueIndex)
{
  if (largeSlateOwnerIndex == static_cast<int8_t>(leagueIndex))
  {
    largeSlateOwnerIndex = -1;
  }
  receivedLeagues[leagueIndex].usesLargeSlateStorage = false;
}

const char *GameManager::getLastSlateError() const
{
  return lastSlateError == nullptr ? "unknown slate storage error" : lastSlateError;
}

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
  if (receivedLeagueCount > 0)
  {
    return gamesForReceivedLeague(currentReceivedLeagueIndex)[
      currentReceivedGameIndex
    ];
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
bool GameManager::setReceivedGame(
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
  return setReceivedSlate(league, &game, 1);
}


int8_t GameManager::findReceivedLeague(const char *league)
{
  for (uint8_t index = 0; index < receivedLeagueCount; index++)
  {
    if (strcmp(receivedLeagues[index].name, league) == 0)
    {
      return static_cast<int8_t>(index);
    }
  }
  return -1;
}


bool GameManager::setReceivedSlate(
  const char *league,
  const GameData *games,
  uint8_t gameCount
)
{
  lastSlateError = nullptr;
  if (gameCount == 0 || gameCount > MAX_LARGE_SLATE_GAMES)
  {
    lastSlateError = "slate game count exceeds 1..72";
    return false;
  }

  int8_t existingIndex = findReceivedLeague(league);
  if (existingIndex < 0 && receivedLeagueCount >= MAX_RECEIVED_LEAGUES)
  {
    lastSlateError = "received league capacity reached";
    return false;
  }

  uint8_t targetIndex = existingIndex >= 0
    ? static_cast<uint8_t>(existingIndex)
    : receivedLeagueCount;
  const bool needsLargeStorage = gameCount > MAX_STANDARD_SLATE_GAMES;
  if (needsLargeStorage &&
      largeSlateOwnerIndex >= 0 &&
      largeSlateOwnerIndex != static_cast<int8_t>(targetIndex))
  {
    lastSlateError = "shared large-slate storage already owned by another league";
    return false;
  }
  const bool replacingActive = existingIndex >= 0 &&
    targetIndex == currentReceivedLeagueIndex;
  const uint8_t previousGameIndex = currentReceivedGameIndex;
  char previousEventId[49] = {};
  if (replacingActive &&
      receivedLeagues[targetIndex].contentType == RECEIVED_TEAM_SPORT &&
      previousGameIndex < receivedLeagues[targetIndex].gameCount)
  {
    strlcpy(
      previousEventId,
      gamesForReceivedLeague(targetIndex)[previousGameIndex].eventId,
      sizeof(previousEventId)
    );
  }
  ReceivedLeague &target = receivedLeagues[targetIndex];
  strlcpy(target.name, league, sizeof(target.name));
  GameData *targetGames = needsLargeStorage ? largeSlateGames : target.games;
  for (uint8_t index = 0; index < gameCount; index++)
  {
    targetGames[index] = games[index];
  }
  if (needsLargeStorage)
  {
    largeSlateOwnerIndex = static_cast<int8_t>(targetIndex);
    target.usesLargeSlateStorage = true;
  }
  else
  {
    releaseLargeSlateIfOwnedBy(targetIndex);
  }
  target.gameCount = gameCount;
  target.contentType = RECEIVED_TEAM_SPORT;

  if (existingIndex < 0)
  {
    receivedLeagueCount++;
    if (receivedLeagueCount == 1)
    {
      currentReceivedLeagueIndex = 0;
      currentReceivedGameIndex = 0;
    }
  }
  else if (replacingActive)
  {
    bool restoredByEventId = false;
    if (previousEventId[0] != '\0')
    {
      for (uint8_t index = 0; index < gameCount; index++)
      {
        if (strcmp(targetGames[index].eventId, previousEventId) == 0)
        {
          currentReceivedGameIndex = index;
          restoredByEventId = true;
          break;
        }
      }
    }
    if (!restoredByEventId)
    {
      currentReceivedGameIndex = previousGameIndex < gameCount
        ? previousGameIndex
        : gameCount - 1;
    }
  }
  return true;
}


bool GameManager::setReceivedGolfLeaderboard(
  const char *league,
  const char *tournamentId,
  const char *tournamentName,
  const GolfLeaderboardRow *golfers,
  uint8_t golferCount
)
{
  if (golferCount == 0 || golferCount > MAX_RECEIVED_GOLFERS)
  {
    return false;
  }
  int8_t existingIndex = findReceivedLeague(league);
  if (existingIndex < 0 && receivedLeagueCount >= MAX_RECEIVED_LEAGUES)
  {
    return false;
  }
  uint8_t targetIndex = existingIndex >= 0
    ? static_cast<uint8_t>(existingIndex)
    : receivedLeagueCount;
  const bool replacingActive = existingIndex >= 0 &&
    targetIndex == currentReceivedLeagueIndex;
  const uint8_t previousPageIndex = currentReceivedGameIndex;
  ReceivedLeague &target = receivedLeagues[targetIndex];
  releaseLargeSlateIfOwnedBy(targetIndex);
  strlcpy(target.name, league, sizeof(target.name));
  strlcpy(target.tournamentId, tournamentId, sizeof(target.tournamentId));
  strlcpy(target.tournamentName, tournamentName, sizeof(target.tournamentName));
  for (uint8_t index = 0; index < golferCount; index++)
  {
    target.golfers[index] = golfers[index];
  }
  target.golferCount = golferCount;
  target.contentType = RECEIVED_GOLF;
  if (existingIndex < 0)
  {
    receivedLeagueCount++;
    if (receivedLeagueCount == 1)
    {
      currentReceivedLeagueIndex = 0;
      currentReceivedGameIndex = 0;
    }
  }
  else if (replacingActive)
  {
    const uint8_t pageCount =
      (golferCount + GOLFERS_PER_PAGE - 1) / GOLFERS_PER_PAGE;
    currentReceivedGameIndex = previousPageIndex < pageCount
      ? previousPageIndex
      : pageCount - 1;
  }
  return true;
}


bool GameManager::setReceivedFantasyMatchup(const FantasyMatchupData &matchup)
{
  const char *category = "FANTASY";
  int8_t existingIndex = findReceivedLeague(category);
  if (existingIndex < 0 && receivedLeagueCount >= MAX_RECEIVED_LEAGUES)
  {
    return false;
  }
  uint8_t targetIndex = existingIndex >= 0
    ? static_cast<uint8_t>(existingIndex)
    : receivedLeagueCount;
  ReceivedLeague &target = receivedLeagues[targetIndex];
  releaseLargeSlateIfOwnedBy(targetIndex);
  strlcpy(target.name, category, sizeof(target.name));
  target.contentType = RECEIVED_FANTASY;
  target.gameCount = 1;
  target.fantasyMatchup = matchup;
  if (existingIndex < 0)
  {
    receivedLeagueCount++;
    if (receivedLeagueCount == 1)
    {
      currentReceivedLeagueIndex = 0;
      currentReceivedGameIndex = 0;
    }
  }
  return true;
}


bool GameManager::clearReceivedFantasyMatchup()
{
  int8_t index = findReceivedLeague("FANTASY");
  if (index < 0)
  {
    return false;
  }
  uint8_t removed = static_cast<uint8_t>(index);
  releaseLargeSlateIfOwnedBy(removed);
  for (uint8_t cursor = removed; cursor + 1 < receivedLeagueCount; cursor++)
  {
    receivedLeagues[cursor] = receivedLeagues[cursor + 1];
  }
  receivedLeagues[receivedLeagueCount - 1] = {};
  receivedLeagueCount--;
  if (largeSlateOwnerIndex > static_cast<int8_t>(removed))
  {
    largeSlateOwnerIndex--;
  }
  if (receivedLeagueCount == 0)
  {
    currentReceivedLeagueIndex = 0;
  }
  else if (currentReceivedLeagueIndex >= receivedLeagueCount)
  {
    currentReceivedLeagueIndex = receivedLeagueCount - 1;
  }
  else if (removed < currentReceivedLeagueIndex)
  {
    currentReceivedLeagueIndex--;
  }
  currentReceivedGameIndex = 0;
  return true;
}


/**
 * Move to the next game in the current league.
 */
void GameManager::nextGame()
{
  if (receivedLeagueCount > 0)
  {
    const ReceivedLeague &league = receivedLeagues[currentReceivedLeagueIndex];
    if (league.contentType == RECEIVED_FANTASY)
    {
      return;
    }
    if (league.contentType == RECEIVED_GOLF)
    {
      uint8_t pageCount =
        (league.golferCount + GOLFERS_PER_PAGE - 1) / GOLFERS_PER_PAGE;
      currentReceivedGameIndex = (currentReceivedGameIndex + 1) % pageCount;
      return;
    }
    currentReceivedGameIndex =
      (currentReceivedGameIndex + 1) % league.gameCount;
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
  if (receivedLeagueCount > 0)
  {
    const ReceivedLeague &league = receivedLeagues[currentReceivedLeagueIndex];
    currentReceivedGameIndex = currentReceivedGameIndex == 0
      ? league.gameCount - 1
      : currentReceivedGameIndex - 1;
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
  if (receivedLeagueCount > 0)
  {
    currentReceivedLeagueIndex =
      (currentReceivedLeagueIndex + 1) % receivedLeagueCount;
    currentReceivedGameIndex = 0;
    return;
  }
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
  if (receivedLeagueCount > 0)
  {
    currentReceivedLeagueIndex = currentReceivedLeagueIndex == 0
      ? receivedLeagueCount - 1
      : currentReceivedLeagueIndex - 1;
    currentReceivedGameIndex = 0;
    return;
  }
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
  if (receivedLeagueCount > 0)
  {
    return receivedLeagues[currentReceivedLeagueIndex].name;
  }

  return getCurrentLeague().name;
}


uint8_t GameManager::getCurrentGameNumber()
{
  if (receivedLeagueCount > 0)
  {
    return currentReceivedGameIndex + 1;
  }

  getCurrentGame();
  return currentGameIndex + 1;
}


uint8_t GameManager::getCurrentGameCount()
{
  if (receivedLeagueCount > 0)
  {
    return receivedLeagues[currentReceivedLeagueIndex].contentType == RECEIVED_FANTASY
      ? 1
      : receivedLeagues[currentReceivedLeagueIndex].gameCount;
  }

  return getCurrentLeague().gameCount;
}


uint8_t GameManager::getCurrentLeagueNumber()
{
  if (receivedLeagueCount > 0)
  {
    return currentReceivedLeagueIndex + 1;
  }

  getCurrentLeague();
  return currentLeagueIndex + 1;
}


uint8_t GameManager::getLeagueCount()
{
  if (receivedLeagueCount > 0)
  {
    return receivedLeagueCount;
  }

  return MOCK_LEAGUE_COUNT;
}


bool GameManager::isCurrentLeagueGolf()
{
  return receivedLeagueCount > 0 &&
    receivedLeagues[currentReceivedLeagueIndex].contentType == RECEIVED_GOLF;
}


bool GameManager::isCurrentLeagueFantasy()
{
  return receivedLeagueCount > 0 &&
    receivedLeagues[currentReceivedLeagueIndex].contentType == RECEIVED_FANTASY;
}


const FantasyMatchupData *GameManager::getCurrentFantasyMatchup()
{
  return isCurrentLeagueFantasy()
    ? &receivedLeagues[currentReceivedLeagueIndex].fantasyMatchup
    : nullptr;
}


const char *GameManager::getCurrentTournamentName()
{
  return isCurrentLeagueGolf()
    ? receivedLeagues[currentReceivedLeagueIndex].tournamentName
    : "";
}


const GolfLeaderboardRow *GameManager::getCurrentGolfPageRows()
{
  if (!isCurrentLeagueGolf())
  {
    return nullptr;
  }
  return &receivedLeagues[currentReceivedLeagueIndex]
    .golfers[currentReceivedGameIndex * GOLFERS_PER_PAGE];
}


uint8_t GameManager::getCurrentGolfPageRowCount()
{
  if (!isCurrentLeagueGolf())
  {
    return 0;
  }
  const ReceivedLeague &league = receivedLeagues[currentReceivedLeagueIndex];
  uint8_t start = currentReceivedGameIndex * GOLFERS_PER_PAGE;
  uint8_t remaining = league.golferCount - start;
  return remaining > GOLFERS_PER_PAGE ? GOLFERS_PER_PAGE : remaining;
}


uint8_t GameManager::getCurrentGolfPageNumber()
{
  return isCurrentLeagueGolf() ? currentReceivedGameIndex + 1 : 0;
}


uint8_t GameManager::getCurrentGolfPageCount()
{
  if (!isCurrentLeagueGolf())
  {
    return 0;
  }
  return (
    receivedLeagues[currentReceivedLeagueIndex].golferCount +
    GOLFERS_PER_PAGE - 1
  ) / GOLFERS_PER_PAGE;
}
