#include <M5Unified.h>
#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"

#include "main.h"
#include "main_EEPROM_handler.h"
#include "main_HTTP_UI.h"
#include "main_Loop.h"

void TimeUpdateLoop(void *arg)
{
  M5_LOGI("TimeUpdateLoop Start  ");
  unsigned long lastepoc = 0;

  while (true)
  {
    delay(200);
    String timeLine = NtpClient.getTime(ntpSrvIP_String, +9);
    if (lastepoc > NtpClient.currentEpoch)
      lastepoc = 0;

    if (lastepoc + 10 <= NtpClient.currentEpoch)
    {
      M5.Log.println(("timeLine= " + timeLine).c_str());
      lastepoc = NtpClient.currentEpoch;
    }
  }

  vTaskDelete(NULL);
}

void TimeServerAccessLoop(void *arg)
{
  M5_LOGI("TimeServerAccessLoop Start  ");
  unsigned long count = 0;
  while (true)
  {
    delay(10000);
    if (count > 60)
    {
      NtpClient.updateTimeFromServer(ntpSrvIP_String, +9);
      count = 0;
    }
  }

  vTaskDelete(NULL);
}

void ButtonKeepCountLoop(void *arg)
{
  M5_LOGI("ButtonKeepCountLoop Start  ");
  int PushKeepSubSecCounter = 0;

  while (true)
  {
    delay(100);
    M5.update();

    if (M5.BtnA.isPressed())
    {
      M5_LOGI("BtnA Pressed");
      PushKeepSubSecCounter++;
    }
    else if (M5.BtnA.wasReleased())
    {
      M5_LOGI("BtnA Released");
      PushKeepSubSecCounter = 0;
    }

    if (PushKeepSubSecCounter > 60)
      break;
  }

  bool LED_ON = true;
  while (true)
  {
    delay(1000);

    M5.update();
    if (M5.BtnA.wasReleased())
      break;

    LED_ON = !LED_ON;
  }

  InitEEPROM();
  delay(1000);
  ESP.restart();

  vTaskDelete(NULL);
}

typedef struct
{
  unsigned long currentEpoch;
  uint16_t tofDeviceValue;
} ShotTaskParams;

unsigned long ShotStartOffset()
{
  const int intervals[] = {3600, 600, 300, 10, 5};
  for (int i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
  {
    if (storeData.ftpSaveInterval % intervals[i] == 0)
    {
      return 1;
    }
  }
  return 0;
}

bool ShotTaskRunTrigger(unsigned long currentEpoch)
{
  String ms = NtpClient.readMinute(currentEpoch);
  char mc0 = ms[0];
  char mc1 = ms[1];
  String ss = NtpClient.readSecond(currentEpoch);
  char sc0 = ss[0];
  char sc1 = ss[1];

  if (!(storeData.ftpSaveInterval % 3600) && mc0 == '0' && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.ftpSaveInterval % 600) && (mc1 == '0') && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.ftpSaveInterval % 300) && (mc1 == '0' || mc1 == '5') && (sc0 == '0' && sc1 == '0'))
    return true;
  else if (!(storeData.ftpSaveInterval % 10) && sc1 == '0')
    return true;
  else if (!(storeData.ftpSaveInterval % 5) && (sc1 == '0' || sc1 == '5'))
    return true;

  return false;
}

