/**
 * Peter's Sports Hub
 *
 * Hardware:
 * ESP32-S3 + ST7789 TFT
 * 240 x 280 display
 *
 * Stage 1:
 * Static sports dashboard
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Arduino_GFX_Library.h"
#include "BluetoothManager.h"
#include "GameManager.h"
#include "pin_config.h"
#include "HWCDC.h"

// USB serial connection for debugging
HWCDC USBSerial;

// Display hardware
Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC,
  LCD_CS,
  LCD_SCK,
  LCD_MOSI
);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  0,
  true,
  LCD_WIDTH,
  LCD_HEIGHT,
  0,
  20,
  0,
  0
);

// Custom RGB565 colors
const uint16_t COLOR_NAVY = 0x0010;
const uint16_t COLOR_DARK_BLUE = 0x0211;
const uint16_t COLOR_LIGHT_BLUE = 0x5D7F;
const uint16_t COLOR_GRAY = 0x8410;
const uint16_t COLOR_DARK_GRAY = 0x3186;
const uint16_t COLOR_STATUS_BG = 0x03E0;

const uint8_t BOOT_BUTTON_PIN = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long DOUBLE_CLICK_WINDOW_MS = 300;

GameManager gameManager;
BluetoothManager bluetoothManager;

bool lastBootButtonReading = HIGH;
bool stableBootButtonState = HIGH;
bool bootButtonWasPressed = false;
unsigned long lastBootButtonChangeTime = 0;
uint8_t pendingClickCount = 0;
unsigned long lastClickTime = 0;
bool lastDisplayedBluetoothConnected = false;

// Chunked slate staging is deliberately separate from GameManager's active
// slate so incomplete transfers can never disturb the displayed games.
const unsigned long SLATE_TRANSFER_TIMEOUT_MS = 30000;
const uint8_t MAX_LEGACY_SLATE_GAMES = 4;
bool slateTransferActive = false;
char stagingSlateId[49] = "";
char stagingLeague[13] = "";
GameData stagingGames[GameManager::MAX_RECEIVED_SLATE_GAMES] = {};
uint8_t stagingExpectedGames = 0;
uint8_t stagingExpectedChunks = 0;
uint8_t stagingReceivedGames = 0;
uint8_t stagingNextChunkIndex = 0;
unsigned long stagingLastActivityTime = 0;

bool golfTransferActive = false;
char stagingGolfTransferId[49] = "";
char stagingGolfTournamentId[49] = "";
char stagingGolfTournamentName[49] = "";
GolfLeaderboardRow stagingGolfers[GameManager::MAX_RECEIVED_GOLFERS] = {};
uint8_t stagingExpectedGolfers = 0;
uint8_t stagingExpectedGolfChunks = 0;
uint8_t stagingReceivedGolfers = 0;
uint8_t stagingNextGolfChunkIndex = 0;
unsigned long stagingGolfLastActivityTime = 0;


/**
 * Prepare the LCD and backlight.
 */
void initializeDisplay()
{
  if (!gfx->begin())
  {
    USBSerial.println("Display initialization failed!");
    return;
  }

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  gfx->fillScreen(BLACK);

  USBSerial.println("Display initialized.");
}


/**
 * Draw the startup screen.
 */
void drawSplashScreen()
{
  gfx->fillScreen(COLOR_NAVY);

  // Decorative border
  gfx->drawRoundRect(
    8,
    8,
    gfx->width() - 16,
    gfx->height() - 16,
    12,
    COLOR_LIGHT_BLUE
  );

  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(48, 82);
  gfx->println("PETER'S");

  gfx->setTextColor(CYAN);
  gfx->setCursor(32, 120);
  gfx->println("SPORTS HUB");

  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(87, 244);
  gfx->println("Version 0.1");
}


/**
 * Draw text with size 1 if the team name is long.
 */
void drawTeamName(const char *teamName, int16_t x, int16_t y)
{
  uint8_t nameLength = strlen(teamName);
  uint8_t textSize = 2;
  uint8_t maxChars = 11;

  if (nameLength > maxChars)
  {
    textSize = 1;
    maxChars = 21;
    y += 5;
  }

  gfx->setTextColor(WHITE);
  gfx->setTextSize(textSize);
  gfx->setCursor(x, y);

  for (uint8_t i = 0; i < nameLength && i < maxChars; i++)
  {
    if (i == maxChars - 1 && nameLength > maxChars)
    {
      gfx->print(".");
    }
    else
    {
      gfx->print(teamName[i]);
    }
  }
}


/**
 * Draw a score aligned to the right side of the card.
 */
void drawScore(uint8_t score, uint16_t color, int16_t y)
{
  int16_t x = 183;

  if (score >= 100)
  {
    x = 165;
  }
  else if (score < 10)
  {
    x = 201;
  }

  gfx->setTextColor(color);
  gfx->setTextSize(3);
  gfx->setCursor(x, y);
  gfx->println(score);
}


