#ifndef _18_TFT_h
#define _18_TFT_h

// 与屏幕显示有关的库
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>
#include "string.h"
#include "Web_wifi.h"

//#include "bizhi.h"    //导入壁纸数据

#define width   128     //屏幕宽度
#define height  160     //屏幕高度

// 创建屏幕对象
extern TFT_eSPI tft;
extern U8g2_for_TFT_eSPI u8g2;
extern String text_temp;



void tft_start(void);
void tft_chinese_start(void);
void sys_tft_start(void);
void sys_tft_start_ok(void);
void sys_tft_start_wifi(void);

void displayWrappedText(const std::string &text1, int x, int y, int maxWidth);

#endif
