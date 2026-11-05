#include "secondary_stats.h"

#ifdef EPD2_ENABLE

#include <stdio.h>

#include "statistics.h"

namespace
{

// 只在本文件内用的排版小工具。gfx_utils 那套函数的形参类型是固定的
// DISPLAY_CLASS，第二块屏类型不同接不上，所以这里自己算居中——反正总共
// 也就五个元素。
template <typename D>
uint16_t textWidth(D &d, const char *s, const GFXfont *f)
{
    int16_t x1 = 0, y1 = 0;
    uint16_t tw = 0, th = 0;
    d.setFont(f);
    d.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
    return tw;
}

template <typename D>
void textAt(D &d, const char *s, int16_t x, int16_t cy, const GFXfont *f, uint16_t color)
{
    int16_t x1 = 0, y1 = 0;
    uint16_t tw = 0, th = 0;
    d.setFont(f);
    d.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
    d.setTextColor(color, GxEPD_WHITE);
    d.setCursor(x - x1, cy - (int16_t)th / 2 - y1);
    d.print(s);
}

template <typename D>
void textRight(D &d, const char *s, int16_t right, int16_t cy, const GFXfont *f, uint16_t color)
{
    textAt(d, s, right - (int16_t)textWidth(d, s, f), cy, f, color);
}

template <typename D>
void textCentered(D &d, const char *s, int16_t cx, int16_t cy, const GFXfont *f, uint16_t color)
{
    textAt(d, s, cx - (int16_t)textWidth(d, s, f) / 2, cy, f, color);
}

void formatDuration(unsigned long ms, char *out, size_t n)
{
    const unsigned long totalMin = ms / 60000UL;
    const unsigned long hh = totalMin / 60UL;
    const unsigned long mm = totalMin % 60UL;
    if (hh > 0)
    {
        snprintf(out, n, "%luh %02lum", hh, mm);
    }
    else
    {
        snprintf(out, n, "%lu 分钟", mm);
    }
}

const char *phaseTitle(const TimerSnapshot &s)
{
    if (s.phase == TimerPhase::Break)
    {
        return s.isLongBreak ? "长休息" : "休息中";
    }
    if (s.phase == TimerPhase::Focus)
    {
        return s.presetName && *s.presetName ? s.presetName : "专注";
    }
    return "专注统计";
}

} // namespace

void SecondaryStats::begin()
{
    // 不复位 RST（宏为 -1 时 GxEPD2 自己会跳过），只做面板初始化与清屏。
    display2_.init(115200, true, 2, false);
    display2_.setRotation(0);
    render(TimerSnapshot{TimerPhase::Idle, TimerRunState::Idle, "", 0, 0, 0, 0,
                         1, 4, 0, false, 0, 0});
}

void SecondaryStats::onTimerSnapshot(const TimerSnapshot &snap, bool fullRedraw)
{
    const unsigned int inSet =
        (snap.phase == TimerPhase::Break)
            ? ((snap.cycleTotal && snap.completedCycles % snap.cycleTotal == 0)
                   ? snap.cycleTotal
                   : (snap.cycleTotal ? snap.completedCycles % snap.cycleTotal : 0))
            : (snap.cycleTotal ? snap.completedCycles % snap.cycleTotal + 1 : 1);

    const unsigned long minute = snap.remainingMs / 60000UL;

    if (fullRedraw || snap.phase != lastPhase || snap.runState != lastRun ||
        minute != lastMinute || inSet != lastInSet)
    {
        dirty = true;
    }

    if (!dirty)
    {
        return;
    }

    render(snap);

    dirty = false;
    lastPhase = snap.phase;
    lastRun = snap.runState;
    lastMinute = minute;
    lastInSet = inSet;
}

