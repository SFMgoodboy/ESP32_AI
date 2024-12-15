#include "1.8TFT.h"

// 创建屏幕对象
TFT_eSPI tft = TFT_eSPI(); // 创建TFT对象
U8g2_for_TFT_eSPI u8g2;
String text_temp = ""; // 存储超出当前屏幕的文字，在下一屏幕显示

void tft_start(void)
{
    // 初始化屏幕
    tft.init();
    tft.setRotation(0); // 设置屏幕方向，0-3顺时针转
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_WHITE);   // 设置屏幕背景为白色
    tft.setTextColor(TFT_BLACK); // 设置字体颜色为黑色
    tft.setTextWrap(true);       // 开启文本自动换行，只支持英文
}

void tft_chinese_start(void)
{
    // 初始化U8g2
    u8g2.begin(tft);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312); // 设置中文字体库
    u8g2.setFontMode(1);                    // 设置字体模式为透明模式，不设置的话中文字符会变成一个黑色方块
    u8g2.setForegroundColor(TFT_BLACK);     // 设置字体颜色为黑色
}

void sys_tft_start_ok(void)
{
    // 清空屏幕
    tft.fillScreen(TFT_WHITE);
    // 显示文字
    u8g2.setCursor(0, 12);
    u8g2.print("欢迎使用");
    if (WIFI_sign)
        displayWrappedText("请进行语音唤醒或按boot键开始对话！", 0, u8g2.getCursorY() + 12, width);
    else
        displayWrappedText("WIFI模块故障或者政府网络瘫痪！", 0, u8g2.getCursorY() + 12, width);
}

void sys_tft_start(void)
{
    // 清空屏幕
    tft.fillScreen(TFT_WHITE);
    // 显示文字
    u8g2.setCursor(0, 12);
    u8g2.print("AI系统启动中");
}

void sys_tft_start_wifi(void)
{
    // 显示文字
    u8g2.setCursor(0, u8g2.getCursorY() + 12);
    u8g2.print("正在连接WIFI");
}

void displayWrappedText(const std::string &text1, int x, int y, int maxWidth)
{
    int cursorX = x;
    int cursorY = y;
    int lineHeight = u8g2.getFontAscent() - u8g2.getFontDescent() + 2; // 中文字符12像素高度
    int start = 0;                                                     // 指示待输出字符串已经输出到哪一个字符
    int num = text1.size();
    int i = 0;

    while (start < num)
    {
        u8g2.setCursor(cursorX, cursorY);
        int wid = 0;
        int numBytes = 0;

        // Calculate how many bytes fit in the maxWidth
        while (i < num)
        {
            int size = 1;
            if (text1[i] & 0x80)
            { // 核心所在
                char temp = text1[i];
                temp <<= 1;
                do
                {
                    temp <<= 1;
                    ++size;
                } while (temp & 0x80);
            }
            std::string subWord;
            subWord = text1.substr(i, size); // 取得单个中文或英文字符

            int charBytes = subWord.size(); // 获取字符的字节长度

            int charWidth = charBytes == 3 ? 12 : 6; // 中文字符12像素宽度，英文字符6像素宽度
            if (wid + charWidth > maxWidth - cursorX)
            {
                break;
            }
            numBytes += charBytes;
            wid += charWidth;

            i += size;
        }

        if (cursorY <= height - 10)
        {
            u8g2.print(text1.substr(start, numBytes).c_str());
            cursorY += lineHeight;
            cursorX = 0;
            start += numBytes;
        }
        else
        {
            text_temp = text1.substr(start).c_str();
            break;
        }
    }
}
