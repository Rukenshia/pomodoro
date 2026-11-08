#include "timer.h"
#include "strings.h"
#include "images.h"
#include "led.h"
#include "preferences_manager.h"

extern Preferences preferences;

Preset::Preset(Icon *icon, const unsigned char *background, const char *name, unsigned long duration, unsigned long pauseDuration, unsigned long longPauseDuration, unsigned int longPauseAfter)
{
    this->icon = icon;
    this->background = background;
    this->name = name;
    this->duration = duration;
    this->pauseDuration = pauseDuration;
    this->longPauseAfter = longPauseAfter;
    this->longPauseDuration = longPauseDuration;
}

Preset::~Preset()
{
}

Icon *Preset::getIcon()
{
    return icon;
}

const unsigned char *Preset::getBackground()
{
    return background;
}

unsigned long Preset::getDuration()
{
#ifdef DEBUG
    return duration / 60;
#endif
    return duration;
}

unsigned long Preset::getPauseDuration()
{
#ifdef DEBUG
    return pauseDuration / 60;
#endif
    return pauseDuration;
}

const char *Preset::getName()
{
    return name;
}

unsigned long Preset::getLongPauseDuration()
{
#ifdef DEBUG
    return longPauseDuration / 60;
#endif
    return longPauseDuration;
}

unsigned int Preset::getLongPauseAfter()
{
    return longPauseAfter;
}

Timer::Timer(DISPLAY_CLASS &display) : display(display)
{
    this->state = TimerState::SelectingPreset;
    this->currentPreset = nullptr;
    this->presetIndex = 0;
    this->needsRedraw = true;
    this->lastEncoderCount = 0;
    this->lastRedrawTime = 0;
    this->pauseStartTime = 0;
    this->totalPausedTime = 0;

    MenuItem *items = new MenuItem[3]{
        MenuItem(messageCache.getMessage(Messages::MenuItem_Pause)),
        MenuItem(messageCache.getMessage(Messages::MenuItem_BreakNow)),
        MenuItem(messageCache.getMessage(Messages::MenuItem_Cancel))};
    this->topMenu = new Menu(display, items, 3);
    this->topMenu->setSelectedIndex(1);
    lastEncoderCount = 0;

    MenuItem *confirmationItems = new MenuItem[2]{
        MenuItem(messageCache.getMessage(Messages::MenuItem_StartBreak)),
        MenuItem(messageCache.getMessage(Messages::MenuItem_BackToPresets))};
    this->confirmationMenu = new Menu(display, confirmationItems, 2);
    this->confirmationMenu->setEncoderCount(0); // Initialize encoder count
}

Timer::~Timer()
{
    if (topMenu)
    {
        delete[] topMenu->getItems();
        delete topMenu;
    }
}

void Timer::addPreset(Icon *icon, const unsigned char *background, const char *name, unsigned long duration, unsigned long pauseDuration, unsigned long longPauseDuration, unsigned int longPauseAfter)
{
    presets.push_back(Preset(icon, background, name, duration, pauseDuration, longPauseDuration, longPauseAfter));
}

void Timer::selectPreset(int index)
{
    currentPreset = &presets[index];
    presetIndex = index;
}

void Timer::nextPreset()
{
    presetIndex++;
    if (presetIndex >= presets.size())
    {
        presetIndex = 0;
    }

    currentPreset = &presets[presetIndex];
}

void Timer::previousPreset()
{
    if (presetIndex <= 0)
    {
        presetIndex = presets.size() - 1;
    }
    else
    {
        presetIndex--;
    }

    currentPreset = &presets[presetIndex];
}

void Timer::enterPresetSelection()
{
    state = TimerState::SelectingPreset;
    selectPreset(1);
    needsRedraw = true;
}