void SecondaryStats::render(const TimerSnapshot &snap)
{
    auto &d = display2_;
    const int16_t w = (int16_t)d.width();
    const int16_t h = (int16_t)d.height();
    const int16_t margin = (int16_t)(w / 22 < 10 ? 10 : w / 22);
    const int16_t contentW = w - 2 * margin;

    // 同一套版式跑在 400x300 和 800x480 上都成立，靠这两档字体切换。
    const GFXfont *fTitle = (w >= 640) ? &TITLE_FONT : &SECONDARY_FONT;
    const GFXfont *fBig = (w >= 640) ? &LARGE_FONT : &MAIN_FONT;
    const GFXfont *fMeta = (w >= 640) ? &SECONDARY_FONT : &TEXT_FONT;
    const GFXfont *fSmall = &SMALL_FONT;

    unsigned int totalCycles = 0;
    unsigned long totalMs = 0;
    unsigned long breakMs = 0;
    getStatistics(&totalCycles, &totalMs, &breakMs);

    const unsigned int setTotal = snap.cycleTotal ? snap.cycleTotal : 4;
    const unsigned int inSet =
        (snap.phase == TimerPhase::Break)
            ? ((snap.completedCycles % setTotal == 0) ? setTotal : snap.completedCycles % setTotal)
            : (snap.completedCycles % setTotal + 1);

    char accBuf[32];
    char accLine[48];
    formatDuration(totalMs, accBuf, sizeof accBuf);
    snprintf(accLine, sizeof accLine, "累计 %s", accBuf);

    char cycleLine[32];
    snprintf(cycleLine, sizeof cycleLine, "第 %u / %u 轮", inSet, setTotal);

    char bigLine[8];
    if (snap.phase == TimerPhase::Idle && snap.runState == TimerRunState::Idle)
    {
        snprintf(bigLine, sizeof bigLine, "--");
    }
    else
    {
        snprintf(bigLine, sizeof bigLine, "%u", snap.completedCycles);
    }

    const char *footer;
    char footerBuf[64];
    switch (snap.runState)
    {
    case TimerRunState::Paused:
        footer = "已暂停 · 按一下继续";
        break;
    case TimerRunState::Finished:
        footer = snap.phase == TimerPhase::Break ? "休息结束 · 等你确认" : "专注结束 · 等你确认";
        break;
    case TimerRunState::Idle:
        footer = "在设备上按一下开始";
        break;
    default:
        if (snap.phase == TimerPhase::Focus)
        {
            formatDuration(snap.focusDurationMs, footerBuf, sizeof footerBuf);
            footer = "专注进行中";
        }
        else
        {
            char focusBuf[32];
            formatDuration(snap.focusDurationMs, focusBuf, sizeof focusBuf);
            snprintf(footerBuf, sizeof footerBuf, "下一轮 专注 %s", focusBuf);
            footer = footerBuf;
        }
        break;
    }

    d.setFullWindow();
    d.firstPage();
    do
    {
        d.fillScreen(GxEPD_WHITE);

        // 页眉
        textAt(d, "专注统计", margin, (int16_t)(h * 0.09), fTitle, GxEPD_BLACK);
        textRight(d, accLine, w - margin, (int16_t)(h * 0.09), fMeta, GxEPD_BLACK);
        d.drawFastHLine(margin, (int16_t)(h * 0.18), contentW, GxEPD_BLACK);

        // 主体：本组完成的番茄数，是这个屏上唯一值得用最大字号的东西
        textCentered(d, bigLine, (int16_t)(w * 0.30), (int16_t)(h * 0.37), fBig, EPD2_ACCENT);
        textCentered(d, "本组番茄", (int16_t)(w * 0.30), (int16_t)(h * 0.53), fSmall, GxEPD_BLACK);
        textRight(d, cycleLine, w - margin, (int16_t)(h * 0.40), fMeta, GxEPD_BLACK);

        // 进度条：本组走到第几个，长休还有多远
        const int16_t barY = (int16_t)(h * 0.64);
        const int16_t barH = (int16_t)(h * 0.045 < 8 ? 8 : h * 0.045);
        d.drawRect(margin, barY, contentW, barH, GxEPD_BLACK);
        if (setTotal > 0 && inSet > 0)
        {
            const int16_t filled = (int16_t)((long)contentW * inSet / setTotal);
            if (filled > 1)
            {
                d.fillRect(margin, barY, filled, barH, EPD2_ACCENT);
            }
        }

        // 页脚：下一步做什么，比"当前是什么"更值得读
        textAt(d, footer, margin, (int16_t)(h * 0.84), fMeta, GxEPD_BLACK);
    } while (d.nextPage());
}

#endif // EPD2_ENABLE
