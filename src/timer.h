#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_GFX.h>
#include <vector>

#include "defs.h"

#define ENCODER_SW 14 // Optional push button pin

#include "gfx_utils.h"
#include "icons.h"
#include "debug.h"
#include "menu.h"
#include "led.h"
#include "button.h"
#include "statistics.h"
#include "timer_snapshot.h"
#include "secondary_sink.h"

class Preset
{
private:
    Icon *icon;
    const unsigned char *background;
    const char *name;
    unsigned long duration;
    unsigned long pauseDuration;
    unsigned int longPauseAfter;
    unsigned long longPauseDuration;

public:
    Preset(Icon *icon, const unsigned char *background, const char *name, unsigned long duration, unsigned long pauseDuration, unsigned long longPauseDuration, unsigned int longPauseAfter);
    ~Preset();
    Icon *getIcon();
    const unsigned char *getBackground();
    unsigned long getDuration();
    unsigned long getPauseDuration();
    unsigned long getLongPauseDuration();
    unsigned int getLongPauseAfter();

    const char *getName();
};

enum class TimerState
{
    SelectingPreset,
    Running,
    WaitingConfirmStartOfBreak,
    RunningBreak,
    WaitingConfirmEndOfBreak,
    UserInitiatedPause,
    UserInitiatedBreakPause,
    Stopped
};

class Timer
{
private:
    DISPLAY_CLASS &display;
    std::vector<Preset> presets;
    Preset *currentPreset;
    unsigned int presetIndex;
    TimerState state;
    unsigned long startTime;
    unsigned long elapsed;
    unsigned long pauseStartTime;
    unsigned long totalPausedTime;

    bool isLongBreak = false;
    unsigned int cycles = 0;
    unsigned long minutesWorked = 0;
    unsigned long minutesOnBreak = 0;
    unsigned long longestEarnedPauseInShortCycles = 0;
    unsigned long currentBreakDuration = 0;

    Menu *topMenu;
    bool menuNeedsRedraw;

    Menu *confirmationMenu;
    bool flashingIcon;

    const unsigned char *breakImage;

    bool needsRedraw;
    bool needsFullRedraw;
    int lastEncoderCount;
    unsigned long lastRedrawTime;
    unsigned long lastPauseRedrawTime;
    bool lastPauseState;
    static const unsigned long REDRAW_INTERVAL_DEFAULT = 5000;                   // ms
    static const unsigned long REDRAW_INTERVAL_FAST = 1000;                      // ms
    static const unsigned long RUNNING_MESSAGE_REFRESH_INTERVAL = 5 * 60 * 1000; // ms
    unsigned long redrawInterval = REDRAW_INTERVAL_DEFAULT;

    bool showSpeechBubble = true;

    // 副屏出口。为空时（单屏固件的默认情况）整个副屏逻辑不产生任何调用。
    SecondarySink *sink = nullptr;

    void drawWaitingForConfirmation();
    void drawPresetSelection();
    void drawPartialPresetSelection();
    int drawMenuBar();
    void drawRunning();
    void handleWaitingForConfirmation(volatile int *encoderCount);
    void handleSelectingPreset(volatile int *encoderCount);
    void handleRunning(volatile int *encoderCount);
    void handleRunningBreak(volatile int *encoderCount);
    void drawRunningBreak();

    const char *getRunningMessage();
    unsigned long lastMessageUpdate;

    void reset();

public:
    Timer(DISPLAY_CLASS &display);
    ~Timer();
    void addPreset(Icon *icon, const unsigned char *background, const char *name, unsigned long duration, unsigned long pauseDuration, unsigned long longPauseDuration, unsigned int longPauseAfter = 4);
    void selectPreset(int index);
    void nextPreset();
    void previousPreset();
    void enterPresetSelection();
    void start();
    void pause();
    void resume();
    void startBreak();
    void stop();
    void loop(volatile int *encoderCount);
    TimerState getState();
    bool checkAndClearButtonPress();

    // 接一块（或多块）副屏。传 nullptr 即摘掉。
    void setSink(SecondarySink *s) { sink = s; }

    // 只读快照：副屏据此渲染，不直接读 Timer 的私有字段，也不反过来改状态。
    TimerSnapshot snapshot() const;
};

#endif