void Timer::reset()
{
    setLedMode(LedMode::Off);
    redrawInterval = REDRAW_INTERVAL_DEFAULT;
    startTime = millis();
    elapsed = 0;
    pauseStartTime = 0;
    totalPausedTime = 0;
    Button::instance->checkAndClearButtonPress();
    topMenu->getItems()[0].setText(messageCache.getMessage(Messages::MenuItem_Pause));
    topMenu->getItems()[1].setText(messageCache.getMessage(Messages::MenuItem_BreakNow));
    topMenu->setSelectedIndex(1);
}

void Timer::start()
{
    if (currentPreset != nullptr)
    {

        showSpeechBubble = pref_getCheckbox("msgs", true);

        // remember selected preset
        auto presetIndex = this->presetIndex;

        reset();
        topMenu->setEncoderCount(lastEncoderCount); // Sync encoder count

        Serial.printf("Timer::start with preset %d\n", presetIndex);
        state = TimerState::Running;
        selectPreset(presetIndex);
    }
}

void Timer::pause()
{
    if (state == TimerState::Running)
    {
        state = TimerState::UserInitiatedPause;
        pauseStartTime = millis();
        setLedMode(LedMode::TimerPaused);
        topMenu->getItems()[0].setText(messageCache.getMessage(Messages::MenuItem_Resume));
        topMenu->setEncoderCount(lastEncoderCount); // Sync encoder count
    }
    else if (state == TimerState::RunningBreak)
    {
        state = TimerState::UserInitiatedBreakPause;
        pauseStartTime = millis();
        setLedMode(LedMode::TimerPaused);
        topMenu->getItems()[0].setText(messageCache.getMessage(Messages::MenuItem_Resume));
        topMenu->setEncoderCount(lastEncoderCount); // Sync encoder count
    }
}

void Timer::resume()
{
    if (state == TimerState::UserInitiatedPause)
    {
        totalPausedTime += millis() - pauseStartTime;
        state = TimerState::Running;
        setLedMode(LedMode::Off);
        topMenu->getItems()[0].setText(messageCache.getMessage(Messages::MenuItem_Pause));
        topMenu->setEncoderCount(lastEncoderCount); // Sync encoder count
    }
    else if (state == TimerState::UserInitiatedBreakPause)
    {
        totalPausedTime += millis() - pauseStartTime;
        state = TimerState::RunningBreak;
        setLedMode(LedMode::Off);
        topMenu->getItems()[0].setText(messageCache.getMessage(Messages::MenuItem_Pause));
        topMenu->setEncoderCount(lastEncoderCount); // Sync encoder count
    }
}

void Timer::startBreak()
{
    cycles += 1;
    incrementTotalCycles();
    minutesWorked += elapsed / 1000 / 60;
    incrementTotalTime(elapsed);

    Serial.printf("Timer::startBreak: cycles %d with long break after %d\n", cycles, currentPreset->getLongPauseAfter());
    if (cycles % (currentPreset->getLongPauseAfter()) == 0)
    {
        Serial.println("Timer::startBreak: long break");
        currentBreakDuration = currentPreset->getLongPauseDuration();
        isLongBreak = true;
        longestEarnedPauseInShortCycles = 0;

        const std::vector<const unsigned char *> breakImages = {
            image_bg_cat,
            image_bg_stonks,
            image_bg_pablo,
            image_bg_what_a_week,
        };

        breakImage = breakImages.at(random(0, breakImages.size()));

        // HACK: The display doesn't like drawing images in one go
        drawRunningBreak();
        display.display(true);
    }
    else
    {
        Serial.println("Timer::startBreak: short break");
        currentBreakDuration = currentPreset->getPauseDuration();
        isLongBreak = false;
        needsRedraw = true;
    }

    state = TimerState::RunningBreak;
    reset();
    topMenu->getItems()[1].setText(messageCache.getMessage(Messages::MenuItem_SkipBreak));

    // Restore the last selected index
    topMenu->setSelectedIndex(1);

    needsRedraw = true;
}

void Timer::stop()
{
    setLedMode(LedMode::Off);
    reset();

    enterPresetSelection();
}

