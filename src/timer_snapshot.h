#ifndef TIMER_SNAPSHOT_H
#define TIMER_SNAPSHOT_H

#include <Arduino.h>

// 一次「定时器此刻长什么样」的只读快照。
//
// 存在的理由：副屏不应该、也不需要知道 Timer 的内部状态机（它有 8 个状态、
// 一堆只有主屏版式才关心的字段）。把要展示的东西收窄成一个 POD，副屏实现
// 换多少次都不会反过来影响主逻辑。
//
// 注意这里刻意没有复用 TimerState：那是主屏的状态机细节，副屏只关心
// 「专注 / 休息 / 还没开始」和「在跑 / 停着」。两套枚举各管各的，改一边
// 不会牵动另一边。

enum class TimerPhase : uint8_t
{
    Idle,  // 还没开始，或停在选预设界面
    Focus, // 专注段
    Break  // 休息段
};

enum class TimerRunState : uint8_t
{
    Idle,     // 未运行
    Running,  // 正在计时
    Paused,   // 被用户暂停
    Finished  // 本段时长已走完，等用户确认
};

struct TimerSnapshot
{
    TimerPhase phase;
    TimerRunState runState;
    const char *presetName;   // 当前预设名，可能为空串
    unsigned long durationMs; // 本段应有的总时长
    unsigned long elapsedMs;  // 已走时长（暂停期间冻结）
    unsigned long remainingMs;
    unsigned long overdueMs;     // 超出时长，等确认时才是非 0
    unsigned int cycleIndex;      // 在本组（长休息周期）里排第几，1 起
    unsigned int cycleTotal;      // 本组共几个
    unsigned int completedCycles; // 开机以来累计走完的番茄数
    bool isLongBreak;
    unsigned long breakDurationMs; // 当前 / 下一段休息时长
    unsigned long focusDurationMs; // 该预设的专注时长，休息时也要能显示
};

#endif // TIMER_SNAPSHOT_H
