#include "Web_wifi.h"

// AP模式的SSID和密码
const char *ap_ssid = "ESP32-Setup";
const char *ap_password = "12345678";
// Web服务器和Preferences对象
AsyncWebServer server(80);
Preferences preferences;

bool ledstatus = true; // 控制led闪烁
bool WIFI_cont_sign = false;

int WIFI_tinme = 0;
int WIFI_con_time = 0;
int WIFI_sign = 0; // WiFi连接标志
int wifi_num = 6000;
int wifi_choose_num = 0;
int wifi_set_sign = 0;

void openWeb()
{

    // 网络连接失败，启动 AP 模式创建热点用于配网和音乐信息添加
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Started Access Point");
    // 启动 Web 服务器
    server.on("/", HTTP_GET, handleRoot);
    server.on("/wifi", HTTP_GET, handleWifiManagement);

    server.on("/save", HTTP_POST, handleSave);
    server.on("/delete", HTTP_POST, handleDelete);
    server.on("/list", HTTP_GET, handleList);

    server.begin();
    Serial.println("WebServer started, waiting for configuration...");
}
// 处理根路径的请求
void handleRoot(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Configuration</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } a { display: inline-block; padding: 10px 20px; margin: 10px; border: none; background-color: #333; color: white; text-decoration: none; cursor: pointer; } a:hover { background-color: #555; }</style></head><body><h1>ESP32 Configuration</h1><a href='/wifi'>Wi-Fi Management</a><a href='/music'>Music Management</a></body></html>";
    request->send(200, "text/html", html);
}
// wifi配置界面
void handleWifiManagement(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Wi-Fi Management</title><style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } form { display: inline-block; margin-top: 20px; } input[type='text'], input[type='password'] { padding: 10px; margin: 10px 0; width: 200px; } input[type='submit'], input[type='button'] { padding: 10px 20px; margin: 10px 5px; border: none; background-color: #333; color: white; cursor: pointer; } input[type='submit']:hover, input[type='button']:hover { background-color: #555; }</style></head><body><h1>Wi-Fi Management</h1><form action='/save' method='post'><label for='ssid'>Wi-Fi SSID:</label><br><input type='text' id='ssid' name='ssid'><br><label for='password'>Password:</label><br><input type='password' id='password' name='password'><br><input type='submit' value='Save'></form><form action='/delete' method='post'><label for='ssid'>Wi-Fi SSID to Delete:</label><br><input type='text' id='ssid' name='ssid'><br><input type='submit' value='Delete'></form><a href='/list'><input type='button' value='List Wi-Fi Networks'></a><p><a href='/'>Go Back</a></p></body></html>";
    request->send(200, "text/html", html);
}

// 添加或更新wifi信息逻辑
void handleSave(AsyncWebServerRequest *request)
{
    displayWrappedText("请添加或者更新无线网络",0,u8g2.getCursorY(),width);
    
    Serial.println("Start Save!");
    String ssid = request->arg("ssid");
    String password = request->arg("password");

    preferences.begin("wifi_store", false);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String storedSsid = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (storedSsid == ssid)
        {
            preferences.putString(("password" + String(i)).c_str(), password);
            u8g2.setCursor(0, 11 + 12);
            u8g2.print("wifi密码更新成功！");
            Serial.println("Succeess Update!");
            Serial2.print("The wireless network has been updated successfully.");
            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Configuration Updated!</h1><p>Network password updated successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }

    preferences.putString(("ssid" + String(numNetworks)).c_str(), ssid);
    preferences.putString(("password" + String(numNetworks)).c_str(), password);
    preferences.putInt("numNetworks", numNetworks + 1);
    u8g2.setCursor(0, 11 + 12);
    u8g2.print("新wifi添加成功！");
    Serial.println("Succeess Save!");
    Serial2.print("The wireless network has been added successfully.");
    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Configuration Saved!</h1><p>Network information added successfully.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}
