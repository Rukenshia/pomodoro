#ifndef SECONDARY_SINK_H
#define SECONDARY_SINK_H

#include "timer_snapshot.h"

// 副屏的唯一入口。
//
// Timer 不知道副屏是什么、接在哪、甚至有没有——它只在每次主屏刷新完之后
// 调一次 onTimerSnapshot()。没有注册 sink 时这里连函数调用都不会发生，
// 单屏固件的运行时行为因此零变化。
//
// fullRedraw 用来提示「主屏刚做过整屏刷」，副屏可以借这个时机一起整屏刷，
// 避免两块屏的残影在不同时间点各自累积。
class SecondarySink
{
public:
    virtual ~SecondarySink() {}
    virtual void onTimerSnapshot(const TimerSnapshot &snap, bool fullRedraw) = 0;
};

#endif // SECONDARY_SINK_H