/**
 * Draw a compact Bluetooth status glyph using only display primitives.
 */
void drawBluetoothIcon(int16_t x, int16_t y)
{
  uint16_t color = bluetoothManager.isBluetoothConnected()
    ? CYAN
    : COLOR_GRAY;

  gfx->drawLine(x + 5, y, x + 5, y + 18, color);
  gfx->drawLine(x + 5, y, x + 11, y + 6, color);
  gfx->drawLine(x + 11, y + 6, x + 1, y + 13, color);
  gfx->drawLine(x + 1, y + 5, x + 11, y + 12, color);
  gfx->drawLine(x + 11, y + 12, x + 5, y + 18, color);

  if (bluetoothManager.isBluetoothConnected())
  {
    gfx->fillTriangle(x + 6, y + 2, x + 10, y + 6, x + 6, y + 9, color);
    gfx->fillTriangle(x + 6, y + 9, x + 10, y + 12, x + 6, y + 16, color);
  }
}


/**
 * Draw the application's compact top title area.
 */
void drawHeader()
{
  gfx->fillRoundRect(10, 8, 220, 42, 8, COLOR_DARK_BLUE);
  gfx->drawRoundRect(10, 8, 220, 42, 8, COLOR_LIGHT_BLUE);

  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(18, 14);
  gfx->println("PETER'S SPORTS HUB");

  gfx->setTextColor(CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(18, 29);
  gfx->println(gameManager.getCurrentLeagueName());

  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(156, 33);
  gfx->print("L ");
  gfx->print(gameManager.getCurrentLeagueNumber());
  gfx->print("/");
  gfx->println(gameManager.getLeagueCount());

  drawBluetoothIcon(208, 18);
}


void drawBase(int16_t centerX, int16_t centerY, bool occupied)
{
  const int16_t radius = 4;
  uint16_t color = occupied ? YELLOW : COLOR_GRAY;
  if (occupied)
  {
    gfx->fillTriangle(
      centerX,
      centerY - radius,
      centerX - radius,
      centerY,
      centerX + radius,
      centerY,
      color
    );
    gfx->fillTriangle(
      centerX,
      centerY + radius,
      centerX - radius,
      centerY,
      centerX + radius,
      centerY,
      color
    );
    return;
  }
  gfx->drawLine(centerX, centerY - radius, centerX + radius, centerY, color);
  gfx->drawLine(centerX + radius, centerY, centerX, centerY + radius, color);
  gfx->drawLine(centerX, centerY + radius, centerX - radius, centerY, color);
  gfx->drawLine(centerX - radius, centerY, centerX, centerY - radius, color);
}


void drawBaseballSituation(const GameData &game)
{
  // Compact 38x34 MLB-only region in the scoreboard card's lower-right:
  // x=174..212, y=164..198. It stays below the status badge and away from
  // the header Bluetooth icon.
  drawBase(193, 168, game.runnerOnSecond);
  drawBase(183, 178, game.runnerOnThird);
  drawBase(203, 178, game.runnerOnFirst);

  for (uint8_t index = 0; index < 2; index++)
  {
    int16_t x = 188 + index * 11;
    if (index < game.outs)
    {
      gfx->fillCircle(x, 194, 3, RED);
    }
    else
    {
      gfx->drawCircle(x, 194, 3, COLOR_GRAY);
    }
  }
}


/**
 * Draw a scoreboard from game data.
 */
void drawScoreboard(const GameData &game)
{
  // Scoreboard card
  gfx->fillRoundRect(10, 62, 220, 146, 10, COLOR_DARK_GRAY);
  gfx->drawRoundRect(10, 62, 220, 146, 10, COLOR_LIGHT_BLUE);

  // Game position
  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(22, 75);
  gfx->print("Game ");
  gfx->print(gameManager.getCurrentGameNumber());
  gfx->print(" of ");
  gfx->println(gameManager.getCurrentGameCount());

  // Status badge
  gfx->fillRoundRect(157, 71, 54, 18, 5, COLOR_STATUS_BG);
  gfx->setTextColor(BLACK);
  gfx->setTextSize(1);
  gfx->setCursor(167, 77);
  gfx->println(game.status);

  // Divider
  gfx->drawFastHLine(20, 97, 200, COLOR_GRAY);

  // Teams
  drawTeamName(game.awayTeam, 22, 112);
  drawTeamName(game.homeTeam, 22, 144);

  // Scores
  drawScore(game.awayScore, GREEN, 107);
  drawScore(game.homeScore, WHITE, 139);

  // Game clock
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(1);
  gfx->setCursor(94, 176);
  gfx->println(game.clock);

  if (strcmp(gameManager.getCurrentLeagueName(), "MLB") == 0 &&
      strcmp(game.status, "LIVE") == 0)
  {
    drawBaseballSituation(game);
  }
}


void drawGolfLeaderboard()
{
  gfx->fillRoundRect(10, 58, 220, 210, 10, COLOR_DARK_GRAY);
  gfx->drawRoundRect(10, 58, 220, 210, 10, COLOR_LIGHT_BLUE);

  gfx->setTextColor(CYAN);
  gfx->setTextSize(1);
  gfx->setCursor(18, 68);
  String tournament = gameManager.getCurrentTournamentName();
  if (tournament.length() > 28)
  {
    tournament = tournament.substring(0, 27) + "~";
  }
  gfx->println(tournament);
  gfx->drawFastHLine(18, 82, 204, COLOR_GRAY);

  const GolfLeaderboardRow *rows = gameManager.getCurrentGolfPageRows();
  uint8_t rowCount = gameManager.getCurrentGolfPageRowCount();
  for (uint8_t index = 0; index < rowCount; index++)
  {
    int16_t y = 91 + index * 31;
    gfx->setTextColor(WHITE);
    gfx->setCursor(18, y);
    gfx->print(rows[index].rank);
    gfx->setCursor(43, y);
    String name = rows[index].name;
    if (name.length() > 18)
    {
      name = name.substring(0, 17) + "~";
    }
    gfx->print(name);
    gfx->setTextColor(YELLOW);
    gfx->setCursor(190, y);
    gfx->println(rows[index].score);

    if (rows[index].detail[0] != '\0')
    {
      gfx->setTextColor(COLOR_GRAY);
      gfx->setCursor(43, y + 11);
      gfx->println(rows[index].detail);
    }
  }

  gfx->setTextColor(COLOR_GRAY);
  gfx->setCursor(164, 251);
  gfx->print("Page ");
  gfx->print(gameManager.getCurrentGolfPageNumber());
  gfx->print("/");
  gfx->println(gameManager.getCurrentGolfPageCount());
}


/**
 * Draw the complete dashboard.
 */
void drawDashboard()
{
  gfx->fillScreen(BLACK);

  drawHeader();
  if (gameManager.isCurrentLeagueGolf())
  {
    drawGolfLeaderboard();
  }
  else
  {
    drawScoreboard(gameManager.getCurrentGame());
  }
}


bool isValidUtf8(const String &text)
{
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(text.c_str());
  size_t index = 0;

  while (index < text.length())
  {
    uint8_t first = bytes[index++];

    if (first <= 0x7F)
    {
      continue;
    }

    uint8_t continuationCount;
    uint32_t codePoint;

    if ((first & 0xE0) == 0xC0)
    {
      continuationCount = 1;
      codePoint = first & 0x1F;
    }
    else if ((first & 0xF0) == 0xE0)
    {
      continuationCount = 2;
      codePoint = first & 0x0F;
    }
    else if ((first & 0xF8) == 0xF0)
    {
      continuationCount = 3;
      codePoint = first & 0x07;
    }
    else
    {
      return false;
    }

    if (index + continuationCount > text.length())
    {
      return false;
    }

    for (uint8_t i = 0; i < continuationCount; i++)
    {
      uint8_t next = bytes[index++];
      if ((next & 0xC0) != 0x80)
      {
        return false;
      }
      codePoint = (codePoint << 6) | (next & 0x3F);
    }

    if ((continuationCount == 1 && codePoint < 0x80) ||
        (continuationCount == 2 && codePoint < 0x800) ||
        (continuationCount == 3 && codePoint < 0x10000) ||
        codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF))
    {
      return false;
    }
  }

  return true;
}


