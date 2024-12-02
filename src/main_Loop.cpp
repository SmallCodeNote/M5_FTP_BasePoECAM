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
  unsigned long count = 60;
  while (true)
  {
    delay(10000);
    if (count >= 60)
    {
      NtpClient.updateTimeFromServer(ntpSrvIP_String, +9);
      count = 0;
    }
    else
    {
      count++;
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

void SensorShotLoop(void *arg)
{
  unsigned long lastWriteEpoc = 0;
  unsigned long lastCheckEpoc = 0;
  SensorShotTaskParams taskParam;

  M5_LOGI("SensorShotLoop Start  ");

  while (true)
  {
    if ((Ethernet.linkStatus() == LinkON) && storeData.ftpImageSaveInterval >= 1)
    {
      if (!ftp.isConnected())
        ftp.OpenConnection();

      unsigned long currentEpoch = NtpClient.currentEpoch;

      if (currentEpoch < lastWriteEpoc)
        lastWriteEpoc = 0;

      if (currentEpoch - lastCheckEpoc > 1)
        M5_LOGW("EpocDeltaOver 1:");

      lastCheckEpoc = currentEpoch;
      unsigned long shotStartOffset = SensorShotStartOffset();

      if (currentEpoch >= lastWriteEpoc + storeData.ftpImageSaveInterval)
      {
        if ((shotStartOffset == 0 || SensorShotTaskRunTrigger(currentEpoch)))
        {
          taskParam.currentEpoch = currentEpoch;
          xTaskCreatePinnedToCore(SensorShotTask, "SensorShotTask", 4096, &taskParam, 1, NULL, 1);
          lastWriteEpoc = currentEpoch;
        }
      }
    }
    delay(100);
  }

  if (ftp.isConnected())
    ftp.CloseConnection();

  vTaskDelete(NULL);
}

String LastWriteDirectoryPath = "";
void SensorShotTask(void *param)
{
  String directoryPath = "/" + deviceName;

  SensorShotTaskParams *taskParam = (SensorShotTaskParams *)param;
  unsigned long currentEpoch = taskParam->currentEpoch;

  String ss = NtpClient.readSecond(currentEpoch);
  String mm = NtpClient.readMinute(currentEpoch);
  String HH = NtpClient.readHour(currentEpoch);
  String DD = NtpClient.readDay(currentEpoch);
  String MM = NtpClient.readMonth(currentEpoch);
  String YYYY = NtpClient.readYear(currentEpoch);

  String filePath = directoryPath + "/" + YYYY + MM + DD;

  if (storeData.ftpImageSaveInterval > 60)
  {
    directoryPath = "/" + deviceName + "/" + YYYY;
    filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm;
  }
  else
  {
    directoryPath = "/" + deviceName + "/" + YYYY + "/" + YYYY + MM;
    filePath = directoryPath + "/" + YYYY + MM + DD + "_" + HH + mm + ss;
  }

  unit_flash_set_brightness(storeData.flashIntensityMode);
  delay(storeData.flashLength);
  bool CameraGetImage = PoECAM.Camera.get();
  digitalWrite(FLASH_EN_PIN, LOW);

  if (CameraGetImage)
  {
    ftp.MakeDirRecursive(directoryPath);
    ftp.AppendData(filePath + ".jpg", (u_char *)(PoECAM.Camera.fb->buf), (int)(PoECAM.Camera.fb->len));
    PoECAM.Camera.free();
  }

  LastWriteDirectoryPath = directoryPath;
  vTaskDelete(NULL);
}

unsigned long SensorShotStartOffset()
{
  const int intervals[] = {3600, 600, 300, 10, 5};
  for (int i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
  {
    if (storeData.ftpImageSaveInterval % intervals[i] == 0)
    {
      return 1;
    }
  }
  return 0;
}

bool SensorShotTaskRunTrigger(unsigned long currentEpoch)
{
  String ms = NtpClient.readMinute(currentEpoch);
  char mc0 = ms[0];
  char mc1 = ms[1];
  String ss = NtpClient.readSecond(currentEpoch);
  char sc0 = ss[0];
  char sc1 = ss[1];

  if (!(storeData.ftpImageSaveInterval % 3600) && mc0 == '0' && mc1 == '0' && sc0 == '0' && sc1 == '0')
    return true;
  else if (!(storeData.ftpImageSaveInterval % 600) && mc1 == '0' && sc0 == '0' && sc1 == '0')
    return true;
  else if (!(storeData.ftpImageSaveInterval % 300) && (mc1 == '0' || mc1 == '5') && sc0 == '0' && sc1 == '0')
    return true;
  else if (!(storeData.ftpImageSaveInterval % 10) && sc1 == '0')
    return true;
  else if (!(storeData.ftpImageSaveInterval % 5) && (sc1 == '0' || sc1 == '5'))
    return true;

  return false;
}

QueueHandle_t xQueueJpeg_Src;

void ImageStoreLoop(void *arg)
{
  M5_LOGI("");
  xQueueJpeg_Src = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));

  unsigned long lastWriteEpoc = 0;
  unsigned long lastCheckEpoc = 0;
  unsigned long nextBufferingEpoc = 0;

  while (true)
  {
    unsigned long currentEpoch = NtpClient.currentEpoch;

    if (currentEpoch < lastWriteEpoc)
      lastWriteEpoc = 0;
    if (currentEpoch - lastCheckEpoc > 1)
      M5_LOGW("EpocDeltaOver 1:");

    if (currentEpoch >= nextBufferingEpoc)
    {
      if (PoECAM.Camera.get())
      {
        M5_LOGI("");
        uint8_t *frame_buf = PoECAM.Camera.fb->buf;
        int32_t frame_len = PoECAM.Camera.fb->len;
        pixformat_t pixmode = PoECAM.Camera.fb->format;
        u_int32_t fb_width = (u_int32_t)(PoECAM.Camera.fb->width);
        u_int32_t fb_height = (u_int32_t)(PoECAM.Camera.fb->height);

        uint8_t *frame_Jpeg = (uint8_t *)ps_malloc(frame_len);
        memcpy(frame_Jpeg, frame_buf, frame_len);
        PoECAM.Camera.free();

        JpegItem jpegItem = {currentEpoch, frame_Jpeg, frame_len, pixmode, fb_width, fb_height};

        if (uxQueueSpacesAvailable(xQueueJpeg_Src) < 1)
        {
          JpegItem tempJpegItem;
          xQueueReceive(xQueueJpeg_Src, &tempJpegItem, 0);
          free(tempJpegItem.buf);
        }
        xQueueSend(xQueueJpeg_Src, &jpegItem, 0);
        M5_LOGI("");
      }
      lastCheckEpoc = currentEpoch;
      nextBufferingEpoc = currentEpoch + storeData.imageBufferingInterval;
    }
    delay(10);
  }
  vTaskDelete(NULL);
}

QueueHandle_t xQueueJpeg_Store;
QueueHandle_t xQueueEdge_Store;
QueueHandle_t xQueueProf_Store;

void ImageProcessingLoop(void *arg)
{
  M5_LOGI("");
  xQueueJpeg_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));
  xQueueProf_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(ProfItem));
  xQueueEdge_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(EdgeItem));

  while (true)
  {
    JpegItem jpegItem;
    while (xQueueReceive(xQueueJpeg_Src, &jpegItem, 0))
    {
      uint8_t *bitmap_buf = (uint8_t *)ps_malloc(3 * jpegItem.width * jpegItem.height);
      fmt2rgb888(jpegItem.buf, jpegItem.len, jpegItem.pixformat, bitmap_buf);

      u_int16_t edgeX = ImageProcessingLoop_EdgePosition(bitmap_buf, jpegItem);
      EdgeItem edgeItem = {jpegItem.epoc, edgeX};
      if (uxQueueSpacesAvailable(xQueueEdge_Store) < 1)
      {
        EdgeItem tempItem;
        xQueueReceive(xQueueEdge_Store, &tempItem, 0);
      }
      xQueueSend(xQueueEdge_Store, &edgeItem, 0);

      //====================
      uint32_t pixLineStep = (u_int32_t)(storeData.pixLineStep);
      pixLineStep = pixLineStep > jpegItem.width ? jpegItem.width : pixLineStep;
      uint32_t pixLineRange = (u_int32_t)(storeData.pixLineRange);
      pixLineRange = pixLineRange > 100u ? 100u : pixLineRange;

      uint32_t RangePix = (jpegItem.width * pixLineRange) / 100;
      uint32_t xStartPix = ((jpegItem.width * (100u - pixLineRange)) / 200u);
      uint32_t xEndPix = xStartPix + ((jpegItem.width * pixLineRange) / 100u);

      u_int32_t startOffset = (jpegItem.width * jpegItem.height / 2u + xStartPix) * 3u;
      uint8_t *bitmap_pix = bitmap_buf + startOffset;
      uint8_t *bitmap_pix_lineEnd = bitmap_buf + startOffset + (xEndPix - xStartPix) * 3u;

      bitmap_pix_lineEnd = bitmap_pix_lineEnd == bitmap_pix ? bitmap_pix_lineEnd + 3u : bitmap_pix_lineEnd;

      uint16_t *prof_buf = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

      u_int16_t br = 0;
      int index = 0;
      int count = 0;
      while (bitmap_pix_lineEnd > bitmap_pix)
      {
        br = 0;
        br += *(bitmap_pix);
        bitmap_pix++;
        br += *(bitmap_pix);
        bitmap_pix++;
        br += *(bitmap_pix);
        bitmap_pix++;

        if (index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
        {
          prof_buf[index] = br;
        }
        bitmap_pix += pixLineStep * 3;
        index++;
      }
      count = index;
      while (index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
      {
        prof_buf[index] = 0u;
      }
      ProfItem profileItem = {jpegItem.epoc, count, prof_buf};

      if (uxQueueSpacesAvailable(xQueueProf_Store) < 1)
      {
        ProfItem tempItem;
        xQueueReceive(xQueueProf_Store, &tempItem, 0);
        free(tempItem.buf);
      }
      xQueueSend(xQueueProf_Store, &jpegItem, 0);

      free(bitmap_buf);

      //====================

      if (uxQueueSpacesAvailable(xQueueJpeg_Store) < 1)
      {
        JpegItem tempItem;
        xQueueReceive(xQueueJpeg_Store, &tempItem, 0);
        free(tempItem.buf);
      }

      xQueueSend(xQueueJpeg_Store, &jpegItem, 0);
    }
    delay(100);
  }
  vTaskDelete(NULL);
}

uint16_t ImageProcessingLoop_EdgePosition(uint8_t *bitmap_buf, JpegItem taskArgs)
{
  int32_t fb_width = (int32_t)((taskArgs.width));
  int32_t fb_height = (int32_t)((taskArgs.height));
  int32_t xStartRate = (int32_t)(storeData.pixLineEdgeSearchStart);
  int32_t xEndRate = (int32_t)(storeData.pixLineEdgeSearchEnd);
  M5_LOGI("xStartRate=%d , xEndRate=%d ", xStartRate, xEndRate);

  int32_t xStartPix = (int32_t)((fb_width * xStartRate) / 100);
  int32_t xEndPix = (int32_t)((fb_width * xEndRate) / 100);
  M5_LOGI("xStartPix=%d , xEndPix=%d ", xStartPix, xEndPix);

  int32_t xStep = xStartPix < xEndPix ? 1 : -1;

  int32_t startOffset = (fb_width * fb_height / 2) * 3;
  uint8_t *bitmap_pix = bitmap_buf + startOffset;
  int16_t br = 0;
  int32_t EdgeMode = storeData.pixLineEdgeUp == 1 ? 1 : -1;
  int32_t th = (int32_t)storeData.pixLineThrethold;

  int32_t x = xStartPix;

  for (; (xEndPix - x) * xStep > 0; x += xStep)
  {
    bitmap_pix = bitmap_buf + startOffset + x * 3;
    br = 0;
    br += *(bitmap_pix);
    br += *(bitmap_pix + 1);
    br += *(bitmap_pix + 2);
    M5_LOGI("%d : %d / %d", x, br, th);
    if ((th - br) * EdgeMode < 0)
    {
      return (uint16_t)x;
    }
  }

  return (uint16_t)x;
}

void DataSaveLoop(void *arg)
{
  while (true)
  {
    if ((Ethernet.linkStatus() != LinkON))
    {
      delay(10000);
      continue;
    }

    if (!ftp.isConnected())
      ftp.OpenConnection();

    JpegItem jpegItem;
    while (xQueueReceive(xQueueJpeg_Store, &jpegItem, 0))
    {
      String directoryPath = createDirectorynameFromEpoc(jpegItem.epoc, storeData.ftpImageSaveInterval, false);
      ftp.MakeDirRecursive(directoryPath);
      String filePath = directoryPath + "/" + createFilenameFromEpoc(jpegItem.epoc, storeData.ftpImageSaveInterval, false);
      ftp.AppendData(filePath + ".jpg", (u_char *)(jpegItem.buf), (int)(jpegItem.len));
      free(jpegItem.buf);
    }

    EdgeItem edgeItem;
    while (xQueueReceive(xQueueEdge_Store, &edgeItem, 0))
    {
      String directoryPath = createDirectorynameFromEpoc(edgeItem.epoc, storeData.ftpImageSaveInterval, false);
      ftp.MakeDirRecursive(directoryPath);
      String filePath = directoryPath + "/edge_" + createFilenameFromEpoc(edgeItem.epoc, storeData.ftpImageSaveInterval, false);
      String testLine = NtpClient.convertTimeEpochToString(edgeItem.epoc)+ "," + String(edgeItem.edgeX);
      ftp.AppendTextLine(filePath + ".csv", testLine);
    }

    ProfItem profItem;
    while (xQueueReceive(xQueueProf_Store, &profItem, 0))
    {
      String directoryPath = createDirectorynameFromEpoc(profItem.epoc, storeData.ftpImageSaveInterval, false);
      ftp.MakeDirRecursive(directoryPath);
      String filePath = directoryPath + "/prof_" + createFilenameFromEpoc(profItem.epoc, storeData.ftpImageSaveInterval, false);
      String headLine = NtpClient.convertTimeEpochToString(profItem.epoc);
      ftp.AppendDataArrayAsTextLine(filePath + ".csv", headLine,profItem.buf,profItem.len);
      free(profItem.buf);
    }

    delay(100);
  }

  if (ftp.isConnected())
    ftp.CloseConnection();

  vTaskDelete(NULL);
}

String createFilenameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine)
{
  String ss = NtpClient.readSecond(currentEpoch);
  String mm = NtpClient.readMinute(currentEpoch);
  String HH = NtpClient.readHour(currentEpoch);
  String DD = NtpClient.readDay(currentEpoch);
  String MM = NtpClient.readMonth(currentEpoch);
  String YYYY = NtpClient.readYear(currentEpoch);

  if (multiLine)
  {
    if (interval >= 3600) // ~ 744line / file
      return YYYY + MM;
    if (interval >= 120) // 24 ~ 720line / file
      return YYYY + MM + DD;
    if (interval >= 60) // 360~ 720line / file
      return YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 12) + "00";
    if (interval >= 30) // 360~ 720line / file
      return YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 6) + "00";
    if (interval >= 10) // 240~ 720line / file
      return YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 2) + "00";
    if (interval >= 5) // 360~ 720line / file
      return YYYY + MM + DD + "_" + HH + "00";
    // 120~ 600line / file
    return YYYY + MM + DD + "_" + HH + String(mm[1]) + "0";
  }
  else
  {
    if (interval >= 60)
      return YYYY + MM + DD + "_" + HH + mm;
  }
  return YYYY + MM + DD + "_" + HH + mm + ss;
}

String createDirectorynameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine)
{
  String ss = NtpClient.readSecond(currentEpoch);
  String mm = NtpClient.readMinute(currentEpoch);
  String HH = NtpClient.readHour(currentEpoch);
  String DD = NtpClient.readDay(currentEpoch);
  String MM = NtpClient.readMonth(currentEpoch);
  String YYYY = NtpClient.readYear(currentEpoch);

  if (multiLine && interval >= 30)
    return YYYY + MM;

  return YYYY + MM + DD;
}