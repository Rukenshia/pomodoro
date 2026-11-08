#ifndef SECONDARY_STATS_H
#define SECONDARY_STATS_H

#include "secondary_sink.h"

#ifdef EPD2_ENABLE

#include <limits.h>

#include "defs.h"
#include "dual_config.h"

// 副屏：统计卡。
//
// 主屏是「此刻」，副屏是「今天」——两块屏各说一件事，不重复。
// 主屏每秒都可能重画，副屏只在「分钟数变了 / 阶段变了 / 主屏整刷」时才动，
// 一天 8 个番茄大约二十来次刷新，跟一次天气更新的功耗是一个量级。
class SecondaryStats : public SecondarySink
{
public:
    void begin();
    void onTimerSnapshot(const TimerSnapshot &snap, bool fullRedraw) override;

private:
    // 引脚全部来自 dual_config.h 的宏，这里没有任何写死的板级参数。
    // RST 共用时宏为 -1，GxEPD2 会跳过自己的复位流程。
    EPD2_CLASS display2_{EPD2_DRIVER_CLASS(EPD2_CS, EPD2_DC, EPD2_RST, EPD2_BUSY)};

    bool dirty = true;
    TimerPhase lastPhase = TimerPhase::Idle;
    TimerRunState lastRun = TimerRunState::Idle;
    unsigned long lastMinute = ULONG_MAX;
    unsigned int lastInSet = UINT_MAX;

    void render(const TimerSnapshot &snap);
};

#endif // EPD2_ENABLE

#endif // SECONDARY_STATS_H