void rejectGamePacket(const char *reason)
{
  USBSerial.print("BLE game rejected: ");
  USBSerial.println(reason);
}


bool readRequiredText(
  JsonObjectConst packet,
  const char *key,
  size_t maximumLength,
  const char *&value
)
{
  JsonVariantConst field = packet[key];
  if (!field.is<const char *>())
  {
    rejectGamePacket(key);
    return false;
  }

  value = field.as<const char *>();
  size_t length = strlen(value);
  if (length == 0 || length > maximumLength)
  {
    rejectGamePacket(key);
    return false;
  }

  return true;
}


bool readValidatedGame(JsonObjectConst packet, GameData &game)
{
  const char *away;
  const char *home;
  const char *status;
  const char *clock;
  if (!readRequiredText(packet, "away", 32, away) ||
      !readRequiredText(packet, "home", 32, home) ||
      !readRequiredText(packet, "status", 8, status) ||
      !readRequiredText(packet, "clock", 24, clock))
  {
    return false;
  }

  if (strcmp(status, "UPCOMING") != 0 &&
      strcmp(status, "LIVE") != 0 &&
      strcmp(status, "FINAL") != 0)
  {
    rejectGamePacket("invalid status");
    return false;
  }

  JsonVariantConst awayScore = packet["awayScore"];
  JsonVariantConst homeScore = packet["homeScore"];
  if (!awayScore.is<int>() || !homeScore.is<int>() ||
      awayScore.as<int>() < 0 || awayScore.as<int>() > 255 ||
      homeScore.as<int>() < 0 || homeScore.as<int>() > 255)
  {
    rejectGamePacket("invalid score");
    return false;
  }

  JsonVariantConst eventId = packet["id"];
  if (!eventId.isNull())
  {
    if (!eventId.is<const char *>() ||
        strlen(eventId.as<const char *>()) == 0 ||
        strlen(eventId.as<const char *>()) > 48)
    {
      rejectGamePacket("invalid id");
      return false;
    }
    strlcpy(game.eventId, eventId.as<const char *>(), sizeof(game.eventId));
  }

  strlcpy(game.awayTeam, away, sizeof(game.awayTeam));
  strlcpy(game.homeTeam, home, sizeof(game.homeTeam));
  strlcpy(game.status, status, sizeof(game.status));
  strlcpy(game.clock, clock, sizeof(game.clock));
  game.awayScore = static_cast<uint8_t>(awayScore.as<int>());
  game.homeScore = static_cast<uint8_t>(homeScore.as<int>());

  JsonVariantConst onFirst = packet["onFirst"];
  JsonVariantConst onSecond = packet["onSecond"];
  JsonVariantConst onThird = packet["onThird"];
  JsonVariantConst outs = packet["outs"];
  bool hasAnyBaseballField = !onFirst.isNull() || !onSecond.isNull() ||
    !onThird.isNull() || !outs.isNull();
  if (hasAnyBaseballField)
  {
    if (!onFirst.is<bool>() || !onSecond.is<bool>() ||
        !onThird.is<bool>() || !outs.is<int>() ||
        outs.as<int>() < 0 || outs.as<int>() > 2)
    {
      rejectGamePacket("invalid baseball state");
      return false;
    }
    game.hasBaseballState = true;
    game.runnerOnFirst = onFirst.as<bool>();
    game.runnerOnSecond = onSecond.as<bool>();
    game.runnerOnThird = onThird.as<bool>();
    game.outs = static_cast<uint8_t>(outs.as<int>());
  }
  return true;
}


