一.编程环境要求
安装esp32的驱动，使用串口监视器进行测试，确保可以识别
下载配置vscode环境，安装platformIO插件，安装天问Block客户端软件
二.简介
本项目基于ESP32实现了一个智能语音助手，连接的是科大讯飞的星火模型，后期会尝试连接其他大模型。
多功能：支持串口对话、显示屏实时显示等
便捷网页配置：支持网页配置WiFi信息
三.硬件推荐
ESP-WROOM-32
INMP441全向麦克风
MAX98357 I2S音频放大器
1.8寸RGB_TFT屏幕(128x160)
ASRPRO开发板或者核心板(2M/4M均可)
其他辅助配件若干，具体详细硬件参考硬件原理图
四.连接（如果用模块搭建话，可以使用面包板和跳线搭配使用）
1. INMP441麦克风模块
模块引脚	ESP32连接	
VDD	3.3V	
GND	GND
SD	GPIO22	
WS	GPIO15	
SCK	GPIO4	
2. MAX98357音频放大模块
模块引脚	ESP32连接	
Vin	VIN	
GND	GND	
LRC	GPIO27	
BCLK	GPIO26	
DIN	GPIO25	
3. 1.8寸RGB_TFT屏幕
模块引脚	ESP32连接	
VDD	VIN	
GND	GND	
SCL	GPIO18	
SDA	GPIO23	
RST	GPIO12	
DC	GPIO32	
CS	GPIO5	
4. ASRPRO模块
模块引脚	ESP32连接	
5V	VIN
GND	GND	
PA5	RX2(GPIO16)	
PA6	TX2(GPIO17)	
5.LED指示灯
语音播报互动  IO2
唤醒         IO21
WIFI         IO14
五.功能简单介绍
1.可以语音控制网页配网，有语音提示和显示，和超时自动退出配网。通过说出莫斯或者小苔藓唤醒ASR ，通过管理无线网络指令可以进入配网，然后说出重新连接网络即可退出配网。
2.可以离线唤醒和手动按键唤醒，离线语音唤醒指令为“我们聊天吧”，听到回复语后有蜂鸣器滴一声即可开始问问题。
3.网络连接增加了当前网络的信号检测，信号低于70，就会再次重连网络以此类推。还有当网络断开后，会连接其他wifi。
4.可以在视频里查看其他更多的细节。
六.其他
1.如果使用模块搭建，直接烧录程序，就可以使用。（ESP32需要添加自己的星火模型参数）
2.以上ESP32语音项目基于https://github.com/Explorerlowi/ESP32_AI_LLM.git和GitHub：https://github.com/MetaWu2077/Esp32_VoiceChat_LLMs/tree/main的开源代码上修改添加ASRPRO来实现的！
3.后期，我会继续更新完善代码，以增加其他功能。

