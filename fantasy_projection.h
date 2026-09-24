#ifndef FANTASY_PROJECTION_H
#define FANTASY_PROJECTION_H

#include <ArduinoJson.h>
#include <cmath>
#include <cstddef>
#include <cstdio>

struct OptionalFantasyProjection
{
  bool hasValue;
  float value;
};

inline OptionalFantasyProjection parseOptionalFantasyProjection(
  bool present,
  bool numeric,
  float value
)
{
  if (!present || !numeric || !std::isfinite(value) || std::fabs(value) > 10000)
  {
    return { false, 0 };
  }
  return { true, value };
}

inline OptionalFantasyProjection readOptionalFantasyProjection(
  JsonObjectConst packet,
  const char *fieldName
)
{
  JsonVariantConst field = packet[fieldName];
  const bool numeric = field.is<float>();
  return parseOptionalFantasyProjection(
    !field.isNull(),
    numeric,
    numeric ? field.as<float>() : 0
  );
}

inline void formatFantasyProjection(
  char *buffer,
  std::size_t bufferSize,
  bool hasValue,
  float value
)
{
  if (!hasValue)
  {
    std::snprintf(buffer, bufferSize, "PROJ --");
    return;
  }
  std::snprintf(buffer, bufferSize, "PROJ %.1f", value);
}

#endif