bool handleGamePacket(const String &message)
{
  if (!isValidUtf8(message))
  {
    rejectGamePacket("invalid UTF-8");
    return false;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(
    document,
    message.c_str(),
    message.length()
  );

  if (error)
  {
    rejectGamePacket(error.c_str());
    return false;
  }

  if (!document.is<JsonObject>())
  {
    rejectGamePacket("root is not an object");
    return false;
  }

  JsonObjectConst packet = document.as<JsonObjectConst>();

  if (!packet["version"].is<int>() || packet["version"].as<int>() != 1)
  {
    rejectGamePacket("unsupported version");
    return false;
  }

  if (!packet["type"].is<const char *>() ||
      strcmp(packet["type"].as<const char *>(), "game") != 0)
  {
    rejectGamePacket("invalid type");
    return false;
  }

  const char *league;
  if (!readRequiredText(packet, "league", 12, league))
  {
    return false;
  }
  GameData game = {};
  if (!readValidatedGame(packet, game))
  {
    return false;
  }
  if (!gameManager.setReceivedSlate(league, &game, 1))
  {
    rejectGamePacket("received league capacity reached");
    return false;
  }
  drawDashboard();
  USBSerial.println("BLE game accepted.");
  return true;
}


bool handleSlatePacket(const String &message)
{
  if (message.length() > 512)
  {
    rejectGamePacket("slate exceeds 512 bytes");
    return false;
  }
  if (!isValidUtf8(message))
  {
    rejectGamePacket("invalid UTF-8");
    return false;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(
    document,
    message.c_str(),
    message.length()
  );
  if (error || !document.is<JsonObject>())
  {
    rejectGamePacket(error ? error.c_str() : "root is not an object");
    return false;
  }

  JsonObjectConst packet = document.as<JsonObjectConst>();
  if (!packet["version"].is<int>() || packet["version"].as<int>() != 1 ||
      !packet["type"].is<const char *>() ||
      strcmp(packet["type"].as<const char *>(), "slate") != 0)
  {
    rejectGamePacket("invalid slate header");
    return false;
  }

  const char *league;
  if (!readRequiredText(packet, "league", 12, league) ||
      !packet["games"].is<JsonArrayConst>())
  {
    rejectGamePacket("invalid games");
    return false;
  }

  JsonArrayConst games = packet["games"].as<JsonArrayConst>();
  if (games.size() == 0 ||
      games.size() > MAX_LEGACY_SLATE_GAMES)
  {
    rejectGamePacket("invalid slate game count");
    return false;
  }

  // Validate into owned temporary storage so a malformed slate never replaces
  // the active received slate.
  GameData validatedGames[GameManager::MAX_RECEIVED_SLATE_GAMES] = {};
  uint8_t index = 0;
  for (JsonVariantConst item : games)
  {
    if (!item.is<JsonObjectConst>() ||
        !readValidatedGame(item.as<JsonObjectConst>(), validatedGames[index]))
    {
      rejectGamePacket("invalid slate game");
      return false;
    }
    index++;
  }

  if (!gameManager.setReceivedSlate(league, validatedGames, index))
  {
    rejectGamePacket("received league capacity reached");
    return false;
  }
  drawDashboard();
  USBSerial.print("BLE slate accepted with ");
  USBSerial.print(index);
  USBSerial.println(" games.");
  return true;
}


void clearStagingSlate(const char *reason)
{
  if (reason != nullptr)
  {
    USBSerial.print("BLE slate transfer discarded: ");
    USBSerial.println(reason);
  }
  slateTransferActive = false;
  stagingSlateId[0] = '\0';
  stagingLeague[0] = '\0';
  stagingExpectedGames = 0;
  stagingExpectedChunks = 0;
  stagingReceivedGames = 0;
  stagingNextChunkIndex = 0;
  stagingLastActivityTime = 0;
}


void expireStagingSlateIfNeeded()
{
  if (slateTransferActive &&
      millis() - stagingLastActivityTime >= SLATE_TRANSFER_TIMEOUT_MS)
  {
    clearStagingSlate("30-second timeout");
  }
}


bool readSlateTransferDocument(
  const String &message,
  const char *expectedType,
  JsonDocument &document,
  JsonObjectConst &packet
)
{
  if (message.length() > 512 || !isValidUtf8(message))
  {
    rejectGamePacket("invalid chunked slate packet size/UTF-8");
    return false;
  }
  DeserializationError error = deserializeJson(
    document,
    message.c_str(),
    message.length()
  );
  if (error || !document.is<JsonObject>())
  {
    rejectGamePacket(error ? error.c_str() : "root is not an object");
    return false;
  }
  packet = document.as<JsonObjectConst>();
  if (!packet["version"].is<int>() || packet["version"].as<int>() != 1 ||
      !packet["type"].is<const char *>() ||
      strcmp(packet["type"].as<const char *>(), expectedType) != 0)
  {
    rejectGamePacket("invalid chunked slate header");
    return false;
  }
  return true;
}


bool handleSlateStartPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "slate_start", document, packet))
  {
    return false;
  }

  const char *league;
  const char *slateId;
  JsonVariantConst totalGames = packet["totalGames"];
  JsonVariantConst totalChunks = packet["totalChunks"];
  if (!readRequiredText(packet, "league", 12, league) ||
      !readRequiredText(packet, "slateId", 48, slateId) ||
      !totalGames.is<int>() || !totalChunks.is<int>() ||
      totalGames.as<int>() < 1 ||
      totalGames.as<int>() > GameManager::MAX_RECEIVED_SLATE_GAMES ||
      totalChunks.as<int>() < 1 ||
      totalChunks.as<int>() > totalGames.as<int>())
  {
    rejectGamePacket("invalid slate_start fields");
    return false;
  }

  clearStagingSlate(slateTransferActive ? "replaced by new slate_start" : nullptr);
  strlcpy(stagingLeague, league, sizeof(stagingLeague));
  strlcpy(stagingSlateId, slateId, sizeof(stagingSlateId));
  stagingExpectedGames = static_cast<uint8_t>(totalGames.as<int>());
  stagingExpectedChunks = static_cast<uint8_t>(totalChunks.as<int>());
  stagingLastActivityTime = millis();
  slateTransferActive = true;
  USBSerial.print("BLE slate transfer started: id=");
  USBSerial.print(stagingSlateId);
  USBSerial.print(", games=");
  USBSerial.print(stagingExpectedGames);
  USBSerial.print(", chunks=");
  USBSerial.println(stagingExpectedChunks);
  return true;
}


