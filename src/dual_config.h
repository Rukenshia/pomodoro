#ifndef DUAL_CONFIG_H
#define DUAL_CONFIG_H

// ---------------------------------------------------------------------------
// 第二块屏（B 屏 / 副屏）的全部配置入口。
//
// 型号、引脚、色数一律通过 platformio.ini 的 build_flags 注入，源码里不写死
// 任何板级参数——换一块屏只需要改 ini，不需要动代码。
//
//   build_flags =
//       -DEPD2_ENABLE=1
//       -DEPD2_DRIVER_CLASS=GxEPD2_420   ; 换成你自己面板对应的 GxEPD2 类
//       -DEPD2_CS=39                     ; 第二块屏的 CS（必须与主屏不同）
//       -DEPD2_DC=8                      ; 与主屏共用 DC 时填同一个引脚号
//       -DEPD2_BUSY=40                   ; 第二块屏的 BUSY（必须独立）
//       -DEPD2_RST=-1                    ; 与主屏共用 RST 时填 -1
//       -DEPD2_3COLOR=1                  ; 三色面板才需要，黑白不要定义
//
// 未定义 EPD2_ENABLE 时，本文件不产生任何符号，整个副屏功能被预处理器剔除，
// 编译产物与你原来的单屏版本逐字节等价。
// ---------------------------------------------------------------------------

#ifdef EPD2_ENABLE

#include <GxEPD2.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#ifndef EPD2_DRIVER_CLASS
// 默认给一个 4.2 寸 400x300 黑白面板（GDEW042T2）当占位，
// 不匹配你的屏时刷出来会是花屏或全白，改这个宏即可。
#define EPD2_DRIVER_CLASS GxEPD2_420
#endif

#ifndef EPD2_CS
#error "EPD2_CS 未定义：第二块屏的片选引脚必须显式给出。"
#endif
#ifndef EPD2_BUSY
#error "EPD2_BUSY 未定义：第二块屏的 BUSY 引脚必须独立。"
#endif
#ifndef EPD2_DC
#error "EPD2_DC 未定义：填入与主屏相同的 DC 引脚号。"
#endif

// 共用 RST 时传 -1，让 GxEPD2 跳过自己的复位流程（由主屏那次复位统一处理）。
// 两块屏各自持有一个 RST 实例会互相打断对方的复位时序。
#ifndef EPD2_RST
#define EPD2_RST -1
#endif

// GxEPD2 库里没有 MAX_HEIGHT，那是示例代码自己定义的。这里直接给整屏缓冲：
// 4.2 寸 400x300 约 15KB，800x480 约 48KB。RAM 吃紧可以调小，GxEPD2 会自动
// 退化成按页渲染（paged），只是刷新会慢一些。
#ifndef EPD2_BUFFER_HEIGHT
#define EPD2_BUFFER_HEIGHT (EPD2_DRIVER_CLASS::HEIGHT)
#endif

#ifdef EPD2_3COLOR
#define EPD2_CLASS GxEPD2_3C<EPD2_DRIVER_CLASS, EPD2_BUFFER_HEIGHT>
#define EPD2_ACCENT GxEPD_RED
#else
#define EPD2_CLASS GxEPD2_BW<EPD2_DRIVER_CLASS, EPD2_BUFFER_HEIGHT>
#define EPD2_ACCENT GxEPD_BLACK
#endif

#endif // EPD2_ENABLE

#endif // DUAL_CONFIG_H