void ShotLoop(void *arg)
{
  unsigned long lastWriteEpoc = 0;
  unsigned long lastCheckEpoc = 0;
  ShotTaskParams taskParam;
  uint16_t tofDeviceValue = 0;
  char tofDeviceValueChars[5];

  String directoryName_Past = "";

  M5_LOGI("ShotLoop Start  ");

  if (Ethernet.linkStatus() == LinkON)
    ftp.OpenConnection();

  while (true)
  {
    // tofDeviceValue = tofDevice.read();
    // sprintf(tofDeviceValueChars, "%04d", tofDeviceValue);
    // SensorValueString = String(tofDeviceValueChars);

    if ((Ethernet.linkStatus() == LinkON) && storeData.ftpSaveInterval >= 1)
    {
      unsigned long currentEpoch = NtpClient.currentEpoch;
      if (currentEpoch < lastWriteEpoc)
        lastWriteEpoc = 0;

      if (currentEpoch - lastCheckEpoc > 1)
        M5_LOGW("EpocDeltaOver 1:");

      lastCheckEpoc = currentEpoch;
      unsigned long shotStartOffset = ShotStartOffset();

      // if (currentEpoch + checkStartOffset >= lastWriteEpoc + storeData.ftpSaveInterval)
      if (currentEpoch >= lastWriteEpoc + storeData.ftpSaveInterval)
      {
        if (!ftp.isConnected())
          ftp.OpenConnection();
        if ((shotStartOffset == 0 || ShotTaskRunTrigger(currentEpoch)))
        {
          taskParam.currentEpoch = currentEpoch;
          taskParam.tofDeviceValue = tofDeviceValue;
          xTaskCreatePinnedToCore(ShotTask, "ShotTask", 4096, &taskParam, 1, NULL, 1);

          lastWriteEpoc = currentEpoch;
          delay(1000);
          continue;
        }
      }
    }
    delay(100);
  }

  if (ftp.isConnected())
    ftp.CloseConnection();

  vTaskDelete(NULL);
}

void unit_flash_set_brightness(uint8_t brightness)
{
  if ((brightness >= 1) && (brightness <= 16))
  {
    for (int i = 0; i < brightness; i++)
    {
      digitalWrite(FLASH_EN_PIN, LOW);
      delayMicroseconds(4);
      digitalWrite(FLASH_EN_PIN, HIGH);
      delayMicroseconds(4);
    }
  }
  else
  {
    digitalWrite(FLASH_EN_PIN, LOW);
  }

  M5_LOGI("unit_flash_set_brightness = %u",brightness);
}

void unit_flash_set_delay(uint8_t brightness)
{
  if ((brightness >= 1) && (brightness <= 8))
  {
    delay(190);
  }
  else
  {
    delay(1270);
  }
  digitalWrite(FLASH_EN_PIN, LOW);
}

String LastWriteDirectoryPath = "";
void ShotTask(void *param)
{
  String directoryPath = "/" + deviceName;

  ShotTaskParams *taskParam = (ShotTaskParams *)param;
  unsigned long currentEpoch = taskParam->currentEpoch;
  // uint16_t tofDeviceValue = taskParam->tofDeviceValue;

  String ss = NtpClient.readSecond(currentEpoch);
  String mm = NtpClient.readMinute(currentEpoch);
  String HH = NtpClient.readHour(currentEpoch);
  String DD = NtpClient.readDay(currentEpoch);
  String MM = NtpClient.readMonth(currentEpoch);
  String YYYY = NtpClient.readYear(currentEpoch);

  String filePath = directoryPath + "/" + YYYY + MM + DD;

  if (storeData.ftpSaveInterval > 60)
  {
    directoryPath = "/" + deviceName + "/" + YYYY;
    // filePath = directoryPath + "/" + YYYY + MM ;
  }
  else
  {
    directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM;
    // filePath = directoryPath + "/" + YYYY + MM + DD ;
  }
  /*
    String TimeLine = YYYY + "/" + MM + "/" + DD + " " + HH + ":" + mm + ":" + ss;

    if (true)
    {
      // ftp.OpenConnection();
      if (LastWriteDirectoryPath != directoryPath)
        ftp.MakeDirRecursive(directoryPath);
      ftp.AppendTextLine(filePath + ".csv", TimeLine + "," + String(tofDeviceValue));
      // ftp.CloseConnection();
    }
  */

  filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm + ss;

  unit_flash_set_brightness(storeData.flashIntensityMode);
  delay(storeData.flashLength);
  bool CameraGetImage = PoECAM.Camera.get();
  digitalWrite(FLASH_EN_PIN, LOW);

  if (CameraGetImage)
  {
    // ftp.OpenConnection();
    // if (LastWriteDirectoryPath != directoryPath)
    ftp.MakeDirRecursive(directoryPath);
    ftp.AppendData(filePath + ".jpg", (u_char *)(PoECAM.Camera.fb->buf), (int)(PoECAM.Camera.fb->len));

    // ftp.CloseConnection();
    PoECAM.Camera.free();
  }

  LastWriteDirectoryPath = directoryPath;
  vTaskDelete(NULL);
}

