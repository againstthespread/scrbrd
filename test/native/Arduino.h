#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H

#include <cstddef>
#include <cstdint>
#include <cstring>

inline std::size_t strlcpy(char *destination, const char *source, std::size_t size)
{
  const std::size_t sourceLength = std::strlen(source);
  if (size > 0)
  {
    const std::size_t copied = sourceLength >= size ? size - 1 : sourceLength;
    std::memcpy(destination, source, copied);
    destination[copied] = '\0';
  }
  return sourceLength;
}

#endif