bool handleSlateChunkPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "slate_chunk", document, packet))
  {
    return false;
  }
  if (!slateTransferActive)
  {
    rejectGamePacket("slate_chunk without slate_start");
    return false;
  }

  const char *slateId;
  JsonVariantConst chunkIndex = packet["chunkIndex"];
  if (!readRequiredText(packet, "slateId", 48, slateId) ||
      strcmp(slateId, stagingSlateId) != 0)
  {
    rejectGamePacket("wrong slateId");
    return false;
  }
  if (!chunkIndex.is<int>() || chunkIndex.as<int>() < 0 ||
      chunkIndex.as<int>() >= stagingExpectedChunks)
  {
    rejectGamePacket("chunk index out of range");
    return false;
  }
  if (chunkIndex.as<int>() < stagingNextChunkIndex)
  {
    rejectGamePacket("duplicate chunk");
    return false;
  }
  if (chunkIndex.as<int>() != stagingNextChunkIndex)
  {
    rejectGamePacket("out-of-order chunk");
    return false;
  }
  if (!packet["games"].is<JsonArrayConst>())
  {
    rejectGamePacket("invalid chunk games");
    return false;
  }

  JsonArrayConst games = packet["games"].as<JsonArrayConst>();
  if (games.size() == 0 ||
      stagingReceivedGames + games.size() > stagingExpectedGames)
  {
    rejectGamePacket("invalid chunk game count");
    return false;
  }

  GameData validatedGames[GameManager::MAX_RECEIVED_SLATE_GAMES] = {};
  uint8_t validatedCount = 0;
  for (JsonVariantConst item : games)
  {
    if (!item.is<JsonObjectConst>() ||
        !readValidatedGame(
          item.as<JsonObjectConst>(),
          validatedGames[validatedCount]
        ))
    {
      rejectGamePacket("malformed chunk game");
      return false;
    }
    validatedCount++;
  }

  for (uint8_t index = 0; index < validatedCount; index++)
  {
    stagingGames[stagingReceivedGames + index] = validatedGames[index];
  }
  stagingReceivedGames += validatedCount;
  stagingNextChunkIndex++;
  stagingLastActivityTime = millis();
  USBSerial.print("BLE slate chunk accepted: index=");
  USBSerial.print(chunkIndex.as<int>());
  USBSerial.print(", bytes=");
  USBSerial.println(message.length());
  return true;
}


