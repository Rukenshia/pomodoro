#include <Arduino.h>

#include <ESP32Encoder.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include "GxEPD2_display_selection_new_style.h"
#include "timer.h"
#include "led.h"
#include "debug.h"
#include "images.h"
#include "checkbox.h"
#include "splashscreen.h"
#include "button.h"
#include "icon_provider.h"
#include "anniversary.h"
#include "preferences_manager.h"
#include "secondary_stats.h"

#if STRINGS_TEST
#include <sstream>
#endif

#define MINUTE 60 * 1000

#define ENCODER_CLK 32
#define ENCODER_DT 21

#define ENCODER_STABILITY_DELAY 50         // ms to wait for stable position
#define ENCODER_LOCK_TIME_AFTER_BUTTON 200 // ms to ignore encoder changes after button press

ESP32Encoder encoder;
volatile unsigned long lastEncoderUpdate = 0;
const unsigned long encoderDebounceTime = 10; // ms
volatile int debouncedCount = 0;
volatile int lastCount = 0;
volatile int tempCount = 0;
volatile unsigned long tempCountTime = 0;
volatile bool positionStable = true;
volatile bool buttonPressed = false;
volatile unsigned long lastButtonPressTime = 0;

#if CHECKBOX_TEST
Checkbox checkbox(&icon_lpehacker, "A test", "test");
Checkbox checkbox2(&icon_lpetantrum, "Another test", "test2");
#endif

void IRAM_ATTR checkPosition(void *arg)
{
  unsigned long currentTime = millis();
  int currentCount = encoder.getCount();

  // If there was a recent button press, ignore encoder changes
  if (currentTime - Button::instance->lastPressTime < ENCODER_STABILITY_DELAY)
  {
    return;
  }

  if (currentCount % 2 == 0 && currentCount != debouncedCount && (currentTime - lastEncoderUpdate >= encoderDebounceTime))
  {
    debouncedCount = (currentCount % 2 == 0 ? currentCount : currentCount - 1) / 2;
    lastEncoderUpdate = currentTime;
  }
}

void setupEncoder()
{
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);

  encoder = ESP32Encoder(true, checkPosition);
  ESP32Encoder::useInternalWeakPullResistors = puType::none;

  encoder.attachHalfQuad(ENCODER_DT, ENCODER_CLK);
  encoder.setFilter(1023);

  encoder.clearCount();
  debouncedCount = 0;
  lastCount = 0;
}

Timer timer(display);

#ifdef EPD2_ENABLE
// 副屏实例。宏没开时这个符号根本不存在，下面的接线也一并消失。
SecondaryStats secondaryStats;
#endif

void setup()
{
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Button::instance = new Button(ENCODER_SW);
  ledSetupEncoder(&debouncedCount);

  setupLed();

  Serial.begin(115200);

  // Initialize the encoder
  setupEncoder();

  // Initialize preferences once
  initPreferences();

  // Initialize the display
  display.init(115200, true, 2, false);
  display.setRotation(0);

#ifdef EPD2_ENABLE
  // 副屏：主屏上面刚做完整屏复位，共用 RST 的副屏不再重复复位（否则会把
  // 主屏刚初始化的面板又拽回上电态）。初始化完把统计卡挂到定时器上。
  secondaryStats.begin();
  timer.setSink(&secondaryStats);
#endif

  // Load LPE mode setting from checkbox
  IconProvider::getInstance()->setLpeMode(pref_getCheckbox("lpe", true));

#ifdef DEBUG
#if ICON_SCALING_TEST
  const uint16_t padding = 20;

  const auto sizes = {48, 64, 128, 192};
  uint16_t xOffset = padding;
  for (const auto size : sizes)
  {
    const auto icon = icon_lpesip.scaled(size);
    const uint16_t yOffset = padding + 192 - size;
    // draw each icon size side by side left to right
    display.drawBitmap(xOffset, padding, icon.data, size, size, GxEPD_BLACK);
    drawCenteredText(display, String(size).c_str(), xOffset + size / 2, size + 2 * padding, &SUB_FONT, GxEPD_BLACK);

    drawCenteredText(display, "#autoscaling", display.width() / 2, display.height() / 2 + 48, &MAIN_FONT, GxEPD_BLACK);

    xOffset += size + padding;
  }

  display.display();

  while (true)
    ;
#endif

#if PATTERN_TEST

  // draw each pattern in a rounded area on a grid within display bounds
  const uint16_t padding = 10;
  const uint16_t w = display.width() / patterns.size() - 2 * padding;
  for (uint16_t i = 0; i < patterns.size(); i++)
  {
    drawPatternInRoundedArea(display, i * display.width() / patterns.size() + padding, padding, w, display.height() - 2 * padding, 10, Pattern(i));

    char buffer[2];
    sprintf(buffer, "%d", i);
    drawCenteredText(display, buffer, i * display.width() / patterns.size() + display.width() / patterns.size() / 2, display.height() - padding - 10, &SUB_FONT, GxEPD_BLACK);
  }

  display.display();

  while (true)
    ;

#endif

#if IMAGE_CYCLE_TEST
  std::vector<const unsigned char *> images = {
      // image_bg_cat,
      image_bg_stonks,
      image_bg_pablo,
      image_bg_what_a_week,
  };

  display.fillScreen(GxEPD_WHITE);
  display.display();

  for (const auto image : images)
  {

    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, image, display.width(), display.height(), GxEPD_BLACK);
    display.display(false);
    display.drawBitmap(0, 0, image, display.width(), display.height(), GxEPD_BLACK);
    display.display(true);
    display.drawBitmap(0, 0, image, display.width(), display.height(), GxEPD_BLACK);
    display.display(true);

    delay(5000);
  }