int Timer::drawMenuBar()
{
    const unsigned int padding = 8;
    const unsigned int innerPadding = 4;
    const unsigned int iconSize = 48;

    Bounds bounds = getBounds(display, currentPreset->getName(), &SUB_FONT);

#ifdef DEBUG
    display.drawRect(padding, padding, iconSize, iconSize, GxEPD_BLACK);
#endif

    // Draw the menu next to the category
    // const uint16_t menuX = padding + innerPadding + iconSize + innerPadding + bounds.w + innerPadding + 8;
    const uint16_t menuX = padding;
    const uint16_t menuY = padding;
    const uint16_t paddingBetweenBoxes = 10;
    const uint16_t totalPaddingWidth = paddingBetweenBoxes * (topMenu->getItemCount() - 1);
    const uint16_t rawMenuWidth = display.width() - menuX - padding;
    const uint16_t menuItemWidth = (rawMenuWidth - (topMenu->getItemCount() - 1) * paddingBetweenBoxes) / topMenu->getItemCount();
    const uint16_t menuHeight = innerPadding + 48; // iconSize;

    // Draw the menu
    for (int i = 0; i < topMenu->getItemCount(); i++)
    {
        const bool selected = i == topMenu->getSelectedIndex();
        const auto foreground = GxEPD_BLACK;
        const auto background = GxEPD_WHITE;

        const unsigned int xOffset = menuX + i * (menuItemWidth + paddingBetweenBoxes);

        auto item = topMenu->getItems()[i];

        if (selected)
        {
            display.fillRoundRect(xOffset, menuY, menuItemWidth, menuHeight, 10, GxEPD_WHITE);
            drawPatternInRoundedArea(display, xOffset, menuY, menuItemWidth, menuHeight, 10, Pattern::SparseDots);
        }
        else
        {
            display.fillRoundRect(xOffset, menuY, menuItemWidth, menuHeight, 10, GxEPD_WHITE);
        }
        display.drawRoundRect(xOffset, menuY, menuItemWidth, menuHeight, 10, GxEPD_BLACK);

        drawCenteredText(display, item.getText(), xOffset + menuItemWidth / 2, menuY + menuHeight / 2, &SUB_FONT, foreground);

#ifdef DEBUG
        display.drawFastVLine(xOffset + menuItemWidth / 2, menuY, menuHeight, GxEPD_BLACK);
        display.drawFastHLine(xOffset, menuY + menuHeight / 2, menuItemWidth, GxEPD_BLACK);
        display.drawCircle(xOffset + menuItemWidth / 2, menuY + menuHeight / 2, 5, GxEPD_BLACK);
#endif
    }

    return menuY + menuHeight;
}

