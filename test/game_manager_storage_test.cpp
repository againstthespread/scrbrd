#include <cassert>
#include <cstdio>
#include <cstring>

#define private public
#include "GameManager.h"
#undef private

static void makeGames(GameData *games, uint8_t count, uint16_t firstId = 0)
{
  for (uint8_t index = 0; index < count; index++)
  {
    games[index] = {};
    std::snprintf(
      games[index].eventId,
      sizeof(games[index].eventId),
      "event-%u",
      firstId + index
    );
    std::snprintf(games[index].awayTeam, sizeof(games[index].awayTeam), "A%u", index);
    std::snprintf(games[index].homeTeam, sizeof(games[index].homeTeam), "H%u", index);
  }
}

static void selectEvent(GameManager &manager, uint8_t index)
{
  while (manager.getCurrentGameNumber() - 1 != index)
  {
    manager.nextGame();
  }
}

static FantasyMatchupData makeFantasyMatchup(const char *identity)
{
  FantasyMatchupData matchup = {};
  std::snprintf(matchup.identity, sizeof(matchup.identity), "%s", identity);
  std::snprintf(matchup.leagueName, sizeof(matchup.leagueName), "%s", identity);
  matchup.hasUserProjectedScore = true;
  matchup.userProjectedScore = 126.7f;
  matchup.hasOpponentProjectedScore = true;
  matchup.opponentProjectedScore = 119.4f;
  return matchup;
}

int main()
{
  GameData games[GameManager::MAX_LARGE_SLATE_GAMES] = {};
  GameManager manager;

  const uint8_t acceptedCounts[] = {1, 20, 21, 40, 67, 72};
  for (uint8_t count : acceptedCounts)
  {
    manager.clearReceivedContent();
    makeGames(games, count);
    assert(manager.setReceivedSlate("NCAAF", games, count));
    assert(manager.getCurrentGameCount() == count);
    assert(manager.receivedLeagues[0].usesLargeSlateStorage ==
      (count > GameManager::MAX_STANDARD_SLATE_GAMES));
    assert((manager.gamesForReceivedLeague(0) == manager.largeSlateGames) ==
      (count > GameManager::MAX_STANDARD_SLATE_GAMES));
    for (uint8_t index = 0; index < count; index++)
    {
      assert(std::strcmp(manager.getCurrentGame().eventId, games[index].eventId) == 0);
      manager.nextGame();
    }
    assert(manager.getCurrentGameNumber() == 1);
  }

  makeGames(games, 72);
  assert(!manager.setReceivedSlate("NCAAF", games, 73));

  manager.clearReceivedContent();
  makeGames(games, 35);
  assert(manager.setReceivedSlate("NCAAF", games, 35));
  selectEvent(manager, 10);
  const uint8_t transitionCounts[] = {42, 40, 18, 35};
  for (uint8_t count : transitionCounts)
  {
    makeGames(games, count);
    assert(manager.setReceivedSlate("NCAAF", games, count));
    assert(std::strcmp(manager.getCurrentGame().eventId, "event-10") == 0);
    assert(manager.receivedLeagues[0].usesLargeSlateStorage == (count > 20));
    assert((manager.largeSlateOwnerIndex == 0) == (count > 20));
  }

  GameData otherGames[21] = {};
  makeGames(otherGames, 21, 100);
  assert(!manager.setReceivedSlate("OTHER", otherGames, 21));
  assert(std::strstr(manager.getLastSlateError(), "already owned") != nullptr);
  assert(manager.getLeagueCount() == 1);
  assert(manager.getCurrentGameCount() == 35);
  assert(std::strcmp(manager.getCurrentGame().eventId, "event-10") == 0);

  manager.clearReceivedContent();
  FantasyMatchupData matchup = {};
  assert(manager.setReceivedFantasyMatchup(matchup));
  makeGames(games, 67);
  assert(manager.setReceivedSlate("NCAAF", games, 67));
  assert(manager.largeSlateOwnerIndex == 1);
  assert(manager.clearReceivedFantasyMatchup());
  assert(manager.largeSlateOwnerIndex == 0);
  assert(manager.getCurrentGameCount() == 67);
  assert(std::strcmp(manager.getCurrentGame().eventId, "event-0") == 0);

  manager.clearReceivedContent();
  assert(manager.beginFantasySlate());
  assert(manager.stageFantasyMatchup(makeFantasyMatchup("fantasy-1")));
  assert(manager.stageFantasyMatchup(makeFantasyMatchup("fantasy-2")));
  assert(manager.stageFantasyMatchup(makeFantasyMatchup("fantasy-3")));
  assert(manager.commitFantasySlate());
  makeGames(games, 2);
  assert(manager.setReceivedSlate("NFL", games, 2));
  assert(manager.setReceivedSlate("NBA", games, 2));
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-1") == 0);
  assert(manager.getCurrentFantasyMatchup()->hasUserProjectedScore);
  assert(manager.getCurrentFantasyMatchup()->userProjectedScore == 126.7f);
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-2") == 0);
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-3") == 0);
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-1") == 0);
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-2") == 0);
  assert(!manager.setReceivedFantasyMatchup(makeFantasyMatchup("legacy")));
  assert(manager.fantasySlateCount == 3);
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-2") == 0);
  manager.nextLeague();
  assert(std::strcmp(manager.getCurrentLeagueName(), "NFL") == 0);
  assert(std::strcmp(manager.getCurrentGame().eventId, "event-0") == 0);
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentGame().eventId, "event-1") == 0);
  manager.nextLeague();
  assert(std::strcmp(manager.getCurrentLeagueName(), "NBA") == 0);
  manager.nextLeague();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-1") == 0);

  manager.clearReceivedContent();
  assert(manager.setReceivedFantasyMatchup(makeFantasyMatchup("fantasy-only")));
  assert(manager.setReceivedSlate("NBA", games, 1));
  manager.nextGame();
  assert(std::strcmp(manager.getCurrentFantasyMatchup()->identity, "fantasy-only") == 0);
  manager.nextLeague();
  assert(std::strcmp(manager.getCurrentLeagueName(), "NBA") == 0);

  manager.clearReceivedContent();
  assert(manager.beginFantasySlate());
  assert(manager.commitFantasySlate());
  assert(!manager.hasReceivedContent());

  manager.clearReceivedContent();
  assert(manager.largeSlateOwnerIndex == -1);
  assert(!manager.hasReceivedContent());

  std::puts("GameManager storage tests passed");
  return 0;
}
