#include <cassert>
#include <cmath>
#include <cstring>

#include "fantasy_projection.h"

int main()
{
  JsonDocument legacyDocument;
  assert(!deserializeJson(
    legacyDocument,
    R"({"version":1,"type":"fantasy_matchup","userScore":71.4,"opponentScore":83.2})"
  ));
  const JsonObjectConst legacyPacket = legacyDocument.as<JsonObjectConst>();
  assert(!readOptionalFantasyProjection(
    legacyPacket,
    "userProjectedScore"
  ).hasValue);
  assert(!readOptionalFantasyProjection(
    legacyPacket,
    "opponentProjectedScore"
  ).hasValue);

  JsonDocument projectedDocument;
  assert(!deserializeJson(
    projectedDocument,
    R"({"version":1,"type":"fantasy_matchup","userScore":71.4,"userProjectedScore":126.7,"opponentScore":83.2,"opponentProjectedScore":119.4})"
  ));
  const JsonObjectConst projectedPacket =
    projectedDocument.as<JsonObjectConst>();
  const auto projectedUser = readOptionalFantasyProjection(
    projectedPacket,
    "userProjectedScore"
  );
  const auto projectedOpponent = readOptionalFantasyProjection(
    projectedPacket,
    "opponentProjectedScore"
  );
  assert(
    projectedUser.hasValue &&
    std::fabs(projectedUser.value - 126.7f) < 0.001f
  );
  assert(
    projectedOpponent.hasValue &&
    std::fabs(projectedOpponent.value - 119.4f) < 0.001f
  );

  JsonDocument malformedDocument;
  assert(!deserializeJson(
    malformedDocument,
    R"({"userProjectedScore":"126.7","opponentProjectedScore":10000.1})"
  ));
  const JsonObjectConst malformedPacket =
    malformedDocument.as<JsonObjectConst>();
  assert(!readOptionalFantasyProjection(
    malformedPacket,
    "userProjectedScore"
  ).hasValue);
  assert(!readOptionalFantasyProjection(
    malformedPacket,
    "opponentProjectedScore"
  ).hasValue);

  const auto missing = parseOptionalFantasyProjection(false, false, 0);
  assert(!missing.hasValue);

  const auto malformed = parseOptionalFantasyProjection(true, false, 0);
  assert(!malformed.hasValue);

  const auto nonfinite = parseOptionalFantasyProjection(
    true,
    true,
    INFINITY
  );
  assert(!nonfinite.hasValue);

  const auto outOfRange = parseOptionalFantasyProjection(true, true, 10000.1f);
  assert(!outOfRange.hasValue);

  const auto zero = parseOptionalFantasyProjection(true, true, 0);
  assert(zero.hasValue && zero.value == 0);

  const auto projected = parseOptionalFantasyProjection(true, true, 126.7f);
  assert(projected.hasValue && projected.value == 126.7f);

  char formatted[20];
  formatFantasyProjection(formatted, sizeof(formatted), false, 0);
  assert(std::strcmp(formatted, "PROJ --") == 0);
  formatFantasyProjection(formatted, sizeof(formatted), true, 126.7f);
  assert(std::strcmp(formatted, "PROJ 126.7") == 0);
  return 0;
}