bool handleSlateEndPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "slate_end", document, packet))
  {
    return false;
  }
  if (!slateTransferActive)
  {
    rejectGamePacket("slate_end without slate_start");
    return false;
  }

  const char *slateId;
  if (!readRequiredText(packet, "slateId", 48, slateId) ||
      strcmp(slateId, stagingSlateId) != 0)
  {
    rejectGamePacket("wrong slateId");
    return false;
  }
  if (stagingNextChunkIndex != stagingExpectedChunks ||
      stagingReceivedGames != stagingExpectedGames)
  {
    rejectGamePacket("incomplete slate transfer");
    return false;
  }

  if (!gameManager.setReceivedSlate(
    stagingLeague,
    stagingGames,
    stagingReceivedGames
  ))
  {
    rejectGamePacket("received league capacity reached");
    clearStagingSlate("league capacity reached");
    return false;
  }
  USBSerial.print("BLE slate transfer complete: id=");
  USBSerial.println(stagingSlateId);
  clearStagingSlate(nullptr);
  drawDashboard();
  return true;
}


void clearGolfStaging(const char *reason)
{
  if (reason != nullptr)
  {
    USBSerial.print("BLE golf transfer discarded: ");
    USBSerial.println(reason);
  }
  golfTransferActive = false;
  stagingGolfTransferId[0] = '\0';
  stagingExpectedGolfers = 0;
  stagingExpectedGolfChunks = 0;
  stagingReceivedGolfers = 0;
  stagingNextGolfChunkIndex = 0;
}


void expireGolfStagingIfNeeded()
{
  if (golfTransferActive &&
      millis() - stagingGolfLastActivityTime >= SLATE_TRANSFER_TIMEOUT_MS)
  {
    clearGolfStaging("30-second timeout");
  }
}


bool readGolfRow(JsonObjectConst packet, GolfLeaderboardRow &row)
{
  const char *id;
  const char *name;
  const char *rank;
  const char *score;
  if (!readRequiredText(packet, "id", 48, id) ||
      !readRequiredText(packet, "name", 32, name) ||
      !readRequiredText(packet, "rank", 8, rank) ||
      !readRequiredText(packet, "score", 8, score))
  {
    return false;
  }
  strlcpy(row.playerId, id, sizeof(row.playerId));
  strlcpy(row.name, name, sizeof(row.name));
  strlcpy(row.rank, rank, sizeof(row.rank));
  strlcpy(row.score, score, sizeof(row.score));
  JsonVariantConst detail = packet["detail"];
  if (!detail.isNull())
  {
    if (!detail.is<const char *>() || strlen(detail.as<const char *>()) > 16)
    {
      rejectGamePacket("invalid golf detail");
      return false;
    }
    strlcpy(row.detail, detail.as<const char *>(), sizeof(row.detail));
  }
  return true;
}