void Timer::loop(volatile int *encoderCount)
{
    auto start = millis();

    switch (state)
    {
    case TimerState::SelectingPreset:
        handleSelectingPreset(encoderCount);
        break;
    case TimerState::UserInitiatedPause:
    case TimerState::Running:
        handleRunning(encoderCount);
        break;
    case TimerState::UserInitiatedBreakPause:
    case TimerState::RunningBreak:
        handleRunningBreak(encoderCount);
        break;
    case TimerState::WaitingConfirmEndOfBreak:
    case TimerState::WaitingConfirmStartOfBreak:
        handleWaitingForConfirmation(encoderCount);
        break;
    default:
        Serial.printf("Timer::loop: unknown state %d\n", state);
        break;
    }

    if (menuNeedsRedraw)
    {
        auto millisMenuStart = millis();
        drawMenuBar();
        display.displayWindow(0, 0, display.width(), 8 + 4 + 48 + 4);
        Serial.printf("Timer::loop: menu update took %d ms\n", millis() - millisMenuStart);

        menuNeedsRedraw = false;

        if (!needsRedraw && !needsFullRedraw)
        {
            Serial.printf("Timer::loop: display update took %d ms\n", millis() - start);
        }
    }

    if (needsRedraw || needsFullRedraw)
    {
        Serial.printf("Timer::loop: needs redraw with state %d\n", state);

        display.firstPage();

        if (needsFullRedraw)
        {
            // display.clearScreen();
            display.fillScreen(GxEPD_WHITE);
        }

        switch (state)
        {
        case TimerState::SelectingPreset:
            Serial.println("Timer::loop: drawPresetSelection");
            drawPresetSelection();
            break;
        case TimerState::UserInitiatedPause:
        case TimerState::Running:
            Serial.println("Timer::loop: drawRunning");
            drawRunning();
            break;
        case TimerState::UserInitiatedBreakPause:
        case TimerState::RunningBreak:
            Serial.println("Timer::loop: drawRunningBreak");
            drawRunningBreak();
            break;
        case TimerState::WaitingConfirmEndOfBreak:
        case TimerState::WaitingConfirmStartOfBreak:
            Serial.println("Timer::loop: drawWaitingForConfirmation");
            drawWaitingForConfirmation();
            break;
        default:
            Serial.printf("Timer::loop: unknown state %d\n", state);
            break;
        }

        if (needsFullRedraw)
        {
            Serial.println("Timer::loop: full display update");
            display.display(false);
        }
        else
        {
            Serial.println("Timer::loop: partial display update");
            display.display(true);
        }

        const bool wasFullRedraw = needsFullRedraw;

        menuNeedsRedraw = false;
        needsRedraw = false;
        needsFullRedraw = false;
        lastRedrawTime = millis();
        Serial.printf("Timer::loop: display update took %d ms\n", millis() - start);

        // 主屏刚画完，此刻把同一份状态交给副屏。放在刷新之后而不是 loop 开头，
        // 是为了让两块屏显示的永远是同一个瞬间的计时值——否则副屏会比主屏
        // 早一个循环，暂停时会看到两块屏差一秒。
        if (sink)
        {
            sink->onTimerSnapshot(snapshot(), wasFullRedraw);
        }
    }
}

TimerState Timer::getState()
{
    return state;
}

TimerSnapshot Timer::snapshot() const
{
    TimerSnapshot s;

    s.presetName = currentPreset ? currentPreset->getName() : "";
    s.focusDurationMs = currentPreset ? currentPreset->getDuration() : 0;
    s.cycleTotal = currentPreset ? currentPreset->getLongPauseAfter() : 4;
    s.completedCycles = cycles;
    s.isLongBreak = isLongBreak;
    s.breakDurationMs = currentBreakDuration
                            ? currentBreakDuration
                            : (currentPreset ? currentPreset->getPauseDuration() : 0);

    const bool onBreak = (state == TimerState::RunningBreak ||
                          state == TimerState::UserInitiatedBreakPause ||
                          state == TimerState::WaitingConfirmEndOfBreak);

    if (state == TimerState::SelectingPreset || state == TimerState::Stopped)
    {
        s.phase = TimerPhase::Idle;
        s.runState = TimerRunState::Idle;
    }
    else
    {
        s.phase = onBreak ? TimerPhase::Break : TimerPhase::Focus;

        switch (state)
        {
        case TimerState::Running:
        case TimerState::RunningBreak:
            s.runState = TimerRunState::Running;
            break;
        case TimerState::UserInitiatedPause:
        case TimerState::UserInitiatedBreakPause:
            s.runState = TimerRunState::Paused;
            break;
        case TimerState::WaitingConfirmStartOfBreak:
        case TimerState::WaitingConfirmEndOfBreak:
            s.runState = TimerRunState::Finished;
            break;
        default:
            s.runState = TimerRunState::Idle;
            break;
        }
    }

    s.durationMs = onBreak ? s.breakDurationMs : s.focusDurationMs;
    s.elapsedMs = elapsed;
    s.remainingMs = s.durationMs > s.elapsedMs ? s.durationMs - s.elapsedMs : 0;
    s.overdueMs = s.elapsedMs > s.durationMs ? s.elapsedMs - s.durationMs : 0;

    const unsigned int setTotal = s.cycleTotal ? s.cycleTotal : 4;
    s.cycleIndex = onBreak
                       ? ((cycles % setTotal == 0) ? setTotal : cycles % setTotal)
                       : (cycles % setTotal) + 1;

    return s;
}