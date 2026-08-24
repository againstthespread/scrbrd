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
  static const uint8_t MAX_RECEIVED_SLATE_GAMES = 72;
  static const uint8_t MAX_RECEIVED_GOLFERS = 50;
  static const uint8_t GOLFERS_PER_PAGE = 5;
  static const uint8_t MAX_RECEIVED_LEAGUES = 8;

  const GameData &getCurrentGame();
  bool hasReceivedContent() const;

  bool setReceivedGame(
    const char *league,
    const char *away,
    const char *home,
    uint8_t awayScore,
    uint8_t homeScore,
    const char *status,
    const char *clock
  );
  bool setReceivedSlate(
    const char *league,
    const GameData *games,
    uint8_t gameCount
  );
  bool setReceivedGolfLeaderboard(
    const char *league,
    const char *tournamentId,
    const char *tournamentName,
    const GolfLeaderboardRow *golfers,
    uint8_t golferCount
  );
  bool setReceivedFantasyMatchup(const FantasyMatchupData &matchup);
  bool clearReceivedFantasyMatchup();
  void clearReceivedContent();

  void nextGame();
  void previousGame();
  void nextLeague();
  void previousLeague();

  const char *getCurrentLeagueName();
  uint8_t getCurrentGameNumber();
  uint8_t getCurrentGameCount();
  uint8_t getCurrentLeagueNumber();
  uint8_t getLeagueCount();
  bool isCurrentLeagueGolf();
  bool isCurrentLeagueFantasy();
  const FantasyMatchupData *getCurrentFantasyMatchup();
  const char *getCurrentTournamentName();
  const GolfLeaderboardRow *getCurrentGolfPageRows();
  uint8_t getCurrentGolfPageRowCount();
  uint8_t getCurrentGolfPageNumber();
  uint8_t getCurrentGolfPageCount();

private:
  struct ReceivedLeague
  {
    char name[13];
    ReceivedLeagueContentType contentType;
    GameData games[MAX_RECEIVED_SLATE_GAMES];
    uint8_t gameCount;
    char tournamentId[49];
    char tournamentName[49];
    GolfLeaderboardRow golfers[MAX_RECEIVED_GOLFERS];
    uint8_t golferCount;
    FantasyMatchupData fantasyMatchup;
  };

  uint8_t currentLeagueIndex = 0;
  uint8_t currentGameIndex = 0;
  ReceivedLeague receivedLeagues[MAX_RECEIVED_LEAGUES] = {};
  uint8_t receivedLeagueCount = 0;
  uint8_t currentReceivedLeagueIndex = 0;
  uint8_t currentReceivedGameIndex = 0;

  const LeagueData &getCurrentLeague();
  int8_t findReceivedLeague(const char *league);
};

#endif