bool handleGolfStartPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "golf_start", document, packet))
  {
    return false;
  }
  const char *league;
  const char *transferId;
  const char *tournamentId;
  const char *tournamentName;
  JsonVariantConst totalGolfers = packet["totalGolfers"];
  JsonVariantConst totalChunks = packet["totalChunks"];
  if (!readRequiredText(packet, "league", 12, league) ||
      strcmp(league, "PGA") != 0 ||
      !readRequiredText(packet, "transferId", 48, transferId) ||
      !readRequiredText(packet, "tournamentId", 48, tournamentId) ||
      !readRequiredText(packet, "tournamentName", 48, tournamentName) ||
      !totalGolfers.is<int>() || !totalChunks.is<int>() ||
      totalGolfers.as<int>() < 1 ||
      totalGolfers.as<int>() > GameManager::MAX_RECEIVED_GOLFERS ||
      totalChunks.as<int>() < 1 ||
      totalChunks.as<int>() > totalGolfers.as<int>())
  {
    rejectGamePacket("invalid golf_start fields");
    return false;
  }
  clearGolfStaging(golfTransferActive ? "replaced by new golf_start" : nullptr);
  strlcpy(stagingGolfTransferId, transferId, sizeof(stagingGolfTransferId));
  strlcpy(stagingGolfTournamentId, tournamentId, sizeof(stagingGolfTournamentId));
  strlcpy(
    stagingGolfTournamentName,
    tournamentName,
    sizeof(stagingGolfTournamentName)
  );
  stagingExpectedGolfers = totalGolfers.as<int>();
  stagingExpectedGolfChunks = totalChunks.as<int>();
  stagingGolfLastActivityTime = millis();
  golfTransferActive = true;
  USBSerial.print("BLE golf transfer started: tournament=");
  USBSerial.println(stagingGolfTournamentName);
  return true;
}


bool handleGolfChunkPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "golf_chunk", document, packet) ||
      !golfTransferActive)
  {
    rejectGamePacket("golf_chunk without golf_start");
    return false;
  }
  const char *transferId;
  JsonVariantConst chunkIndex = packet["chunkIndex"];
  if (!readRequiredText(packet, "transferId", 48, transferId) ||
      strcmp(transferId, stagingGolfTransferId) != 0)
  {
    rejectGamePacket("wrong golf transferId");
    return false;
  }
  if (!chunkIndex.is<int>() || chunkIndex.as<int>() < 0 ||
      chunkIndex.as<int>() >= stagingExpectedGolfChunks ||
      chunkIndex.as<int>() != stagingNextGolfChunkIndex)
  {
    rejectGamePacket(
      chunkIndex.is<int>() && chunkIndex.as<int>() < stagingNextGolfChunkIndex
        ? "duplicate golf chunk"
        : "invalid golf chunk index"
    );
    return false;
  }
  if (!packet["golfers"].is<JsonArrayConst>())
  {
    rejectGamePacket("invalid golfers");
    return false;
  }
  JsonArrayConst golfers = packet["golfers"].as<JsonArrayConst>();
  if (golfers.size() == 0 ||
      stagingReceivedGolfers + golfers.size() > stagingExpectedGolfers)
  {
    rejectGamePacket("invalid golfer count");
    return false;
  }
  GolfLeaderboardRow validated[GameManager::MAX_RECEIVED_GOLFERS] = {};
  uint8_t count = 0;
  for (JsonVariantConst item : golfers)
  {
    if (!item.is<JsonObjectConst>() ||
        !readGolfRow(item.as<JsonObjectConst>(), validated[count]))
    {
      rejectGamePacket("malformed golfer");
      return false;
    }
    count++;
  }
  for (uint8_t index = 0; index < count; index++)
  {
    stagingGolfers[stagingReceivedGolfers + index] = validated[index];
  }
  stagingReceivedGolfers += count;
  stagingNextGolfChunkIndex++;
  stagingGolfLastActivityTime = millis();
  USBSerial.print("BLE golf chunk accepted: index=");
  USBSerial.println(chunkIndex.as<int>());
  return true;
}


bool handleGolfEndPacket(const String &message)
{
  JsonDocument document;
  JsonObjectConst packet;
  if (!readSlateTransferDocument(message, "golf_end", document, packet) ||
      !golfTransferActive)
  {
    rejectGamePacket("golf_end without golf_start");
    return false;
  }
  const char *transferId;
  if (!readRequiredText(packet, "transferId", 48, transferId) ||
      strcmp(transferId, stagingGolfTransferId) != 0 ||
      stagingNextGolfChunkIndex != stagingExpectedGolfChunks ||
      stagingReceivedGolfers != stagingExpectedGolfers)
  {
    rejectGamePacket("incomplete/wrong golf transfer");
    return false;
  }
  if (!gameManager.setReceivedGolfLeaderboard(
    "PGA",
    stagingGolfTournamentId,
    stagingGolfTournamentName,
    stagingGolfers,
    stagingReceivedGolfers
  ))
  {
    rejectGamePacket("received league capacity reached");
    clearGolfStaging("league capacity reached");
    return false;
  }
  USBSerial.println("BLE golf transfer complete.");
  clearGolfStaging(nullptr);
  drawDashboard();
  return true;
}


