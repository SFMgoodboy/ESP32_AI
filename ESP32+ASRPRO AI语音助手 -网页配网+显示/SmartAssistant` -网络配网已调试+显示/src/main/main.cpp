#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include <WiFiClientSecure.h>
#include "HTTPClient.h"
#include "Audio1.h"
#include "Audio2.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include "Web_wifi.h"
#include "1.8TFT.h"
#include "SPEAK_SET.h"
#include "buzzer.h"

using namespace websockets;

#define key 0 // boot按键引脚

#define led3 2  // 回答指示灯
#define led2 21 // 唤醒指示灯

// 讯飞stt和大模型服务的参数
String APPID = "";                             // App ID,必填
String APIKey = "";    // API Key，必填
String APISecret = ""; // API Secret，必填

String appId1 = APPID;
String domain1 = "generalv3";

String websockets_server = "ws://spark-api.xf-yun.com/v3.1/chat";
String websockets_server1 = "ws://ws-api.xfyun.cn/v2/iat";

// 角色设定
String roleSet = "你是一个怎样的人，你自己猜";

// 讯飞stt语种设置
String language = "zh_cn"; // zh_cn：中文（支持简单的英文识别）en_us：English

// 定义一些全局变量

bool startPlay = false;

bool SYS_sign = false; // 系统初始化标志

bool lastsetence = false;
bool isReady = false;

unsigned long urlTime = 0;
unsigned long pushTime = 0;

int receiveFrame = 0;

int noise = 50;   // 噪声门限值
int volume = 100; // 初始音量大小（最小0，最大100）

HTTPClient https;

hw_timer_t *timer = NULL;

// 创建音频对象
Audio1 audio1;
Audio2 audio2(false, 3, I2S_NUM_1);
// 参数: 是否使用内部DAC（数模转换器）如果设置为true，将使用ESP32的内部DAC进行音频输出。否则，将使用外部I2S设备。
// 指定启用的音频通道。可以设置为1（只启用左声道）或2（只启用右声道）或3（启用左右声道）
// 指定使用哪个I2S端口。ESP32有两个I2S端口，I2S_NUM_0和I2S_NUM_1。可以根据需要选择不同的I2S端口。

#define I2S_DOUT 25 // DIN
#define I2S_BCLK 26 // BCLK
#define I2S_LRC 27  // LRC

// 函数声明
void gain_token(void);

void getText(String role, String content);

void checkLen();

float calculateRMS(uint8_t *buffer, int bufferSize);
void ConnServer();
void ConnServer1();
void Network_aut();
void getbaidutime(void);

void voicePlay();
void getTimeFromServer();
String getUrl(String Spark_url, String host, String path, String Date);
int Speak_Rec();
void ConnServer_xunfei();
void SYS_stua();
void processResponse(int status);
void removeChars(const char *input, char *output, const char *removeSet);
void response();

/*/ 创建动态JSON文档对象和数组
StaticJsonDocument<2048> doc;
JsonArray text = doc.to<JsonArray>();*/
// 使用动态JSON文档存储历史对话信息占用的内存过多，故改用c++中的vector向量
std::vector<String> text;

// 定义字符串变量，用于存储鉴权参数
String url = "";
String url1 = "";
String Date = "";

DynamicJsonDocument gen_params(const char *appid, const char *domain, const char *role_set);

String askquestion = "";        // 存储stt语音转文字信息，即用户的提问信息
String Answer = "";             // 存储llm回答，用于语音合成（较短的回答）
std::vector<String> subAnswers; // 存储llm回答，用于语音合成（较长的回答，分段存储）
int subindex = 0;               // subAnswers的下标，用于voicePlay()
int txt_num = 0;
int flag = 0; // 用来确保subAnswer1一定是大模型回答最开始的内容
bool speak_start = false;
using namespace websockets; // 使用WebSocket命名空间
// 创建WebSocket客户端对象
WebsocketsClient webSocketClient;  // 与llm通信
WebsocketsClient webSocketClient1; // 与stt通信

void setup()
{

    delay(3000);
    // 初始化串口通信，波特率为115200
    Serial.begin(115200);
    Serial2.begin(115200);

    Spk_init(); // 初始化喇叭接口
    // tft显示初始化
    tft_start();
    tft_chinese_start();
    sys_tft_start();
    Serial2.print("Start");

    // 配置引脚模式
    // 配置按键引脚为上拉输入模式，用于boot按键检测
    pinMode(key, INPUT_PULLUP);

    //  将led设置为输出模式
    pinMode(wifi_led, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(led3, OUTPUT);
    digitalWrite(wifi_led, HIGH);
    digitalWrite(led2, HIGH);
    digitalWrite(led3, HIGH);
    buzzer_init();

    // 初始化音频模块audio1
    audio1.init();
    // 设置音频输出引脚和音量
    audio2.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio2.setVolume(volume);
    delay(3000);
    digitalWrite(wifi_led, LOW);
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);

    sys_tft_start_wifi();
    Serial2.print("Connecting to WIFI");
    delay(3000);
    // 连接网络
    WIFI_sign = wifiConnect();

    getbaidutime();
    SYS_stua();
    sys_tft_start_ok();
    buzzer_init();
}

void loop()
{

    int Speak_sign = Speak_Rec();

    if (SYS_sign)
    {
        if (Speak_sign == 2 || Speak_sign == 7) // 重新连接WIFI、配网超时强制重连
        {
            if (WIFI_cont_sign)
            {
                // 断开网络后自动重新连接
                server.end();
                WiFi.softAPdisconnect(true);
                WIFI_cont_sign = false;
                if (Speak_sign == 7)
                    Serial2.print("The connection timed out, and the network distribution mode has been exited.");
            }

            WIFI_sign = wifiConnect();
            getbaidutime();
            wifi_stau_sign();
        }
        else if (Speak_sign == 5) // 网络设置
        {

            WIFI_cont_sign = true;
            digitalWrite(wifi_led, LOW);
            tft.fillScreen(TFT_WHITE);
            displayWrappedText("进入网络配置！",0,12,width);
           
            openWeb();
        }
        if (WIFI_sign == 3)
        {
            Serial2.print("Please add WIFI");
            WIFI_cont_sign = true;
            WIFI_sign = 4;
            digitalWrite(wifi_led, LOW);
            openWeb();
        }

        if (!WIFI_cont_sign)
        {
            if (WIFI_sign == 1)
            {
                if (WiFi.status() != WL_CONNECTED)
                {
                    // 断开当前WiFi连接
                    WiFi.disconnect(true);
                    WIFI_sign = 2;
                    digitalWrite(wifi_led, LOW);
                    Serial2.print("The WIFI network has been disconnected.");
                    delay(3000);
                }
                else
                {
                    //  轮询处理WebSocket客户端消息
                    webSocketClient.poll();
                    webSocketClient1.poll();

                    // 如果有多段语音需要播放
                    if (startPlay)
                        voicePlay(); // 调用voicePlay函数播放后续的语音

                    // 音频处理循环
                    audio2.loop();

                    // 如果音频正在播放
                    if (audio2.isplaying == 1)
                        digitalWrite(led3, HIGH); // 点亮互动指示灯
                    else
                    {
                        digitalWrite(led3, LOW); // 熄灭互动指示灯
                        // sys_tft_start_ok();
                        Spk_set_ASR(); // 喇叭控制权交回给ASR
                    }

                    // 检测boot按键是否按下或者被唤醒
                    if (digitalRead(key) == 0 || Speak_sign == 1)
                    {
                        webSocketClient.close(); // 关闭llm服务器，打断上一次提问的回答生成
                        Spk_set_98357();         // 喇叭控制权交回给98357
                        audio2.isplaying = 0;
                        startPlay = false;
                        isReady = false;

                        Answer = "";
                        flag = 0;
                        subindex = 0;
                        subAnswers.clear();
                        text_temp = "";
                        ConnServer_xunfei();
                    }
                }
            }
            else if (WIFI_sign == 2)
            {
                // 断开网络后自动重新连接
                WIFI_sign = wifiConnect();
                getbaidutime();
                wifi_stau_sign();
            }
            else if (WIFI_sign == 0)
            {
                if (Speak_sign == 4 && !WIFI_cont_sign) // wifi网络故障
                    Serial2.print("The device is not connected to WIFI. Please try again after connecting!");
            }
        }
    }
    else
    {

        if (Speak_sign == 3) // 系统故障标志
        {
            if (WIFI_sign != 1)
                Serial2.print("Network failure");
        }

        else if (Speak_sign == 6) // 重启系统
        {
            ;
        }
    }
}

// 移除讯飞星火回复中没用的符号
void removeChars(const char *input, char *output, const char *removeSet)
{
    int j = 0;
    for (int i = 0; input[i] != '\0'; ++i)
    {
        bool shouldRemove = false;
        for (int k = 0; removeSet[k] != '\0'; ++k)
        {
            if (input[i] == removeSet[k])
            {
                shouldRemove = true;
                break;
            }
        }
        if (!shouldRemove)
        {
            output[j++] = input[i];
        }
    }
    output[j] = '\0'; // 结束符
}

// 将回复的文本转成语音
void onMessageCallback(WebsocketsMessage message)
{
    // 创建一个静态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节，硬件限制，无法再增加
    StaticJsonDocument<1024> jsonDocument;

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    // 如果解析没有错误
    if (!error)
    {
        // 从JSON数据中获取返回码
        int code = jsonDocument["header"]["code"];

        // 如果返回码不为0，表示出错
        if (code != 0)
        {
            // 输出错误信息和完整的JSON数据
            Serial.print("sth is wrong: ");
            Serial.println(code);
            Serial.println(message.data());

            // 关闭WebSocket客户端
            webSocketClient.close();
        }
        else
        {
            // 获取JSON数据中的payload部分
            JsonObject choices = jsonDocument["payload"]["choices"];

            // 获取status状态
            int status = choices["status"];

            // 获取文本内容
            const char *content = choices["text"][0]["content"];
            const char *removeSet = "\n*$"; // 定义需要移除的符号集
            // 计算新字符串的最大长度
            int length = strlen(content) + 1;
            char *cleanedContent = new char[length];
            removeChars(content, cleanedContent, removeSet);
            Serial.println(cleanedContent);

            // 将内容追加到Answer字符串中
            Answer += cleanedContent;
            content = "";
            // 释放分配的内存
            delete[] cleanedContent;

            processResponse(status);
        }
    }
}

// 问题发送给讯飞星火大模型
void onEventsCallback(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to server0!");

        // 生成连接参数的JSON文档
        DynamicJsonDocument jsonData = gen_params(appId1.c_str(), domain1.c_str(), roleSet.c_str());

        // 将JSON文档序列化为字符串
        String jsonString;
        serializeJson(jsonData, jsonString);

        // 向串口输出生成的JSON字符串
        Serial.println(jsonString);

        // 通过WebSocket客户端发送JSON字符串到服务器
        webSocketClient.send(jsonString);
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection0 Closed");
    }
    // 当收到Ping消息时触发
    else if (event == WebsocketsEvent::GotPing)
    {
        // 向串口输出提示信息
        Serial.println("Got a Ping!");
    }
    // 当收到Pong消息时触发
    else if (event == WebsocketsEvent::GotPong)
    {
        // 向串口输出提示信息
        Serial.println("Got a Pong!");
    }
}

// 接收stt返回的语音识别文本并做相应的逻辑处理
void onMessageCallback1(WebsocketsMessage message)
{
    // 创建一个动态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节
    DynamicJsonDocument jsonDocument(4096);

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (error)
    {
        // 如果解析出错，输出错误信息和收到的消息数据
        Serial.println("error:");
        Serial.println(error.c_str());
        Serial.println(message.data());
        return;
    }
    // 如果解析没有错误，从JSON数据中获取返回码，如果返回码不为0，表示出错
    if (jsonDocument["code"] != 0)
    {
        // 输出完整的JSON数据
        Serial.println(message.data());
        // 关闭WebSocket客户端
        webSocketClient1.close();
    }
    else
    {
        // 输出收到的讯飞云返回消息
        Serial.println("xunfeiyun stt return message:");
        Serial.println(message.data());

        // 获取JSON数据中的结果部分，并提取文本内容
        JsonArray ws = jsonDocument["data"]["result"]["ws"].as<JsonArray>();

        if (jsonDocument["data"]["status"] != 2) // 处理流式返回的内容，讯飞stt最后一次会返回一个标点符号，需要和前一次返回结果拼接起来
        {
            askquestion = "";
        }

        for (JsonVariant i : ws)
        {
            for (JsonVariant w : i["cw"].as<JsonArray>())
            {
                askquestion += w["w"].as<String>();
            }
        }

        // 输出提取的问句
        Serial.println(askquestion);

        // 获取状态码，等于2表示文本已经转换完成
        if (jsonDocument["data"]["status"] == 2)
        {
            // 如果状态码为2，表示消息处理完成
            Serial.println("status == 2");
            webSocketClient1.close();

            // 如果是调声音大小还有开关灯的指令，就不打断当前的语音
            /* if ((askquestion.indexOf("声音") == -1 && askquestion.indexOf("音量") == -1) && !((askquestion.indexOf("开") > -1 || askquestion.indexOf("关") > -1) && askquestion.indexOf("灯") > -1) && !(askquestion.indexOf("暂停") > -1 || askquestion.indexOf("恢复") > -1))
             {
                 webSocketClient.close(); // 关闭llm服务器，打断上一次提问的回答生成
                 audio2.isplaying = 0;
                 startPlay = false;
                 Answer = "";
                 flag = 0;
                 subindex = 0;
                 subAnswers.clear();
                 text_temp = "";
             }*/

            // 如果问句为空，播放错误提示语音
            if (askquestion == "")
            {
                Answer = "莫斯没有听清，请再说一遍吧";
                response(); // 屏幕显示Answer以及语音播放
            }

            else // 处理一般的问答请求
            {
                tft.fillScreen(TFT_WHITE);
                tft.setCursor(0, 0);
                getText("user", askquestion);

                ConnServer();
            }
        }
    }
}

// 录音
void onEventsCallback1(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to xunfeiyun stt!");

        // 初始化变量
        int silence = 0;
        int firstframe = 1;
        int voicebegin = 0;
        int voice = 0;
        int null_voice = 0;

        // 创建一个静态JSON文档对象，2000一般够了，不够可以再加（最多不能超过4096），但是可能会发生内存溢出
        StaticJsonDocument<2000> doc;
        digitalWrite(led2, HIGH);
        buzzer_on_off();
        u8g2.setCursor(0, 159);
        u8g2.print("请说话！");

        Serial.println("开始录音");
        // 无限循环，用于录制和发送音频数据
        while (1)
        {

            // 清空JSON文档
            doc.clear();

            // 创建data对象
            JsonObject data = doc.createNestedObject("data");

            // 录制音频数据
            audio1.Record();

            // 计算音频数据的RMS值
            float rms = calculateRMS((uint8_t *)audio1.wavData[0], 1280);
            if (null_voice < 10 && rms > 1000) // 抑制录音初期奇奇怪怪的噪声
            {
                rms = 8.6;
            }
            printf("%d %f\n", 0, rms);

            if (null_voice >= 80) // 如果从录音开始过了8秒才说话，讯飞stt识别会超时，所以直接结束本次录音，重新开始录音
            {

                Answer = "录音超时，已结束录音";
                response(); // 屏幕显示Answer以及语音播放

                // 录音超时，断开本次连接
                webSocketClient1.close();
                Serial.println("录音结束");
                digitalWrite(led2, LOW);
                return;
            }

            // 判断是否为噪音
            if (rms < noise)
            {
                null_voice++;
                if (voicebegin == 1)
                {
                    silence++;
                }
            }
            else
            {
                if (null_voice > 0)
                    null_voice--;
                voice++;
                if (voice >= 5)
                {
                    voicebegin = 1;
                }
                else
                {
                    voicebegin = 0;
                }
                silence = 0;
            }

            // 如果静音达到8个周期，发送结束标志的音频数据
            if (silence == 8)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                delay(40);
                Serial.println("录音结束");
                digitalWrite(led2, LOW);
                break;
            }

            // 处理第一帧音频数据
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";

                JsonObject common = doc.createNestedObject("common");
                common["app_id"] = appId1.c_str();

                JsonObject business = doc.createNestedObject("business");
                business["domain"] = "iat";
                business["language"] = language.c_str();
                business["accent"] = "mandarin";
                // 不使用动态修正
                // business["vinfo"] = 1;
                // 使用动态修正
                business["dwa"] = "wpgs";
                business["vad_eos"] = 2000;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                firstframe = 0;
                delay(40);
            }
            else
            {
                // 处理后续帧音频数据
                data["status"] = 1;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                delay(40);
            }
        }
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection1 Closed");
    }
    // 当收到Ping消息时触发
    else if (event == WebsocketsEvent::GotPing)
    {
        // 向串口输出提示信息
        Serial.println("Got a Ping!");
    }
    // 当收到Pong消息时触发
    else if (event == WebsocketsEvent::GotPong)
    {
        // 向串口输出提示信息
        Serial.println("Got a Pong!");
    }
}

void ConnServer()
{
    // 向串口输出WebSocket服务器的URL
    Serial.println("url:" + url);
    // 设置WebSocket客户端的消息回调函数
    webSocketClient.onMessage(onMessageCallback);
    // 设置WebSocket客户端的事件回调函数
    webSocketClient.onEvent(onEventsCallback);
    // 开始连接WebSocket服务器
    Serial.println("Begin connect to server0......");
    // 尝试连接到WebSocket服务器
    if (webSocketClient.connect(url.c_str()))
    {
        // 如果连接成功，输出成功信息
        Serial.println("Connected to server0!");
    }
    else
    {
        // 如果连接失败，输出失败信息
        Serial.println("Failed to connect to server0!");
    }
}

void ConnServer1()
{
    // Serial.println("url1:" + url1);
    webSocketClient1.onMessage(onMessageCallback1);
    webSocketClient1.onEvent(onEventsCallback1);
    // Connect to WebSocket
    Serial.println("Begin connect to server1......");
    if (webSocketClient1.connect(url1.c_str()))
    {
        Serial.println("Connected to server1!");
    }
    else
    {
        Serial.println("Failed to connect to server1!");
    }
}

void voicePlay()
{
    // 检查音频是否正在播放以及回答内容是否为空
    if ((audio2.isplaying == 0) && (Answer != "" || subindex < subAnswers.size()))
    {
        if (subindex < subAnswers.size())
        {
            audio2.connecttospeech(subAnswers[subindex].c_str(), "zh");
            // 在屏幕上显示文字
            if (text_temp != "" && flag == 1)
            {
                // 清空屏幕
                tft.fillScreen(TFT_WHITE);
                // 显示剩余的文字
                displayWrappedText(text_temp.c_str(), 0, 11, width);
                text_temp = "";
                displayWrappedText(subAnswers[subindex].c_str(), u8g2.getCursorX(), u8g2.getCursorY(), width);
            }
            else if (flag == 1)
            {
                // 清空屏幕
                tft.fillScreen(TFT_WHITE);
                displayWrappedText(subAnswers[subindex].c_str(), 0, 11, width);
            }
            subindex++;
        }
        else
        {
            audio2.connecttospeech(Answer.c_str(), "zh");
            // 在屏幕上显示文字
            if (text_temp != "" && flag == 1)
            {
                // 清空屏幕
                tft.fillScreen(TFT_WHITE);
                // 显示剩余的文字
                displayWrappedText(text_temp.c_str(), 0, 11, width);
                text_temp = "";
                displayWrappedText(Answer.c_str(), u8g2.getCursorX(), u8g2.getCursorY(), width);
            }
            else if (flag == 1)
            {
                // 清空屏幕
                tft.fillScreen(TFT_WHITE);
                displayWrappedText(Answer.c_str(), 0, 11, width);
            }
            Answer = "";
        }
        // 设置开始播放标志
        startPlay = true;
    }
    else
    {
        // 如果音频正在播放或回答内容为空，不做任何操作
    }
}

String getUrl(String Spark_url, String host, String path, String Date)
{

    // 拼接字符串
    String signature_origin = "host: " + host + "\n";
    signature_origin += "date: " + Date + "\n";
    signature_origin += "GET " + path + " HTTP/1.1";
    // signature_origin="host: spark-api.xf-yun.com\ndate: Mon, 04 Mar 2024 19:23:20 GMT\nGET /v3.5/chat HTTP/1.1";

    // hmac-sha256 加密
    unsigned char hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t messageLength = signature_origin.length();
    const size_t keyLength = APISecret.length();
    // 初始化HMAC上下文
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    // 设置HMAC密钥
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)APISecret.c_str(), keyLength);
    // 更新HMAC上下文
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)signature_origin.c_str(), messageLength);
    // 完成HMAC计算
    mbedtls_md_hmac_finish(&ctx, hmac);
    // 释放HMAC上下文
    mbedtls_md_free(&ctx);

    // 将HMAC结果进行Base64编码
    String signature_sha_base64 = base64::encode(hmac, sizeof(hmac) / sizeof(hmac[0]));

    // 替换Date字符串中的特殊字符
    Date.replace(",", "%2C");
    Date.replace(" ", "+");
    Date.replace(":", "%3A");
    // 构建Authorization原始字符串
    String authorization_origin = "api_key=\"" + APIKey + "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"" + signature_sha_base64 + "\"";
    // 将Authorization原始字符串进行Base64编码
    String authorization = base64::encode(authorization_origin);
    // 构建最终的URL
    String url = Spark_url + '?' + "authorization=" + authorization + "&date=" + Date + "&host=" + host;
    // 向串口输出生成的URL
    Serial.println(url);
    // 返回生成的URL
    return url;
}

void getTimeFromServer()
{
    String timeurl = "https://www.baidu.com";                                    // 定义用于获取时间的URL
    HTTPClient http;                                                             // 创建HTTPClient对象
    http.begin(timeurl);                                                         // 初始化HTTP连接
    const char *headerKeys[] = {"Date"};                                         // 定义需要收集的HTTP头字段
    http.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0])); // 设置要收集的HTTP头字段
    int httpCode = http.GET();                                                   // 发送HTTP GET请求
    Date = http.header("Date");                                                  // 从HTTP响应头中获取Date字段
    Serial.println(Date);                                                        // 输出获取到的Date字段到串口
    http.end();                                                                  // 结束HTTP连接
    // delay(50); // 可以根据实际情况调整延时时间
}

// 显示文本
void getText(String role, String content)
{
    // 检查并调整文本长度
    checkLen();

    // 创建一个静态JSON文档，容量为512字节
    StaticJsonDocument<512> jsoncon;

    // 设置JSON文档中的角色和内容
    jsoncon["role"] = role;
    jsoncon["content"] = content;
    Serial.print("jsoncon：");
    Serial.println(jsoncon.as<String>());

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsoncon, jsonString);

    // 将字符串存储到vector中
    text.push_back(jsonString);

    // 输出vector中的内容
    for (const auto &jsonStr : text)
    {
        Serial.println(jsonStr);
    }

    /*/ 将生成的JSON文档添加到全局变量text中
    text.add(jsoncon);

    // 序列化全局变量text中的内容为字符串
    String serialized;
    serializeJson(text, serialized);

    // 输出序列化后的JSON字符串到串口
    Serial.print("text: ");
    Serial.println(serialized);*/

    // 清空临时JSON文档
    jsoncon.clear();

    // 打印角色
    tft.print(role);
    tft.print(": ");

    // 打印内容
    displayWrappedText(content.c_str(), tft.getCursorX(), tft.getCursorY() + 11, width);
    tft.setCursor(0, u8g2.getCursorY() + 2);

    // 也可以使用格式化的方式输出JSON，以下代码被注释掉了
    // serializeJsonPretty(text, Serial);
}

// 实时清理较早的历史对话记录

void checkLen()
{
    /*Serial.print("text size:");
    Serial.println(text.memoryUsage());
    // 计算jsonVector占用的字节数
    // 当JSON数组中的字符串总长度超过1600字节时，进入循环
    if (text.memoryUsage() > 1600)
    {
        // 移除数组中的第一对问答
        text.remove(0);
        text.remove(0);
    }*/
    size_t totalBytes = 0;

    // 计算vector中每个字符串的长度
    for (const auto &jsonStr : text)
    {
        totalBytes += jsonStr.length();
    }
    Serial.print("text size:");
    Serial.println(totalBytes);
    // 当vector中的字符串总长度超过800字节时，删除最开始的一对对话
    if (totalBytes > 800)
    {
        Serial.println("totalBytes大于800,删除最开始的一对对话");
        text.erase(text.begin(), text.begin() + 2);
    }
    // 函数没有返回值，直接修改传入的JSON数组
    // return textArray; // 注释掉的代码，表明此函数不返回数组
}

DynamicJsonDocument gen_params(const char *appid, const char *domain, const char *role_set)
{
    // 创建一个容量为1500字节的动态JSON文档
    DynamicJsonDocument data(1500);

    // 创建一个名为header的嵌套JSON对象，并添加app_id和uid字段
    JsonObject header = data.createNestedObject("header");
    header["app_id"] = appid;
    header["uid"] = "1234";

    // 创建一个名为parameter的嵌套JSON对象
    JsonObject parameter = data.createNestedObject("parameter");

    // 在parameter对象中创建一个名为chat的嵌套对象，并添加domain, temperature和max_tokens字段
    JsonObject chat = parameter.createNestedObject("chat");
    chat["domain"] = domain;
    chat["temperature"] = 0.6;
    chat["max_tokens"] = 1024;

    // 创建一个名为payload的嵌套JSON对象
    JsonObject payload = data.createNestedObject("payload");

    // 在payload对象中创建一个名为message的嵌套对象
    JsonObject message = payload.createNestedObject("message");

    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = message.createNestedArray("text");

    JsonObject systemMessage = textArray.createNestedObject();
    systemMessage["role"] = "system";
    systemMessage["content"] = role_set;

    // 遍历全局变量text中的每个元素，并将其添加到text数组中
    /*for (const auto &item : text)
    {
        textArray.add(item);
    }*/
    // 将jsonVector中的内容添加到JsonArray中
    for (const auto &jsonStr : text)
    {
        DynamicJsonDocument tempDoc(512);
        DeserializationError error = deserializeJson(tempDoc, jsonStr);
        if (!error)
        {
            textArray.add(tempDoc.as<JsonVariant>());
        }
        else
        {
            Serial.print("反序列化失败: ");
            Serial.println(error.c_str());
        }
    }
    // 返回构建好的JSON文档
    return data;
}
// 计算录音数据的均方根值
float calculateRMS(uint8_t *buffer, int bufferSize)
{
    float sum = 0;  // 初始化总和变量
    int16_t sample; // 定义16位整数类型的样本变量
    // 遍历音频数据缓冲区，每次处理两个字节（16位）
    for (int i = 0; i < bufferSize; i += 2)
    {
        // 将两个字节组合成一个16位的样本值
        sample = (buffer[i + 1] << 8) | buffer[i];
        // 将样本值平方后累加到总和中
        sum += sample * sample;
    }
    // 计算平均值（样本总数为bufferSize / 2）
    sum /= (bufferSize / 2);
    // 返回总和的平方根，即RMS值
    return sqrt(sum);
}

int Speak_Rec()
{
    String receivedStr = "";
    while (Serial2.available() > 0)
    {
        receivedStr += (char)Serial2.read();
    }
    if (receivedStr.length() > 0 && receivedStr.equals("Please speak"))
        return 1;
    else if (receivedStr.length() > 0 && receivedStr.equals("WIFI_Try"))
        return 2;
    else if (receivedStr.length() > 0 && receivedStr.equals("SYS_bad"))
        return 3;
    else if (receivedStr.length() > 0 && receivedStr.equals("WIFI_bad"))
        return 4;
    else if (receivedStr.length() > 0 && receivedStr.equals("WIFI_SET"))
        return 5;
    else if (receivedStr.length() > 0 && receivedStr.equals("SYS_RSET"))
        return 6;
    else if (receivedStr.length() > 0 && receivedStr.equals("WIFI_SET_TIME_OFF"))
        return 7;
    else
        return 0;
}

void getbaidutime(void)
{
    if (WIFI_sign == 1)
        Network_aut();
}

void SYS_stua()
{
    if (WIFI_sign || 0x01)
        SYS_sign = true;
    if (SYS_sign)
    {
        if (WIFI_sign == 1)
            Serial2.print("Welcome to use");

        if (WIFI_sign == 0)

            Serial2.print("Secondary module failure, please check and restart the relevant modules!");
        if (WIFI_sign == 2)
            Serial2.print("WIFI_NO");
    }
    else
    {
        Serial2.print("System startup failed");
    }
}

void Network_aut()
{
    // 从百度服务器获取当前时间
    getTimeFromServer();
    // 使用当前时间生成WebSocket连接的URL
    url = getUrl("ws://spark-api.xf-yun.com/v3.1/chat", "spark-api.xf-yun.com", "/v3.1/chat", Date);
    url1 = getUrl("ws://ws-api.xfyun.cn/v2/iat", "ws-api.xfyun.cn", "/v2/iat", Date);
    // 记录当前时间，用于后续时间戳比较
    urlTime = millis();
}

void ConnServer_xunfei()
{
    Serial.printf("Start recognition\r\n\r\n");
    askquestion = "";
    Network_aut(); // 网络鉴权
    // audio2.connecttospeech(askquestion.c_str(), "zh");
    // 连接到WebSocket服务器1讯飞stt
    ConnServer1();
    // ConnServer();
    // delay(6000);
    // audio1.Record();

    // Serial.println(text);
    // checkLen(text);
}

void processResponse(int status)
{
    // 如果Answer的长度超过180且音频没有播放
    if (Answer.length() >= 180 && (audio2.isplaying == 0) && flag == 0)
    {
        if (Answer.length() >= 300)
        {
            // 查找第一个句号的位置
            int firstPeriodIndex = Answer.indexOf("。");
            // 如果找到句号
            if (firstPeriodIndex != -1)
            {
                // 提取完整的句子并播放
                String subAnswer1 = Answer.substring(0, firstPeriodIndex + 1);
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);

                // 将提取的句子转换为语音
                audio2.connecttospeech(subAnswer1.c_str(), "zh");

                // 获取最终转换的文本
                getText("assistant", subAnswer1);

                flag = 1;

                // 更新Answer，去掉已处理的部分
                Answer = Answer.substring(firstPeriodIndex + 2);
                subAnswer1.clear();
                // 设置播放开始标志
                startPlay = true;
            }
        }
        else
        {
            // 查找最后一个逗号的位置
            int lastCommaIndex = Answer.lastIndexOf("，");
            if (lastCommaIndex != -1)
            {
                String subAnswer1 = Answer.substring(0, lastCommaIndex + 1);
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);
                audio2.connecttospeech(subAnswer1.c_str(), "zh");
                getText("assistant", subAnswer1);

                flag = 1;
                Answer = Answer.substring(lastCommaIndex + 2);
                subAnswer1.clear();
                startPlay = true;
            }
            else
            {
                String subAnswer1 = Answer.substring(0, Answer.length());
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);
                audio2.connecttospeech(subAnswer1.c_str(), "zh");
                getText("assistant", subAnswer1);

                flag = 1;
                Answer = Answer.substring(Answer.length());
                subAnswer1.clear();
                startPlay = true;
            }
        }
    }
    // 存储多段子音频
    while (Answer.length() >= 180)
    {
        if (Answer.length() >= 300)
        {
            // 查找第一个句号的位置
            int firstPeriodIndex = Answer.indexOf("。");
            // 如果找到句号
            if (firstPeriodIndex != -1)
            {
                subAnswers.push_back(Answer.substring(0, firstPeriodIndex + 1));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(firstPeriodIndex + 2);
            }
        }
        else
        {
            int lastCommaIndex = Answer.lastIndexOf("，");
            if (lastCommaIndex != -1)
            {
                subAnswers.push_back(Answer.substring(0, lastCommaIndex + 1));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(lastCommaIndex + 2);
            }
            else
            {
                subAnswers.push_back(Answer.substring(0, Answer.length()));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(Answer.length());
            }
        }
    }

    // 如果status为2（回复的内容接收完成），且回复的内容小于180字节
    if (status == 2 && flag == 0)
    {
        // 播放最终转换的文本
        audio2.connecttospeech(Answer.c_str(), "zh");
        // 显示最终转换的文本
        getText("assistant", Answer);

        Answer = "";
    }
}

void response()
{
    tft.fillScreen(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.print("assistant: ");
    audio2.connecttospeech(Answer.c_str(), "zh");
    displayWrappedText(Answer.c_str(), tft.getCursorX(), tft.getCursorY() + 11, width);
    Answer = "";
}