#include "M5PoECAM.h"
#include <SPI.h>
#include <M5_Ethernet.h>
#include <time.h>
#include <EEPROM.h>
#include "M5_Ethernet_FtpClient.hpp"
#include "M5_Ethernet_NtpClient.hpp"

#include "main_EEPROM_handler.h"
#include "main_HTTP_UI.h"

// == M5PoECAM_Bus ==
#define SCK 23
#define MISO 38
#define MOSI 13
#define CS 4

void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);
void ButtonKeepCountLoop(void *arg);
void ShotLoop(void *arg);
void ShotTask(void *arg);

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

EthernetClient FtpClient(21);
M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

bool UnitEnable = true;

bool EthernetBegin()
{
  Serial.println("MAX_SOCK_NUM = " + String(MAX_SOCK_NUM));
  if (MAX_SOCK_NUM < 8)
  {
    Serial.println("need overwrite MAX_SOCK_NUM = 8 in M5_Ethernet.h");
    UnitEnable = false;
    return false;
  }

  SPI.begin(SCK, MISO, MOSI, -1);
  Ethernet.init(CS);
  Ethernet.begin(mac, deviceIP);

  return UnitEnable;
}

bool CameraBegin()
{
  if (!PoECAM.Camera.begin())
  {
    Serial.println("Camera Init Fail");
    UnitEnable = false;
    return false;
  }

  PoECAM.Camera.sensor->set_pixformat(PoECAM.Camera.sensor, PIXFORMAT_JPEG);
  PoECAM.Camera.sensor->set_framesize(PoECAM.Camera.sensor, FRAMESIZE_QVGA);
  PoECAM.Camera.sensor->set_vflip(PoECAM.Camera.sensor, 1);
  PoECAM.Camera.sensor->set_hmirror(PoECAM.Camera.sensor, 0);
  PoECAM.Camera.sensor->set_gain_ctrl(PoECAM.Camera.sensor, 0);

  return UnitEnable;
}

void setup()
{
  PoECAM.begin();
  if (!CameraBegin())
    return;

  EEPROM.begin(STORE_DATA_SIZE);
  // LoadEEPROM();
  InitEEPROM();
  M5_Ethernet_FtpClient ftp(ftp_address, ftp_user, ftp_pass, 60000);

  if (!EthernetBegin())
    return;

  HttpServer.begin();

  NtpClient.begin();
  NtpClient.getTime(ntp_address, +9);

  xTaskCreatePinnedToCore(TimeUpdateLoop, "TimeUpdateLoop", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TimeServerAccessLoop, "TimeServerAccessLoop", 4096, NULL, 6, NULL, 0);
  xTaskCreatePinnedToCore(ButtonKeepCountLoop, "ButtonKeepCountLoop", 4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(ShotLoop, "ShotLoop", 4096, NULL, 3, NULL, 1);
}

void loop()
{
  delay(189);
  HTTP_UI();
  Ethernet.maintain();
}

void TimeUpdateLoop(void *arg)
{
  Serial.println("TimeUpdateLoop Start  ");
  unsigned long lastepoc = 0;

  while (1)
  {
    delay(200);
    String timeLine = NtpClient.getTime(ntp_address, +9);
    if (lastepoc > NtpClient.currentEpoch)
      lastepoc = 0;

    if (lastepoc + 10 <= NtpClient.currentEpoch)
    {
      Serial.println(timeLine);
      lastepoc = NtpClient.currentEpoch;
    }
  }

  vTaskDelete(NULL);
}

void TimeServerAccessLoop(void *arg)
{
  Serial.println("TimeServerAccessLoop Start  ");
  unsigned long count = 0;
  while (1)
  {
    delay(10000);
    if (count > 6)
    {
      NtpClient.updateTimeFromServer(ntp_address, +9);
      count = 0;
    }
  }

  vTaskDelete(NULL);
}
void ButtonKeepCountLoop(void *arg)
{
  Serial.println("ButtonKeepCountLoop Start  ");
  int PushKeepSubSecCounter = 0;

  while (1)
  {
    delay(1000);
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

    if (PushKeepSubSecCounter > 6)
      break;
  }

  bool LED_ON = true;
  while (1)
  {
    delay(1000);

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

  vTaskDelete(NULL);
}

typedef struct
{
  unsigned long currentEpoch;
} ShotTaskParams;

unsigned long CheckStartOffset()
{

  const int intervals[] = {3600, 600, 300, 10, 5};
  for (int i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
  {
    if (storeData.interval % intervals[i] == 0)
    {
      return 1;
    }
  }
  return 0;
}

bool ShotTaskRun(unsigned long currentEpoch)
{
  String ms = NtpClient.readMinute(currentEpoch);
  char mc0 = ms[0];
  char mc1 = ms[1];
  String ss = NtpClient.readSecond(currentEpoch);
  char sc0 = ss[0];
  char sc1 = ss[1];

  if (!(storeData.interval % 3600) && mc0 == '0' && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.interval % 600) && (mc1 == '0') && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.interval % 300) && (mc1 == '0' || mc1 == '5') && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.interval % 10) && sc1 == '0')
    return true;
  else if (!(storeData.interval % 5) && (sc1 == '0' || sc1 == '5'))
    return true;

  return false;
}

void ShotLoop(void *arg)
{
  unsigned long lastWriteEpoc = 0;
  unsigned long lastCheckEpoc = 0;

  Serial.println("ShotLoop Start  ");
  if (storeData.interval < 1)
    storeData.interval = 1;

  while (1)
  {
    unsigned long currentEpoch = NtpClient.currentEpoch;
    if (currentEpoch - lastCheckEpoc > 1)
    {
      Serial.println(">>EpocDeltaOver 1:");
    }
    lastCheckEpoc = currentEpoch;
    unsigned long checkStartOffset = CheckStartOffset();
    bool Flag0 = lastWriteEpoc + storeData.interval <= currentEpoch + checkStartOffset;

    if (Flag0)
    {
      if (checkStartOffset == 0 || ShotTaskRun(currentEpoch))
      {
        xTaskCreatePinnedToCore(ShotTask, "ShotTask", 4096, &currentEpoch, 4, NULL, 1);

        if (currentEpoch < lastWriteEpoc)
        {
          lastWriteEpoc = 0;
        }
        else
        {
          lastWriteEpoc = currentEpoch;
        }
        delay(1000);
      }
      else
      {
        delay(100);
      }
    }
    else
    {
      delay(100);
    }
  }

  vTaskDelete(NULL);
}

void ShotTask(void *param)
{
  String directoryPath = "/" + deviceName;

  ShotTaskParams *taskParam = (ShotTaskParams *)param;
  unsigned long currentEpoch = taskParam->currentEpoch;

  String ss = NtpClient.readSecond(currentEpoch);
  String mm = NtpClient.readMinute(currentEpoch);
  String HH = NtpClient.readHour(currentEpoch);
  String DD = NtpClient.readDay(currentEpoch);
  String MM = NtpClient.readMonth(currentEpoch);
  String YYYY = NtpClient.readYear(currentEpoch);

  if (storeData.interval < 60)
  {
    directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD + "/" + HH;
  }
  else
  {
    directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD;
  }

  String filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm + ss;

  if (PoECAM.Camera.get())
  {
    ftp.OpenConnection();
    ftp.MakeDirRecursive(directoryPath);
    ftp.AppendData(filePath + ".jpg", (u_char *)(PoECAM.Camera.fb->buf), (int)(PoECAM.Camera.fb->len));
    ftp.CloseConnection();
    PoECAM.Camera.free();
  }

  vTaskDelete(NULL);
}