// 删除wifi信息逻辑
void handleDelete(AsyncWebServerRequest *request)
{
    tft.fillScreen(TFT_WHITE);
    u8g2.setCursor(0, 11);
    u8g2.print("进入网络配置！");
    Serial.println("Start Delete!");
    String ssidToDelete = request->arg("ssid");

    preferences.begin("wifi_store", false);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String storedSsid = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (storedSsid == ssidToDelete)
        {
            preferences.remove(("ssid" + String(i)).c_str());
            preferences.remove(("password" + String(i)).c_str());

            for (int j = i; j < numNetworks - 1; ++j)
            {
                String nextSsid = preferences.getString(("ssid" + String(j + 1)).c_str(), "");
                String nextPassword = preferences.getString(("password" + String(j + 1)).c_str(), "");

                preferences.putString(("ssid" + String(j)).c_str(), nextSsid);
                preferences.putString(("password" + String(j)).c_str(), nextPassword);
            }

            preferences.remove(("ssid" + String(numNetworks - 1)).c_str());
            preferences.remove(("password" + String(numNetworks - 1)).c_str());
            preferences.putInt("numNetworks", numNetworks - 1);
            u8g2.setCursor(0, 11 + 12);
            u8g2.print("wifi删除成功！");
            Serial.println("Succeess Delete!");
            Serial2.print("A WIFI account and password have been deleted.");
            request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Network Deleted!</h1><p>The network has been deleted.</p><p><a href='/'>Go Back</a></p></body></html>");
            preferences.end();
            return;
        }
    }
    u8g2.setCursor(0, 11 + 12);
    u8g2.print("该wifi不存在！");
    Serial.println("Fail to Delete!");
    Serial2.print("The wireless network account does not exist.");
    request->send(200, "text/html", "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Network Not Found!</h1><p>The specified network was not found.</p><p><a href='/'>Go Back</a></p></body></html>");
    preferences.end();
}
// 显示已有wifi信息逻辑
void handleList(AsyncWebServerRequest *request)
{
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Saved Wi-Fi Networks</h1><ul>";

    preferences.begin("wifi_store", true);
    int numNetworks = preferences.getInt("numNetworks", 0);

    for (int i = 0; i < numNetworks; ++i)
    {
        String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
        String password = preferences.getString(("password" + String(i)).c_str(), "");
        html += "<li>ssid" + String(i) + ": " + ssid + " " + password + "</li>";
    }

    html += "</ul><p><a href='/'>Go Back</a></p></body></html>";

    request->send(200, "text/html", html);
    preferences.end();
}

int wifiConnect()
{
    // 断开当前WiFi连接
    WiFi.disconnect(true);

    preferences.begin("wifi_store");
    int numNetworks = preferences.getInt("numNetworks", 0);
    if (numNetworks == 0)
    {
        Serial2.print("NO WIFI");

        // 在屏幕上输出提示信息
        u8g2.setCursor(0, u8g2.getCursorY() + 12);
        u8g2.print("无任何wifi存储信息！");
        displayWrappedText("请连接热点ESP32-Setup密码为12345678，然后在浏览器中打开http://192.168.4.1添加新的网络！", 0, u8g2.getCursorY() + 12, width);

        preferences.end();
        return 3;
    }
    else
    {
        // 获取存储的 WiFi 配置
        for (int i = 0; i < numNetworks; ++i)
        {
            if (WIFI_sign == 2)
            {
                if (wifi_choose_num < numNetworks)
                    i = wifi_choose_num + 1;
                else
                {

                    break;
                }
            }

            String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
            String password = preferences.getString(("password" + String(i)).c_str(), "");

            // 尝试连接到存储的 WiFi 网络
            if (ssid.length() > 0 && password.length() > 0)
            {
                Serial.print("Connecting to ");
                Serial.println(ssid);
                Serial.print("password:");
                Serial.println(password);
                // 在屏幕上显示每个网络的连接情况
                u8g2.setCursor(0, u8g2.getCursorY() + 12);
                u8g2.print(ssid);
                uint8_t count = 0;
                WiFi.begin(ssid.c_str(), password.c_str());
                // 等待WiFi连接成功
                while (WiFi.status() != WL_CONNECTED)
                {
                    // 闪烁板载LED以指示连接状态
                    digitalWrite(wifi_led, ledstatus);
                    ledstatus = !ledstatus;
                    count++;

                    // 如果尝试连接超过30次，则认为连接失败
                    if (count >= 30)
                    {
                        Serial.printf("\r\n-- wifi connect fail! --\r\n");
                        digitalWrite(wifi_led, LOW);
                        // 在屏幕上显示连接失败信息
                        u8g2.setCursor(0, u8g2.getCursorY() + 12);
                        u8g2.print("Failed!");
                        break;
                    }

                    // 等待100毫秒
                    vTaskDelay(100);
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    wifi_choose_num = i;
                    for (int j = 0, rssi = 0; j < 20; j++)
                    {
                        rssi = -WiFi.RSSI();
                        if (rssi <= 70)
                        {
                            WIFI_tinme++;
                            delay(20);
                        }
                    }
                    if (WIFI_tinme >= 15)
                    {
                        // 向串口输出连接成功信息和IP地址
                        Serial.printf("\r\n-- wifi connect success! --\r\n");
                        Serial.print("IP address: ");
                        Serial.println(WiFi.localIP());
                        // 输出当前空闲堆内存大小
                        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
                        // 在屏幕上显示连接成功信息
                        u8g2.setCursor(0, u8g2.getCursorY() + 12);
                        u8g2.print("Connected!");
                        digitalWrite(wifi_led, HIGH);
                        Serial2.printf("wifi connect success!");
                        preferences.end();

                        return 1;
                    }
                    else
                    {

                        return 2;
                    }
                }
            }
        }

        preferences.end();
        return 0;
    }
}

void wifi_stau_sign(void)
{

    if (WIFI_sign == 0)
    {
        digitalWrite(wifi_led, LOW);
        Serial2.print("Please check whether the WIFI device is faulty and the Wi-Fi connection failed!");
        delay(2000);
    }
    else if (WIFI_sign == 1)
    {
        sys_tft_start_ok();
        Serial2.print("The system replies normally.");
    }
}