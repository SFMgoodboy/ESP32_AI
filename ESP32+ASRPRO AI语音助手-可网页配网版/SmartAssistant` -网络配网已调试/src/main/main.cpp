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

void checkLen(JsonArray textArray);
int getLength(JsonArray textArray);

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

DynamicJsonDocument doc(4000);
JsonArray text = doc.to<JsonArray>();

// 定义字符串变量，用于存储鉴权参数
String url = "";
String url1 = "";
String Date = "";

DynamicJsonDocument gen_params(const char *appid, const char *domain);

String askquestion = ""; // 存储stt语音转文字信息，即用户的提问信息
String Answer = "";      // 存储llm回答，用于语音合成（较短的回答）

const char *appId1 = ""; // 替换为自己的星火大模型参数
const char *domain1 = "generalv3";
const char *websockets_server = "ws://spark-api.xf-yun.com/v3.1/chat";
const char *websockets_server1 = "ws://ws-api.xfyun.cn/v2/iat";

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
                Serial2.print("The connection timed out, and the network distribution mode has been exited.");
            }

            WIFI_sign = wifiConnect();
            getbaidutime();
        }
        else if (Speak_sign == 5) // 网络设置
        {

            WIFI_cont_sign = true;
            digitalWrite(wifi_led, LOW);
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
        else if (WIFI_sign == 4)
        {
            wifi_num--;
            delay(10);
            if (!wifi_num)
            {

                WIFI_cont_sign = false;
                // 断开
                server.end();
                WiFi.softAPdisconnect(true);
                Serial2.print("The connection timed out, and the network distribution mode has been exited.");
                WIFI_sign = wifiConnect();
                getbaidutime();
            }
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
                        Spk_set_ASR();           // 喇叭控制权交回给ASR
                        digitalWrite(led3, LOW); // 熄灭互动指示灯
                    }

                    // 检测boot按键是否按下或者被唤醒
                    if (digitalRead(key) == 0 || Speak_sign == 1)
                    {

                        Spk_set_98357(); // 喇叭控制权交回给98357
                        audio2.isplaying = 0;
                        startPlay = false;
                        isReady = false;
                        Answer = "";

                        ConnServer_xunfei();
                    }
                }
            }
            else if (WIFI_sign == 2)
            {
                // 断开网络后自动重新连接
                WIFI_sign = wifiConnect();
                getbaidutime();
                if (WIFI_sign == 0)
                {
                    digitalWrite(wifi_led, LOW);
                    Serial2.print("Please check whether the WIFI device is faulty and the Wi-Fi connection failed!");
                    delay(2000);
                    // WIFI_sign = ;
                }
                else if (WIFI_sign == 1)
                {
                    sys_tft_start_ok();
                    Serial2.print("The system replies normally.");
                }
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
// 将回复的文本转成语音
void onMessageCallback(WebsocketsMessage message)
{
    // 创建一个静态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节，硬件限制，无法再增加
    StaticJsonDocument<4096> jsonDocument;
    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());
    // 如果解析没有错误
    if (!error)
    { // 从JSON数据中获取返回码
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
            receiveFrame++;
            Serial.print("receiveFrame:");
            Serial.println(receiveFrame);
            // 获取JSON数据中的payload部分
            JsonObject choices = jsonDocument["payload"]["choices"];
            // 获取status状态
            int status = choices["status"];
            // 获取文本内容
            const char *content = choices["text"][0]["content"];

            Serial.println(content);
            Answer += content;
            String answer = "";
            // 如果Answer的长度超过120且音频没有播放
            if (Answer.length() >= 120 && (audio2.isplaying == 0))
            {
                String subAnswer = Answer.substring(0, 120);
                Serial.print("subAnswer:");
                Serial.println(subAnswer);
                // 查找最后一个句号的位置
                int lastPeriodIndex = subAnswer.lastIndexOf("。");
                // 如果找到句号
                if (lastPeriodIndex != -1)
                {
                    // 提取完整的句子并播放
                    answer = Answer.substring(0, lastPeriodIndex + 1);
                    Serial.print("answer: ");
                    Serial.println(answer);
                    // 更新Answer，去掉已处理的部分
                    Answer = Answer.substring(lastPeriodIndex + 2);
                    Serial.print("Answer: ");
                    Serial.println(Answer);
                    // 将提取的句子转换为语音
                    audio2.connecttospeech(answer.c_str(), "zh");
                }
                else
                {
                    const char *chinesePunctuation = "？，：；,.";

                    int lastChineseSentenceIndex = -1;

                    for (int i = 0; i < Answer.length(); ++i)
                    {
                        char currentChar = Answer.charAt(i);

                        if (strchr(chinesePunctuation, currentChar) != NULL)
                        {
                            lastChineseSentenceIndex = i;
                        }
                    }
                    if (lastChineseSentenceIndex != -1)
                    {
                        answer = Answer.substring(0, lastChineseSentenceIndex + 1);
                        audio2.connecttospeech(answer.c_str(), "zh");
                        Answer = Answer.substring(lastChineseSentenceIndex + 2);
                    }
                    else
                    {
                        answer = Answer.substring(0, 120);
                        audio2.connecttospeech(answer.c_str(), "zh");
                        Answer = Answer.substring(120 + 1);
                    }
                }
                startPlay = true;
            }
            // 如果status为2（回复的内容接收完成）
            if (status == 2)
            {
                // 显示最终转换的文本
                getText("assistant", Answer);
                // 如果Answer的长度超过80且音频没有播放
                if (Answer.length() <= 80 && (audio2.isplaying == 0))
                {
                    // getText("assistant", Answer);
                    // 播放最终转换的文本
                    audio2.connecttospeech(Answer.c_str(), "zh");
                }
            }
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
        DynamicJsonDocument jsonData = gen_params(appId1, domain1);
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
    StaticJsonDocument<4096> jsonDocument;
    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (!error)
    {
        // 如果解析没有错误，从JSON数据中获取返回码，
        int code = jsonDocument["code"];
        // 如果返回码不为0，表示出错
        if (code != 0)
        {
            // 输出完整的JSON数据
            Serial.println(code);
            Serial.println(message.data());
            // 关闭WebSocket客户端
            webSocketClient1.close();
        }
        else
        {
            // 输出收到的讯飞云返回消息
            Serial.println("xunfeiyun return message:");
            Serial.println(message.data());
            // 获取JSON数据中的结果部分，并提取文本内容
            JsonArray ws = jsonDocument["data"]["result"]["ws"].as<JsonArray>();

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
            int status = jsonDocument["data"]["status"];
            if (status == 2)
            {
                // 如果状态码为2，表示消息处理完成
                Serial.println("status == 2");
                // 关闭WebSocket客户端
                webSocketClient1.close();

                // 如果问句为空，播放错误提示语音
                if (askquestion == "")
                {
                    // 提示我没有语句
                    askquestion = "sorry, i can't hear you";
                    audio2.connecttospeech(askquestion.c_str(), "zh");
                }

                else
                {
                    getText("user", askquestion);
                    Serial.print("text:");
                    Serial.println(text);
                    Answer = "";
                    lastsetence = false;
                    isReady = true;
                    ConnServer();
                }
            }
        }
    }
    else
    {
        Serial.println("error:");
        Serial.println(error.c_str());
        Serial.println(message.data());
    }
}

void onEventsCallback1(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to xunfeiyun");
        digitalWrite(led2, HIGH);
        buzzer_init();
        int silence = 0;
        int firstframe = 1;
        int j = 0;
        int voicebegin = 0;
        int voice = 0;

        DynamicJsonDocument doc(2500);
        while (1)
        {
            doc.clear();
            JsonObject data = doc.createNestedObject("data");
            audio1.Record();
            float rms = calculateRMS((uint8_t *)audio1.wavData[0], 1280);
            printf("%d %f\n", 0, rms);
            if (rms < noise)
            {
                if (voicebegin == 1)
                {
                    silence++;
                    // Serial.print("noise:");
                    // Serial.println(noise);
                }
            }
            else
            {
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
            if (silence == 6)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";
                j++;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                digitalWrite(led2, LOW);
                delay(40);
                break;
            }
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=8000";
                data["audio"] = base64::encode((byte *)audio1.wavData[0], 1280);
                data["encoding"] = "raw";
                j++;

                JsonObject common = doc.createNestedObject("common");
                common["app_id"] = appId1;

                JsonObject business = doc.createNestedObject("business");
                business["domain"] = "iat";
                business["language"] = "zh_cn";
                business["accent"] = "mandarin";
                business["vinfo"] = 1;
                business["vad_eos"] = 1000;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                firstframe = 0;
                delay(40);
            }
            else
            {
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
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("Connnection1 Closed");
    }
    else if (event == WebsocketsEvent::GotPing)
    {
        Serial.println("Got a Ping!");
    }
    else if (event == WebsocketsEvent::GotPong)
    {
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
    if ((audio2.isplaying == 0) && Answer != "")
    {
        // String subAnswer = "";
        // String answer = "";
        // if (Answer.length() >= 100)
        //     subAnswer = Answer.substring(0, 100);
        // else
        // {
        //     subAnswer = Answer.substring(0);
        //     lastsetence = true;
        //     // startPlay = false;
        // }

        // Serial.print("subAnswer:");
        // Serial.println(subAnswer);
        // 查找第一个句号的位置
        int firstPeriodIndex = Answer.indexOf("。");
        int secondPeriodIndex = 0;
        // 如果找到句号
        if (firstPeriodIndex != -1)
        {
            secondPeriodIndex = Answer.indexOf("。", firstPeriodIndex + 1);
            if (secondPeriodIndex == -1)
                secondPeriodIndex = firstPeriodIndex;
        }
        else
        {
            secondPeriodIndex = firstPeriodIndex;
        }

        if (secondPeriodIndex != -1)
        {
            String answer = Answer.substring(0, secondPeriodIndex + 1);
            Serial.print("answer: ");
            Serial.println(answer);
            Answer = Answer.substring(secondPeriodIndex + 2);
            audio2.connecttospeech(answer.c_str(), "zh");
        }
        else
        {
            const char *chinesePunctuation = "？，：；,.";

            int lastChineseSentenceIndex = -1;

            for (int i = 0; i < Answer.length(); ++i)
            {
                char currentChar = Answer.charAt(i);

                if (strchr(chinesePunctuation, currentChar) != NULL)
                {
                    lastChineseSentenceIndex = i;
                }
            }

            if (lastChineseSentenceIndex != -1)
            {
                String answer = Answer.substring(0, lastChineseSentenceIndex + 1);
                audio2.connecttospeech(answer.c_str(), "zh");
                Answer = Answer.substring(lastChineseSentenceIndex + 2);
            }
        }
        // 设置开始播放标志
        startPlay = true;
    }
    else
    {
        // digitalWrite(led3, LOW);// 如果音频正在播放或回答内容为空，不做任何操作
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

// 通过串口显示文本
void getText(String role, String content)
{
    checkLen(text);                    // 检查并调整文本长度
    DynamicJsonDocument jsoncon(1024); // 创建一个静态JSON文档，容量为1024字节
    // 设置JSON文档中的角色和内容
    jsoncon["role"] = role;
    jsoncon["content"] = content;
    // 将生成的JSON文档添加到全局变量text中
    text.add(jsoncon);
    // 清空临时JSON文档
    jsoncon.clear();
    // 序列化全局变量text中的内容为字符串
    String serialized;
    serializeJson(text, serialized);
    // 输出序列化后的JSON字符串到串口
    Serial.print("text: ");
    Serial.println(serialized);
    // 也可以使用格式化的方式输出JSON，以下代码被注释掉了
    // serializeJsonPretty(text, Serial);
}

int getLength(JsonArray textArray)
{
    int length = 0;
    for (JsonObject content : textArray)
    {
        const char *temp = content["content"];
        int leng = strlen(temp);
        length += leng;
    }
    return length;
}
// 实时清理较早的历史对话记录
void checkLen(JsonArray textArray)
{
    while (getLength(textArray) > 3000)
    {
        textArray.remove(0);
    }
    // return textArray;
}

DynamicJsonDocument gen_params(const char *appid, const char *domain)
{
    // 创建一个容量为1500字节的动态JSON文档
    DynamicJsonDocument data(2048);
    // 创建一个名为header的嵌套JSON对象，并添加app_id和uid字段
    JsonObject header = data.createNestedObject("header");
    header["app_id"] = appid;
    header["uid"] = "1234";
    // 创建一个名为parameter的嵌套JSON对象
    JsonObject parameter = data.createNestedObject("parameter");
    // 在parameter对象中创建一个名为chat的嵌套对象，并添加domain, temperature和max_tokens字段
    JsonObject chat = parameter.createNestedObject("chat");
    chat["domain"] = domain;
    chat["temperature"] = 0.5;
    chat["max_tokens"] = 1024;
    // 创建一个名为payload的嵌套JSON对象
    JsonObject payload = data.createNestedObject("payload");
    // 在payload对象中创建一个名为message的嵌套对象
    JsonObject message = payload.createNestedObject("message");
    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = message.createNestedArray("text");
    // 遍历全局变量text中的每个元素，并将其添加到text数组中
    for (const auto &item : text)
    {
        textArray.add(item);
    }
    return data; // 返回构建好的JSON文档
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