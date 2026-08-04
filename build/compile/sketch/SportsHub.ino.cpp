#line 1 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
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


/**
 * Prepare the LCD and backlight.
 */
#line 71 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void initializeDisplay();
#line 91 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawSplashScreen();
#line 124 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawTeamName(const char *teamName, int16_t x, int16_t y);
#line 158 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawScore(uint8_t score, uint16_t color, int16_t y);
#line 181 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawHeader();
#line 209 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawScoreboard(const GameData &game);
#line 253 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawStatusBar();
#line 275 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void drawDashboard();
#line 287 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
bool isValidUtf8(const String &text);
#line 353 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void rejectGamePacket(const char *reason);
#line 360 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
bool readRequiredText( JsonObjectConst packet, const char *key, size_t maximumLength, const char *&value );
#line 386 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
bool handleGamePacket(const String &message);
#line 479 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void handleBluetoothCommand(const String &message);
#line 504 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void handleBluetoothMessages();
#line 528 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void handlePendingClick();
#line 550 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void countBootButtonClick();
#line 571 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void handleBootButton();
#line 612 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void setup();
#line 635 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
void loop();
#line 71 "C:\\Users\\Jay\\OneDrive\\Documents\\Arduino\\SportsHub\\SportsHub.ino"
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
 * Draw the application's top title area.
 */
void drawHeader()
{
  gfx->fillRoundRect(10, 8, 220, 58, 8, COLOR_DARK_BLUE);
  gfx->drawRoundRect(10, 8, 220, 58, 8, COLOR_LIGHT_BLUE);

  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(22, 16);
  gfx->println("PETER'S SPORTS HUB");

  gfx->setTextColor(CYAN);
  gfx->setTextSize(3);
  gfx->setCursor(22, 34);
  gfx->println(gameManager.getCurrentLeagueName());

  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(178, 42);
  gfx->print("L ");
  gfx->print(gameManager.getCurrentLeagueNumber());
  gfx->print("/");
  gfx->println(gameManager.getLeagueCount());
}


/**
 * Draw a scoreboard from game data.
 */
void drawScoreboard(const GameData &game)
{
  // Scoreboard card
  gfx->fillRoundRect(10, 78, 220, 130, 10, COLOR_DARK_GRAY);
  gfx->drawRoundRect(10, 78, 220, 130, 10, COLOR_LIGHT_BLUE);

  // Game position
  gfx->setTextColor(COLOR_GRAY);
  gfx->setTextSize(1);
  gfx->setCursor(22, 91);
  gfx->print("Game ");
  gfx->print(gameManager.getCurrentGameNumber());
  gfx->print(" of ");
  gfx->println(gameManager.getCurrentGameCount());

  // Status badge
  gfx->fillRoundRect(157, 87, 54, 18, 5, COLOR_STATUS_BG);
  gfx->setTextColor(BLACK);
  gfx->setTextSize(1);
  gfx->setCursor(167, 93);
  gfx->println(game.status);

  // Divider
  gfx->drawFastHLine(20, 113, 200, COLOR_GRAY);

  // Teams
  drawTeamName(game.awayTeam, 22, 128);
  drawTeamName(game.homeTeam, 22, 160);

  // Scores
  drawScore(game.awayScore, GREEN, 123);
  drawScore(game.homeScore, WHITE, 155);

  // Game clock
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(1);
  gfx->setCursor(94, 192);
  gfx->println(game.clock);
}


/**
 * Draw the bottom status area.
 */
void drawStatusBar()
{
  gfx->fillRoundRect(10, 220, 220, 48, 8, COLOR_DARK_BLUE);

  gfx->setTextColor(GREEN);
  gfx->setTextSize(1);
  gfx->setCursor(22, 231);
  gfx->println("SYSTEM ONLINE");

  gfx->setTextColor(WHITE);
  gfx->setCursor(22, 249);
  gfx->println(bluetoothManager.getBluetoothStatusText());

  gfx->setTextColor(COLOR_GRAY);
  gfx->setCursor(192, 249);
  gfx->println("0.1");
}


/**
 * Draw the complete dashboard.
 */
void drawDashboard()
{
  const GameData &game = gameManager.getCurrentGame();

  gfx->fillScreen(BLACK);

  drawHeader();
  drawScoreboard(game);
  drawStatusBar();
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
  const char *away;
  const char *home;
  const char *status;
  const char *clock;

  if (!readRequiredText(packet, "league", 12, league) ||
      !readRequiredText(packet, "away", 32, away) ||
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

  gameManager.setReceivedGame(
    league,
    away,
    home,
    static_cast<uint8_t>(awayScore.as<int>()),
    static_cast<uint8_t>(homeScore.as<int>()),
    status,
    clock
  );
  drawDashboard();
  USBSerial.println("BLE game accepted.");
  return true;
}


/**
 * Handle legacy BLE commands or a version 1 game packet.
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

  handleGamePacket(message);
}


/**
 * Keep BLE handling isolated from display and button code.
 */
void handleBluetoothMessages()
{
  bluetoothManager.updateBluetooth();

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