/**
 * Handle legacy BLE commands or a version 1 game/slate packet.
 */
void handleBluetoothCommand(const String &message)
{
  if (message == "NEXT_GAME")
  {
    USBSerial.println("BLE command: NEXT_GAME");
    gameManager.nextGame();
    drawDashboard();
    return;
  }

  if (message == "NEXT_LEAGUE")
  {
    USBSerial.println("BLE command: NEXT_LEAGUE");
    gameManager.nextLeague();
    drawDashboard();
    return;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(
    document,
    message.c_str(),
    message.length()
  );
  if (!error && document.is<JsonObject>() &&
      document["type"].is<const char *>())
  {
    const char *type = document["type"].as<const char *>();
    if (strcmp(type, "slate") == 0)
    {
      handleSlatePacket(message);
      return;
    }
    if (strcmp(type, "slate_start") == 0)
    {
      handleSlateStartPacket(message);
      return;
    }
    if (strcmp(type, "slate_chunk") == 0)
    {
      handleSlateChunkPacket(message);
      return;
    }
    if (strcmp(type, "slate_end") == 0)
    {
      handleSlateEndPacket(message);
      return;
    }
    if (strcmp(type, "golf_start") == 0)
    {
      handleGolfStartPacket(message);
      return;
    }
    if (strcmp(type, "golf_chunk") == 0)
    {
      handleGolfChunkPacket(message);
      return;
    }
    if (strcmp(type, "golf_end") == 0)
    {
      handleGolfEndPacket(message);
      return;
    }
  }

  handleGamePacket(message);
}


/**
 * Keep BLE handling isolated from display and button code.
 */
void handleBluetoothMessages()
{
  bluetoothManager.updateBluetooth();
  expireStagingSlateIfNeeded();
  expireGolfStagingIfNeeded();

  bool bluetoothConnected = bluetoothManager.isBluetoothConnected();

  if (bluetoothConnected != lastDisplayedBluetoothConnected)
  {
    lastDisplayedBluetoothConnected = bluetoothConnected;
    drawDashboard();
  }

  if (!bluetoothManager.hasReceivedMessage())
  {
    return;
  }

  handleBluetoothCommand(bluetoothManager.getReceivedMessage());
}


/**
 * Run a single click after the double-click window has passed.
 */
void handlePendingClick()
{
  if (pendingClickCount != 1)
  {
    return;
  }

  if ((millis() - lastClickTime) < DOUBLE_CLICK_WINDOW_MS)
  {
    return;
  }

  pendingClickCount = 0;
  USBSerial.println("Single click: next game");
  gameManager.nextGame();
  drawDashboard();
}


/**
 * Count one click after a clean press and release.
 */
void countBootButtonClick()
{
  unsigned long now = millis();

  if (pendingClickCount == 1 && (now - lastClickTime) <= DOUBLE_CLICK_WINDOW_MS)
  {
    pendingClickCount = 0;
    USBSerial.println("Double click: next league");
    gameManager.nextLeague();
    drawDashboard();
    return;
  }

  pendingClickCount = 1;
  lastClickTime = now;
}


/**
 * Read the BOOT button and detect clean clicks without blocking.
 */
void handleBootButton()
{
  bool currentReading = digitalRead(BOOT_BUTTON_PIN);

  if (currentReading != lastBootButtonReading)
  {
    lastBootButtonChangeTime = millis();
    lastBootButtonReading = currentReading;
  }

  if ((millis() - lastBootButtonChangeTime) < BUTTON_DEBOUNCE_MS)
  {
    handlePendingClick();
    return;
  }

  if (currentReading == stableBootButtonState)
  {
    handlePendingClick();
    return;
  }

  stableBootButtonState = currentReading;

  if (stableBootButtonState == LOW)
  {
    bootButtonWasPressed = true;
    handlePendingClick();
    return;
  }

  if (bootButtonWasPressed)
  {
    bootButtonWasPressed = false;
    countBootButtonClick();
  }

  handlePendingClick();
}


void setup()
{
  USBSerial.begin(115200);
  USBSerial.println("Starting Peter's Sports Hub...");

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  lastBootButtonReading = digitalRead(BOOT_BUTTON_PIN);
  stableBootButtonState = lastBootButtonReading;

  initializeDisplay();
  bluetoothManager.beginBluetooth();
  lastDisplayedBluetoothConnected = bluetoothManager.isBluetoothConnected();

  drawSplashScreen();

  delay(2000);

  drawDashboard();

  USBSerial.println("Sports Hub is running.");
}


void loop()
{
  handleBluetoothMessages();
  handleBootButton();
}
