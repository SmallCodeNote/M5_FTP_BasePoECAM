// #include <M5Unified.h>
// #include <Arduino.h>
#include "M5PoECAM.h"
#include <SPI.h>
#include <M5_Ethernet.h>
#include <time.h>
#include <EEPROM.h>
#include "M5_Ethernet_FtpClient.hpp"
#include "M5_Ethernet_NtpClient.hpp"

// == M5Basic_Bus ==
/*#define SCK  18
#define MISO 19
#define MOSI 23
#define CS   26
*/

// == M5CORES2_Bus ==
/*#define SCK  18
#define MISO 38
#define MOSI 23
#define CS   26
*/

// == M5PoECAM_Bus ==
#define SCK 23
#define MISO 38
#define MOSI 13
#define CS 4

// == M5CORES3_Bus/M5CORES3_SE_Bus ==
/*#define SCK 36
#define MISO 35
#define MOSI 37
#define CS 9
*/

#define STORE_DATA_SIZE 256 // byte

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress deviceIP(192, 168, 25, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer HttpServer(80);
EthernetClient FtpClient(21);

String ftp_address = "192.168.25.77";
String ftp_user = "ftpusr";
String ftp_pass = "ftpword";

M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

String ntp_address = "192.168.25.77";

char textArray[] = "textArray";

void HTTP_UI();
void ShotTask(void *arg);
void TimeUpdateTask(void *arg);
void HTML_UI_Task(void *arg);
void ButtonKeepCount_Task(void *arg);

/// @brief Encorder Profile Struct
struct DATA_SET
{
  /// @brief IP address
  IPAddress deviceIP;
  IPAddress ftpSrvIP;
  IPAddress ntpSrvIP;

  u_int16_t interval;

  /// @brief deviceName
  String deviceName;
  String ftp_user;
  String ftp_pass;
};
/// @brief Encorder Profile
DATA_SET storeData;
String shotInterval;

String deviceName = "Device";
String deviceIP_String = "";
String ftpSrvIP_String = "";
String ntpSrvIP_String = "";

void LoadEEPROM()
{
  EEPROM.begin(STORE_DATA_SIZE);
  EEPROM.get<DATA_SET>(0, storeData);

  deviceName = storeData.deviceName;
  deviceIP = storeData.deviceIP;
  deviceIP_String = storeData.deviceIP.toString();
  ftpSrvIP_String = storeData.ftpSrvIP.toString();
  ntpSrvIP_String = storeData.ntpSrvIP.toString();
  ftp_user = storeData.ftp_user;
  ftp_pass = storeData.ftp_pass;
  shotInterval = String(storeData.interval);

  Serial.println("LoadEEPROM: interval = " + String(storeData.interval));
}

void PutEEPROM()
{
  storeData.deviceName = deviceName;
  storeData.deviceIP.fromString(deviceIP_String);
  storeData.ntpSrvIP.fromString(ntpSrvIP_String);
  storeData.ftpSrvIP.fromString(ftpSrvIP_String);
  storeData.ftp_user = ftp_user;
  storeData.ftp_pass = ftp_pass;
  storeData.interval = (u_int16_t)(shotInterval.toInt());

  Serial.println("PutEEPROM: interval = " + String(storeData.interval));

  EEPROM.put<DATA_SET>(0, storeData);
  EEPROM.commit();
}

void InitEEPROM()
{
  storeData.deviceName = "PoECAM-W";
  storeData.deviceIP.fromString("192.168.25.177");
  storeData.ntpSrvIP.fromString("192.168.25.77");
  storeData.ftpSrvIP.fromString("192.168.25.77");
  storeData.ftp_user = "ftpusr";
  storeData.ftp_pass = "ftpword";
  storeData.interval = 10;

  EEPROM.put<DATA_SET>(0, storeData);
  EEPROM.commit();
}

void EthernetBegin()
{
  SPI.begin(SCK, MISO, MOSI, -1);
  Ethernet.init(CS);
  Ethernet.begin(mac, deviceIP);
}

void setup()
{
  PoECAM.begin();

  if (!PoECAM.Camera.begin())
  {
    Serial.println("Camera Init Fail");
    return;
  }

  PoECAM.Camera.sensor->set_pixformat(PoECAM.Camera.sensor, PIXFORMAT_JPEG);
  PoECAM.Camera.sensor->set_framesize(PoECAM.Camera.sensor, FRAMESIZE_QVGA);
  PoECAM.Camera.sensor->set_vflip(PoECAM.Camera.sensor, 1);
  PoECAM.Camera.sensor->set_hmirror(PoECAM.Camera.sensor, 0);
  PoECAM.Camera.sensor->set_gain_ctrl(PoECAM.Camera.sensor, 0);

  Serial.println("MAX_SOCK_NUM = " + String(MAX_SOCK_NUM));
  if (MAX_SOCK_NUM < 8)
    Serial.println("need overwrite MAX_SOCK_NUM = 8 in M5_Ethernet.h");

  LoadEEPROM();

  EthernetBegin();
  HttpServer.begin();

  Serial.println("server is at " + Ethernet.localIP());

  NtpClient.begin();
  NtpClient.getTime(ntp_address, +9);

  M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

  delay(100);
  xTaskCreatePinnedToCore(TimeUpdateTask, "TimeUpdateTask", 4096, NULL, 2, NULL, 0);
  delay(100);
  xTaskCreatePinnedToCore(ShotTask, "ShotTask", 4096, NULL, 3, NULL, 1);
  delay(100);
  xTaskCreatePinnedToCore(HTML_UI_Task, "HTML_UI_Task", 4096, NULL, 4, NULL, 0);
  delay(100);
  xTaskCreatePinnedToCore(ButtonKeepCount_Task, "ButtonKeepCount_Task", 4096, NULL, 5, NULL, 1);
}

void loop()
{
  delay(10000);
}


void TimeUpdateTask(void *arg)
{
  Serial.println("TimeUpdateTask Start  ");
  unsigned long lastepoc = 0;

  while (1)
  {
    delay(89);
    String timeLine = NtpClient.getTime(ntp_address, +9);
    if (lastepoc > NtpClient.currentEpoch)
      lastepoc = 0;

    if (lastepoc + 10 <= NtpClient.currentEpoch)
    {
      Serial.println(timeLine);
      lastepoc = NtpClient.currentEpoch;
    }
    // vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void ShotTask(void *arg)
{
  Serial.println("ShotTask Start  ");
  if (storeData.interval < 1)
    storeData.interval = 1;

  unsigned long lastWriteEpoc = 0;
  unsigned long lastEpoc = 0;

  unsigned long offset = 0;

  if (!(storeData.interval % 10) || !(storeData.interval % 5) || !(storeData.interval % 3600) || !(storeData.interval % 600) || !(storeData.interval % 300))
  {
    offset = 1;
  }

  while (1)
  {
    delay(13);

    unsigned long currentEpoch = NtpClient.currentEpoch;

    if (currentEpoch - lastEpoc > 1)
    {
      Serial.println(">>EpocDeltaOver 1:");
    }
    lastEpoc = currentEpoch;

    if (lastWriteEpoc + storeData.interval <= currentEpoch + offset)
    {

      String ss = NtpClient.readSecond(currentEpoch);

      if (!(storeData.interval % 10))
      {
        if (ss[1] != '0')
        {
          continue;
        }
      }
      else if (!(storeData.interval % 5))
      {
        if ((ss[1] != '0' && ss[1] != '5'))
        {
          continue;
        }
      }
      String mm = NtpClient.readMinute(currentEpoch);
      if (!(storeData.interval % 3600))
      {
        if (mm[0] != '0' && mm[1] != '0')
        {
          continue;
        }
      }
      else if (!(storeData.interval % 600))
      {
        if (mm[1] != '0')
        {
          continue;
        }
      }
      else if (!(storeData.interval % 300))
      {
        if (mm[1] != '5' && mm[1] != '0')
        {
          continue;
        }
      }

      String HH = NtpClient.readHour(currentEpoch);
      String DD = NtpClient.readDay(currentEpoch);
      String MM = NtpClient.readMonth(currentEpoch);
      String YYYY = NtpClient.readYear(currentEpoch);

      String directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD;
      String filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm + ss;

      if (PoECAM.Camera.get())
      {
        ftp.OpenConnection();
        ftp.MakeDirRecursive(directoryPath);
        ftp.AppendData(filePath + ".jpg", (u_char *)(PoECAM.Camera.fb->buf), (int)(PoECAM.Camera.fb->len));
        ftp.CloseConnection();
        PoECAM.Camera.free();
      }

      unsigned long nowEpoch = NtpClient.currentEpoch;
      if (nowEpoch < lastWriteEpoc)
      {
        lastWriteEpoc = 0;
      }
      else
      {
        lastWriteEpoc = nowEpoch;
      }
    }
  }
}

void HTML_UI_Task(void *arg)
{
  Serial.println("HTML_UI_Task Start  ");
  while (1)
  {
    delay(209);

    HTTP_UI();
    Ethernet.maintain();
  }
}

#define HTTP_GET_PARAM_FROM_POST(paramName)                                              \
  {                                                                                      \
    int start##paramName = currentLine.indexOf(#paramName "=") + strlen(#paramName "="); \
    int end##paramName = currentLine.indexOf("&", start##paramName);                     \
    if (end##paramName == -1)                                                            \
    {                                                                                    \
      end##paramName = currentLine.length();                                             \
    }                                                                                    \
    paramName = currentLine.substring(start##paramName, end##paramName);                 \
    Serial.println(#paramName ": " + paramName);                                         \
  }

#define HTML_PUT_INFOWITHLABEL(labelString) \
  client.print(#labelString ": ");          \
  client.print(labelString);                \
  client.println("<br />");

#define HTML_PUT_LI_INPUT(inputName)                                                             \
  {                                                                                              \
    client.println("<li>");                                                                      \
    client.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    client.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    client.print(inputName);                                                                     \
    client.println("\" required>");                                                              \
    client.println("</li>");                                                                     \
  }

void HTTP_UI()
{
  EthernetClient client = HttpServer.available();
  if (client)
  {
    Serial.println("new client");
    boolean currentLineIsBlank = true;
    String currentLine = "";
    bool isPost = false;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          if (isPost)
          {
            // Load post data
            while (client.available())
            {
              char c = client.read();
              if (c == '\n' && currentLine.length() == 0)
              {
                break;
              }
              currentLine += c;
            }

            HTTP_GET_PARAM_FROM_POST(deviceName);
            HTTP_GET_PARAM_FROM_POST(deviceIP_String);
            HTTP_GET_PARAM_FROM_POST(ntpSrvIP_String);
            HTTP_GET_PARAM_FROM_POST(ftpSrvIP_String);
            HTTP_GET_PARAM_FROM_POST(ftp_user);
            HTTP_GET_PARAM_FROM_POST(ftp_pass);
            HTTP_GET_PARAM_FROM_POST(shotInterval);

            PutEEPROM();
            delay(1000);
            ESP.restart();
          }

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
          client.println("<h1>PoE CAM Unit</h1>");
          client.println("<br />");

          client.println("<form action=\"/\" method=\"post\">");
          client.println("<ul>");

          HTML_PUT_LI_INPUT(deviceName);
          HTML_PUT_LI_INPUT(deviceIP_String);
          HTML_PUT_LI_INPUT(ntpSrvIP_String);
          HTML_PUT_LI_INPUT(ftpSrvIP_String);
          HTML_PUT_LI_INPUT(ftp_user);
          HTML_PUT_LI_INPUT(ftp_pass);
          HTML_PUT_LI_INPUT(shotInterval);

          client.println("<li class=\"button\">");
          client.println("<button type=\"submit\">Save</button>");
          client.println("</li>");

          client.println("</ul>");
          client.println("</form>");
          client.println("</body>");
          client.println("</html>");

          break;
        }
        if (c == '\n')
        {
          currentLineIsBlank = true;
          currentLine = "";
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false;
          currentLine += c;
        }
        if (currentLine.startsWith("POST /"))
        {
          isPost = true;
        }
      }
    }
    delay(1);
    client.stop();
    Serial.println("client disconnected");
  }
}

void ButtonKeepCount_Task(void *arg)
{
  Serial.println("ButtonKeepCount_Task Start  ");
  int PushKeepSubSecCounter = 0;

  while (1)
  {
    delay(97);
    PoECAM.update();

    if (PoECAM.BtnA.isPressed())
    {
      Serial.println("BtnA Pressed");
      PushKeepSubSecCounter++;
    }
    else if (PoECAM.BtnA.wasReleased())
    {
      Serial.println("BtnA Released");
      PushKeepSubSecCounter = 0;
    }

    if (PushKeepSubSecCounter > 60)
      break;
  }

  bool LED_ON = true;
  while (1)
  {
    delay(97);

    PoECAM.update();
    if (PoECAM.BtnA.wasReleased())
      break;

    LED_ON = !LED_ON;
    PoECAM.setLed(LED_ON);
  }

  InitEEPROM();
  PoECAM.setLed(true);
  delay(1000);
  ESP.restart();
}

