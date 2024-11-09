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
EthernetServer server(80);

EthernetClient FtpClient(21);

String ftp_address = "192.168.25.77";
String ftp_user = "ftpusr";
String ftp_pass = "ftpword";

M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

String ntp_address = "192.168.25.77";

char textArray[] = "textArray";

void HTTP_UI();
void ShotTask(void *arg);

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
void LoadEEPROM()
{
  EEPROM.begin(STORE_DATA_SIZE);
  EEPROM.get<DATA_SET>(0, storeData);
}

void EthernetBegin()
{
  SPI.begin(SCK, MISO, MOSI, -1);
  Ethernet.init(CS);
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, deviceIP);
}

String deviceName = "Device";
String deviceIP_String = "";
String ftpSrvIP_String = "";
String ntpSrvIP_String = "";

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

  // Open serial communications and wait for port to open:
  // M5Begin();
  Serial.println("MAX_SOCK_NUM = " + String(MAX_SOCK_NUM));
  if (MAX_SOCK_NUM < 8)
    Serial.println("need overwrite MAX_SOCK_NUM = 8 in M5_Ethernet.h");

  LoadEEPROM();
  if (storeData.deviceName.length() > 1)
  {
    deviceName = storeData.deviceName;
    if (storeData.deviceIP.toString() != "255.255.255.255")
    {
      deviceIP = storeData.deviceIP;
      deviceIP_String = storeData.deviceIP.toString();
      ftpSrvIP_String = storeData.ftpSrvIP.toString();
      ntpSrvIP_String = storeData.ntpSrvIP.toString();
      ftp_user = storeData.ftp_user;
      ftp_pass = storeData.ftp_pass;
      shotInterval = String(storeData.interval / 1000);
    }
  }

  EthernetBegin();
  server.begin();

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  NtpClient.begin();

  xTaskCreatePinnedToCore(ShotTask, "ShotTask", 4096, NULL, 2, NULL, 1);
}

void loop()
{
  HTTP_UI();
  Ethernet.maintain();
  delay(1000);
}

void ShotTask(void *arg)
{
  if (storeData.interval < 1000)
    storeData.interval = 1000;

  unsigned long lastMillis = millis() - storeData.interval;

  while (1)
  {
    if (millis() - lastMillis >= storeData.interval)
    {
      if (PoECAM.Camera.get())
      {
        Serial.printf("pic size: %d\n", PoECAM.Camera.fb->len);
        PoECAM.Camera.free();
      }

      String timeLine = NtpClient.getTime(ntp_address, +9);
      Serial.println(timeLine);

      String YYYY = NtpClient.readYear();
      String MM = NtpClient.readMonth();
      String DD = NtpClient.readDay();
      String HH = NtpClient.readHour();
      String mm = NtpClient.readMinute();
      String ss = NtpClient.readSecond();

      String directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD;
      String filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm + ss;

      ftp.OpenConnection();
      ftp.MakeDirRecursive(directoryPath);
      // ftp.AppendTextLine(filePath + ".txt", timeLine);
      PoECAM.Camera.get();
      ftp.AppendData(filePath + ".jpg", (u_char *)(PoECAM.Camera.fb->buf), (int)(PoECAM.Camera.fb->len));
      ftp.CloseConnection();
      PoECAM.Camera.free();

      unsigned long nowMillis = millis();
      if (nowMillis < lastMillis)
      {
        lastMillis = 0;
      }
      else
      {
        lastMillis = nowMillis;
      }
    }

    vTaskDelay(100 / portTICK_RATE_MS);
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
  EthernetClient client = server.available();
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

            M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

            storeData.deviceName = deviceName;
            storeData.deviceIP.fromString(deviceIP_String);
            storeData.ntpSrvIP.fromString(ntpSrvIP_String);
            storeData.ftpSrvIP.fromString(ftpSrvIP_String);
            storeData.ftp_user = ftp_user;
            storeData.ftp_pass = ftp_pass;
            storeData.interval = (u_int16_t)(shotInterval.toInt()) * 1000;

            EEPROM.put<DATA_SET>(0, storeData);
            EEPROM.commit();
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

          /*
          HTML_PUT_INFOWITHLABEL(deviceName);
          HTML_PUT_INFOWITHLABEL(deviceIP_String);
          HTML_PUT_INFOWITHLABEL(ftpSrvIP_String);
          HTML_PUT_INFOWITHLABEL(ntpSrvIP_String);
          */

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