#endif

#if CHECKBOX_TEST
  display.fillScreen(GxEPD_WHITE);

  Checkbox *selected = &checkbox;

  checkbox.load();
  checkbox2.load();

  checkbox.draw(display, 0, 0, display.width(), 96, selected == &checkbox);
  checkbox2.draw(display, 0, 96 + 8, display.width(), 96, selected == &checkbox2);
  display.display();
  while (true)
  {
    if (!digitalRead(ENCODER_SW))
    {
      display.fillScreen(GxEPD_WHITE);

      selected->toggle();
      selected->save();

      checkbox.draw(display, 0, 0, display.width(), 96, selected == &checkbox);
      checkbox2.draw(display, 0, 96 + 8, display.width(), 96, selected == &checkbox2);
      display.display();
      delay(500);
    }

    if (debouncedCount != lastCount)
    {
      display.fillScreen(GxEPD_WHITE);

      selected->draw(display, 0, selected == &checkbox ? 0 : 96 + 8, display.width(), 96, false);
      selected = selected == &checkbox ? &checkbox2 : &checkbox;
      selected->draw(display, 0, selected == &checkbox ? 0 : 96 + 8, display.width(), 96, true);

      display.display();
    }

    lastCount = debouncedCount;
  }
#endif

#endif

#if STRINGS_TEST

  // Space for message: 221,330 until 649,409
  const uint16_t messageMinX = 221;
  const uint16_t messageMaxX = 649;
  const uint16_t messageMinY = 330;
  const uint16_t messageMaxY = 409;
  const uint16_t messageW = messageMaxX - messageMinX;
  const uint16_t messageH = messageMaxY - messageMinY;

  Serial.println("--- Messages ---");

  for (auto msg : messageCache.getMessages())
  {
    // Split message by lines and print individually
    std::string messageStr(msg);
    std::istringstream iss(messageStr);
    std::string line;
    int lineIndex = 0;
    while (std::getline(iss, line, '\n'))
    {
      if (lineIndex >= 3)
      {
        Serial.printf("ERR: Too many lines in message \"%s\"\n", msg);
        break;
      }

      int yPos = messageMinY + lineIndex * 18 + 18; // + 18 because of the first line
      Bounds b = drawText(display, line.c_str(), messageMinX, yPos, &SMALL_FONT, GxEPD_BLACK);
      ++lineIndex;

      if (b.w > messageW || b.y + b.h > messageMaxY)
      {
        auto wOver = b.w - messageW;
        auto hOver = b.y + b.h - messageMaxY;

        Serial.printf("ERR: Line too long: \"%s\" {x: %d, y: %d, w: %d (+%d), h: %d (+%d)}\n", line.c_str(), b.x, b.y, b.w, wOver > 0 ? wOver : 0, b.h, hOver > 0 ? hOver : 0);
      }
      else
      {
        Serial.printf("OK:  \"%s\" {x: %d, y: %d, w: %d, h: %d}\n", line.c_str(), b.x, b.y, b.w, b.h);
      }
    }
  }
#endif

#if ANNIVERSARY_MODE
  if (!pref_getCheckbox("anniversary", false))
  {
    Anniversary anniversary(display);

    anniversary.loop();

    pref_putCheckbox("anniversary", true);
  }
#endif

  setLedMode(LedMode::Splashscreen);

  SplashScreen splashScreen(display, timer);
  splashScreen.draw();
  splashScreen.loop(&debouncedCount);
  setLedMode(LedMode::Off);

  auto iconProvider = IconProvider::getInstance();

  timer.addPreset(iconProvider->getPresetIcon("Emails"), iconProvider->getTimerRunningBackgroundImage(), "Emails", 15 * MINUTE, 5 * MINUTE, 15 * MINUTE);
  timer.addPreset(iconProvider->getPresetIcon("Coding"), iconProvider->getTimerRunningBackgroundImage(), "Coding", 45 * MINUTE, 15 * MINUTE, 30 * MINUTE, 2);
  timer.addPreset(iconProvider->getPresetIcon("Focus"), iconProvider->getTimerRunningBackgroundImage(), "Focus", 25 * MINUTE, 5 * MINUTE, 20 * MINUTE);

  timer.selectPreset(1);
}

void loop()
{
  timer.loop(&debouncedCount);
  lastCount = debouncedCount;
}
