#include <M5Unified.h>
#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"

#include "main.h"
#include "main_EEPROM_handler.h"
#include "main_HTTP_UI.h"
#include "main_Loop.h"

#include <cmath>

void TimeUpdateLoop(void *arg)
{
  M5_LOGI("TimeUpdateLoop Start  ");

  while (true)
  {
    delay(100);
    NtpClient.updateCurrentEpoch();
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
    if (NtpClient.currentEpoch == 0 && count >= 3 || count >= 60)
    {
      M5_LOGV("");
      if (xSemaphoreTakeRetry(mutex_Ethernet, 1))
      {
        M5_LOGV("");
        NtpClient.updateTimeFromServer(ntpSrvIP_String, +9);
        M5_LOGV("");

        xSemaphoreGive(mutex_Ethernet);
      }
      else
      {
        M5_LOGW("mutex can not take.");
      }
      M5_LOGV("");
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

  bool pushBeforeCheckSpan = false;

  while (true)
  {
    delay(100);
    PoECAM.update();

    if (PoECAM.BtnA.pressedFor(6000))
    {
      M5_LOGI("Unit Initialize");
      InitEEPROM();
      break;
    }

    if (pushBeforeCheckSpan)
      M5_LOGI("pushBeforeCheckSpan = true");
    if (!pushBeforeCheckSpan && PoECAM.BtnA.pressedFor(200))
    {
      M5_LOGI("");
      pushBeforeCheckSpan = true;
    }
    if (pushBeforeCheckSpan && PoECAM.BtnA.releasedFor(1000))
    {
      M5_LOGI("");
      pushBeforeCheckSpan = false;
    }
    if (pushBeforeCheckSpan && PoECAM.BtnA.pressedFor(1000))
    {
      M5_LOGI("");
      break;
    }
  }

  M5_LOGW("ESP.restart()");
  delay(1000);
  ESP.restart();

  vTaskDelete(NULL);
}

QueueHandle_t xQueueJpeg_Src;

void ImageStoreLoop(void *arg)
{
  M5_LOGI("");
  xQueueJpeg_Src = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));

  unsigned long lastWriteEpoc = 0;
  unsigned long lastCheckEpoc = 0;
  unsigned long nextBufferingEpoc = 0;

  unsigned long lastShotMillis = 0;
  unsigned long nowShotMillis = 0;

  u_int8_t flashMode = storeData.flashIntensityMode;
  uint16_t flashLength = storeData.flashLength;

  while (true)
  {
    unsigned long currentEpoch = NtpClient.currentEpoch;

    if (flashMode != storeData.flashIntensityMode)
    {
      unit_flash_set_brightness(storeData.flashIntensityMode);
      flashMode = storeData.flashIntensityMode;
    }
    if (flashLength != storeData.flashLength)
    {
      flashLength = storeData.flashLength;
    }

    if (currentEpoch == 0)
    {
      currentEpoch = millis() / 1000;
    }

    if (currentEpoch < lastWriteEpoc)
      lastWriteEpoc = 0;
    if (currentEpoch - lastCheckEpoc > 1)
      M5_LOGW("EpocDeltaOver 1:");

    if (currentEpoch >= nextBufferingEpoc)
    {
      // M5_LOGI("");
      if (flashMode > 0)
      {
        digitalWrite(FLASH_EN_PIN, HIGH);
        // delay(30);
        // M5_LOGD("currentEpoch = %u, nextBufferingEpoc = %u", currentEpoch, nextBufferingEpoc);
        delay(flashLength);
      }
      nowShotMillis = millis();
      if (PoECAM.Camera.get())
      {
        digitalWrite(FLASH_EN_PIN, LOW);
        // M5_LOGI("");
        uint8_t *frame_buf = PoECAM.Camera.fb->buf;
        size_t frame_len = PoECAM.Camera.fb->len;
        pixformat_t pixmode = PoECAM.Camera.fb->format;
        u_int32_t fb_width = (u_int32_t)(PoECAM.Camera.fb->width);
        u_int32_t fb_height = (u_int32_t)(PoECAM.Camera.fb->height);
        // M5_LOGI("");
        uint8_t *frame_Jpeg = (uint8_t *)ps_malloc(frame_len);
        memcpy(frame_Jpeg, frame_buf, frame_len);
        PoECAM.Camera.free();
        // M5_LOGI("");
        JpegItem jpegItem = {currentEpoch, frame_Jpeg, frame_len, pixmode, fb_width, fb_height};

        UBaseType_t Jpeg_Src_Aveilable = uxQueueSpacesAvailable(xQueueJpeg_Src);
        // M5_LOGV("Jpeg_Src_Aveilable = %u", Jpeg_Src_Aveilable);
        if (Jpeg_Src_Aveilable < 1)
        {
          JpegItem tempJpegItem;
          xQueueReceive(xQueueJpeg_Src, &tempJpegItem, 0);
          free(tempJpegItem.buf);
        }
        Jpeg_Src_Aveilable = uxQueueSpacesAvailable(xQueueJpeg_Src);
        // M5_LOGV("Jpeg_Src_Aveilable = %u", Jpeg_Src_Aveilable);
        xQueueSend(xQueueJpeg_Src, &jpegItem, 0);
      }

      digitalWrite(FLASH_EN_PIN, LOW);

      lastCheckEpoc = currentEpoch;
      nextBufferingEpoc = currentEpoch + storeData.imageBufferingEpochInterval;

      // M5_LOGI("shot interval = %u", nowShotMillis - lastShotMillis);
      lastShotMillis = nowShotMillis;
    }

    delay(100);
  }
  vTaskDelete(NULL);
}

QueueHandle_t xQueueJpeg_Store;
QueueHandle_t xQueueEdge_Store;
QueueHandle_t xQueueProf_Store;

QueueHandle_t xQueueJpeg_Last;
QueueHandle_t xQueueEdge_Last;
QueueHandle_t xQueueProf_Last;

u_int16_t channelSum(uint8_t *bitmap_pix)
{
  uint8_t rgb[3];
  memcpy(rgb, bitmap_pix, 3);
  uint16_t r = rgb[0];
  uint16_t g = rgb[1];
  uint16_t b = rgb[2];

  return r + g + b;
}

u_int16_t channelSum90(uint8_t *bitmap_pix)
{
  uint8_t *pixStart = bitmap_pix - storeData.pixLineSideWidth * 3;
  uint8_t *pixEnd = bitmap_pix + storeData.pixLineSideWidth * 3;

  uint16_t maxBr = 0;
  uint16_t tempBr = 0;

  for (uint8_t *pix = pixStart; pix <= pixEnd; pix += 3)
  {
    tempBr = channelSum(pix);
    if (maxBr < tempBr)
      maxBr = tempBr;
  }

  return maxBr;
}

void addEdgeItemToQueue(QueueHandle_t queueHandle, EdgeItem *edgeItem)
{
  if (queueHandle != NULL && uxQueueSpacesAvailable(queueHandle) < 1 && uxQueueMessagesWaiting(queueHandle) > 0)
  {
    EdgeItem tempItem;
    xQueueReceive(queueHandle, &tempItem, 0);
  }
  xQueueSend(queueHandle, edgeItem, 0);
}

void addProfItemToQueue(QueueHandle_t queueHandle, ProfItem *profileItem)
{
  if (queueHandle != NULL && uxQueueSpacesAvailable(queueHandle) < 1 && uxQueueMessagesWaiting(queueHandle) > 0)
  {
    ProfItem tempItem;
    xQueueReceive(queueHandle, &tempItem, 0);
    free(tempItem.buf);
  }
  xQueueSend(queueHandle, profileItem, 0);
}

void addJpegItemToQueue(QueueHandle_t queueHandle, JpegItem *jpegItem)
{
  if (queueHandle != NULL && uxQueueSpacesAvailable(queueHandle) < 1 && uxQueueMessagesWaiting(queueHandle) > 0)
  {
    M5_LOGI();
    JpegItem tempItem;
    xQueueReceive(queueHandle, &tempItem, 0);
    free(tempItem.buf);
    M5_LOGI();
  }
  M5_LOGI();
  xQueueSend(queueHandle, jpegItem, 0);
  M5_LOGI();
}

void getProfStartAndEnd(u_int8_t angle, u_int32_t width, u_int32_t height, u_int32_t *startX, u_int32_t *endX, u_int32_t *startY, u_int32_t *endY, size_t *len, int32_t *addressStep)
{
  if (angle == 0)
  {
    *startX = 0;
    *startY = height / 2;
    *endX = width;
    *endY = height / 2;
    *len = width;
    *addressStep = 3;
  }
  else // if (angle == 90)
  {
    *startX = width / 2 + storeData.pixLineShiftUp;
    *startY = 0;
    *endX = width / 2 + storeData.pixLineShiftUp;
    *endY = height;
    *len = height;
    *addressStep = 3 * width;
  }
  /*else if (angle == 45)
  {
    *startX = width > height ? (width - height) / 2 : 0;
    *startY = height > width ? (height - width) / 2 : 0;
    *endX = width > height ? width - (width - height) / 2 : 0;
    *endY = height > width ? height - (height - width) / 2 : 0;
    *len = height < width ? height : width;
    *addressStep = 3 * width + 3;
  }
  else
  {
    float rad = angle * M_PI / 180.0;
    float tanAngle = tan(rad);

    if (angle < 45)
    {
      *startX = 0;
      *startY = height / 2 - (width / 2) * tanAngle;
      *endX = width - 1;
      *endY = height / 2 + (width / 2) * tanAngle;
      *len = width;
    }
    else
    {
      *startX = width / 2 - (height / 2) / tanAngle;
      *startY = 0;
      *endX = width / 2 + (height / 2) / tanAngle;
      *endY = height - 1;
      *len = height;
    }

    *startY = std::max(0, std::min((int)height - 1, (int)*startY));
    *endY = std::max(0, std::min((int)height - 1, (int)*endY));
    *startX = std::max(0, std::min((int)width - 1, (int)*startX));
    *endX = std::max(0, std::min((int)width - 1, (int)*endX));
  }*/
}

void getImageProfile(uint8_t *bitmap_buf, uint16_t *prof, JpegItem jpegItem, size_t *profLen)
{
  uint32_t pixLineStep = (u_int32_t)(storeData.pixLineStep);
  pixLineStep = pixLineStep > jpegItem.width ? jpegItem.width : pixLineStep;

  int32_t addressStep = 3;
  uint32_t startX = 0, startY = 0, endX = 0, endY = 0;

  getProfStartAndEnd(storeData.pixLineAngle, jpegItem.width, jpegItem.height, &startX, &endX, &startY, &endY, profLen, &addressStep);

  uint8_t *bitmap_pix = bitmap_buf + startX * 3u + startY * jpegItem.width * 3u;
  uint8_t *bitmap_pix_lineEnd = bitmap_buf + endX * 3u + endY * jpegItem.width * 3u;

  u_int16_t br = 0;
  int index = 0;
  int count = 0;

  if (storeData.pixLineAngle == 90)
  {
    while (bitmap_pix_lineEnd > bitmap_pix && index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
    {
      br = channelSum90(bitmap_pix);
      prof[index] = br;
      bitmap_pix += addressStep;
      index++;
    }
  }
  else
  {
    while (bitmap_pix_lineEnd > bitmap_pix && index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
    {
      br = channelSum(bitmap_pix);
      prof[index] = br;
      bitmap_pix += addressStep;
      index++;
    }
  }
}

void ImageProcessingLoop(void *arg)
{
  M5_LOGI("");
  xQueueJpeg_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));
  xQueueProf_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(ProfItem));
  xQueueEdge_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(EdgeItem));

  xQueueJpeg_Last = xQueueCreate(1, sizeof(JpegItem));
  xQueueProf_Last = xQueueCreate(1, sizeof(ProfItem));
  xQueueEdge_Last = xQueueCreate(1, sizeof(EdgeItem));

  while (true)
  {
    JpegItem jpegItem;

    while (xQueueJpeg_Src != NULL && xQueueReceive(xQueueJpeg_Src, &jpegItem, 0) == pdPASS)
    {
      // M5_LOGI("srcQueue waiting count : %u", uxQueueMessagesWaiting(xQueueJpeg_Src));
      int bufIndexMax = 3 * jpegItem.width * jpegItem.height;
      uint8_t *bitmap_buf = (uint8_t *)ps_malloc(bufIndexMax);

      fmt2rgb888(jpegItem.buf, jpegItem.len, jpegItem.pixformat, bitmap_buf);

      uint16_t *prof_buf = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);
      uint16_t *prof_buf_Last = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

      size_t profLen = 0;
      getImageProfile(bitmap_buf, prof_buf, jpegItem, &profLen);
      free(bitmap_buf);

      int index = profLen;
      while (index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
      {
        prof_buf[index] = 0u;
        index++;
      }

      memcpy(prof_buf_Last, prof_buf, MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

      ProfItem profileItem = {jpegItem.epoc, profLen, prof_buf};
      ProfItem profileItem_Last = {jpegItem.epoc, profLen, prof_buf_Last};

      u_int16_t edgeX = ImageProcessingLoop_EdgePosition(profileItem);
      u_int16_t edgeX_Last = edgeX;

      EdgeItem edgeItem = {jpegItem.epoc, edgeX};
      EdgeItem edgeItemLast = {jpegItem.epoc, edgeX_Last};
      addEdgeItemToQueue(xQueueEdge_Store, &edgeItem);
      addEdgeItemToQueue(xQueueEdge_Last, &edgeItemLast);

      addProfItemToQueue(xQueueProf_Store, &profileItem);
      addProfItemToQueue(xQueueProf_Last, &profileItem_Last);

      //====================
      uint8_t *frame_Jpeg_Last = (uint8_t *)ps_malloc(jpegItem.len);
      memcpy(frame_Jpeg_Last, jpegItem.buf, jpegItem.len);
      JpegItem jpegItem_Last = {jpegItem.epoc, frame_Jpeg_Last, jpegItem.len};
      M5_LOGD("addJpegItemToQueue");
      addJpegItemToQueue(xQueueJpeg_Store, &jpegItem);
      addJpegItemToQueue(xQueueJpeg_Last, &jpegItem_Last);

      delay(1);
    }

    delay(1);
  }
  vTaskDelete(NULL);
  M5_LOGE("");
}

uint16_t ImageProcessingLoop_EdgePosition(ProfItem profItem)
{
  int32_t startRate = (int32_t)(storeData.pixLineEdgeSearchStart);
  int32_t endRate = (int32_t)(storeData.pixLineEdgeSearchEnd);
  // M5_LOGI("xStartRate=%d , xEndRate=%d ", xStartRate, xEndRate);

  int32_t xStartPix = (int32_t)((profItem.len * startRate) / 100);
  int32_t xEndPix = (int32_t)((profItem.len * endRate) / 100);
  // M5_LOGI("xStartPix=%d , xEndPix=%d ", xStartPix, xEndPix);

  int32_t xStep = xStartPix < xEndPix ? 1 : -1;

  int16_t br = 0;
  int32_t EdgeMode = storeData.pixLineEdgeUp == 1 ? 1 : -1;
  int32_t th = (int32_t)storeData.pixLineThrethold;

  int32_t x = xStartPix;

  for (; (xEndPix - x) * xStep > 0; x += xStep)
  {
    br = profItem.buf[x];
    if ((th - br) * EdgeMode < 0)
    {
      return (uint16_t)x;
    }
  }
  return (uint16_t)x;
}

void HTTPLoop(void *arg)
{
  while (true)
  {
    HTTP_UI();
    delay(10);
  }
  vTaskDelete(NULL);
}

void DataSaveLoop(void *arg)
{
  unsigned long lastCheckEpoc_Jpeg = 0;
  unsigned long lastCheckEpoc_Edge = 0;
  unsigned long lastCheckEpoc_Prof = 0;

  unsigned long nextSaveEpoc_Jpeg = 0;
  unsigned long nextSaveEpoc_Edge = 0;
  unsigned long nextSaveEpoc_Prof = 0;

  unsigned long loopStartMillis = millis();

  while (true)
  {
    loopStartMillis = millis();

    if ((Ethernet.linkStatus() != LinkON))
    {
      M5_LOGW("Ethernet link is not on.");
      delay(10000);
      continue;
    }

    if (!ftp.isConnected())
    {
      M5_LOGW("ftp is not Connected.");

      if (xSemaphoreTakeRetry(mutex_Ethernet, 3))
      {
        M5_LOGD("mutex take");
        ftp.OpenConnection();
        M5_LOGI("ftp.OpenConnection() finished.");
        xSemaphoreGive(mutex_Ethernet);
        M5_LOGI("mutex give");
      }
      else
      {
        M5_LOGW("mutex can not take.");
        delay(1000);
        continue;
      }
    }

    if (ftp.isConnected())
    {
      M5_LOGD("ftp is Connected.");

      JpegItem jpegItem;
      u_int16_t saveInterval = storeData.ftpImageSaveInterval;
      while (xQueueReceive(xQueueJpeg_Store, &jpegItem, 0))
      {
        if (jpegItem.epoc < lastCheckEpoc_Jpeg)
          lastCheckEpoc_Jpeg = 0;
        if (jpegItem.epoc - lastCheckEpoc_Jpeg > 1)
          M5_LOGW("JpegEpocDeltaOver 1:");
        lastCheckEpoc_Jpeg = jpegItem.epoc;

        if (DataSave_Trigger(jpegItem.epoc, saveInterval, nextSaveEpoc_Jpeg))
        {
          M5_LOGI("Jpeg %u", saveInterval);
          nextSaveEpoc_Jpeg = jpegItem.epoc + saveInterval;
          String directoryPath = String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(jpegItem.epoc, storeData.ftpImageSaveInterval, false);
          String filePath = directoryPath + "/" + createFilenameFromEpoc(jpegItem.epoc, storeData.ftpImageSaveInterval, false);

          if (xSemaphoreTakeRetry(mutex_Ethernet, 5))
          {
            M5_LOGD("mutex take");
            ftp.MakeDirRecursive(directoryPath);
            ftp.AppendData(filePath + ".jpg", (u_char *)(jpegItem.buf), (int)(jpegItem.len));
            xSemaphoreGive(mutex_Ethernet);
            M5_LOGI("mutex give");
          }
          else
          {
            M5_LOGW("mutex can not take.");
          }
        }
        free(jpegItem.buf);
        delay(1); // wdt notice
      }

      EdgeItem edgeItem;
      saveInterval = storeData.ftpEdgeSaveInterval;
      while (xQueueReceive(xQueueEdge_Store, &edgeItem, 0))
      {
        lastCheckEpoc_Edge = edgeItem.epoc;
        if (DataSave_Trigger(edgeItem.epoc, saveInterval, nextSaveEpoc_Edge))
        {
          M5_LOGI("edge %u", saveInterval);
          nextSaveEpoc_Edge = edgeItem.epoc + saveInterval;
          String directoryPath = String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(edgeItem.epoc, storeData.ftpImageSaveInterval, true);
          String filePath = directoryPath + "/edge_" + createFilenameFromEpoc(edgeItem.epoc, storeData.ftpImageSaveInterval, true);
          String testLine = NtpClient.convertTimeEpochToString(edgeItem.epoc) + "," + String(edgeItem.edgeX);

          if (xSemaphoreTakeRetry(mutex_Ethernet, 5))
          {
            M5_LOGD("mutex take");

            ftp.MakeDirRecursive(directoryPath);
            ftp.AppendTextLine(filePath + ".csv", testLine);
            xSemaphoreGive(mutex_Ethernet);
            M5_LOGI("mutex give");
          }
          else
          {
            M5_LOGW("mutex can not take.");
          }
        }
      }

      ProfItem profItem;
      saveInterval = storeData.ftpProfileSaveInterval;
      while (xQueueReceive(xQueueProf_Store, &profItem, 0))
      {
        lastCheckEpoc_Prof = profItem.epoc;
        if (DataSave_Trigger(profItem.epoc, saveInterval, nextSaveEpoc_Prof))
        {
          M5_LOGI("prof %u", saveInterval);
          nextSaveEpoc_Prof = profItem.epoc + saveInterval;
          String directoryPath = String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(profItem.epoc, storeData.ftpImageSaveInterval, true);
          String filePath = directoryPath + "/prof_" + createFilenameFromEpoc(profItem.epoc, storeData.ftpImageSaveInterval, true);
          String headLine = NtpClient.convertTimeEpochToString(profItem.epoc);

          if (xSemaphoreTakeRetry(mutex_Ethernet, 5))
          {
            M5_LOGD("mutex take");

            ftp.MakeDirRecursive(directoryPath);
            ftp.AppendDataArrayAsTextLine(filePath + ".csv", headLine, profItem.buf, profItem.len);
            xSemaphoreGive(mutex_Ethernet);
            M5_LOGI("mutex give");
          }
          else
          {
            M5_LOGW("mutex can not take.");
          }
        }
        free(profItem.buf);
      }
    }

    if (xSemaphoreTake(mutex_Ethernet, 0) == pdTRUE)
    {
      M5_LOGD("mutex take");

      M5_LOGD("Ethernet.maintain();");
      Ethernet.maintain();
      xSemaphoreGive(mutex_Ethernet);
      M5_LOGI("mutex give");
    }
    else
    {
      M5_LOGW("mutex can not take.");
    }

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);
    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
  }

  if (ftp.isConnected())
  {
    if (xSemaphoreTake(mutex_Ethernet, (TickType_t)MUX_BLOCK_TIM) == pdTRUE)
    {
      M5_LOGD("mutex take");

      ftp.CloseConnection();
      xSemaphoreGive(mutex_Ethernet);
      M5_LOGI("mutex give");
    }
    else
    {
      M5_LOGW("mutex can not take.");
    }
  }
  vTaskDelete(NULL);
}

bool DataSave_NeedOffset(u_int16_t SaveInterval)
{
  const int intervals[] = {3600, 600, 300, 10, 5};
  for (int i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
  {
    if (SaveInterval % intervals[i] == 0)
    {
      return true;
    }
  }
  return false;
}

bool DataSave_Trigger(unsigned long currentEpoch, u_int16_t SaveInterval, unsigned long nextSaveEpoc)
{
  if (!DataSave_NeedOffset(SaveInterval))
    return currentEpoch >= nextSaveEpoc;

  String ms = NtpClient.readMinute(currentEpoch);
  char mc0 = ms[0];
  char mc1 = ms[1];
  String ss = NtpClient.readSecond(currentEpoch);
  char sc0 = ss[0];
  char sc1 = ss[1];

  if (!(SaveInterval % 3600) && mc0 == '0' && mc1 == '0' && sc0 == '0' && sc1 == '0')
  {
    return true;
  }
  else if (SaveInterval < 3600 && !(SaveInterval % 600) && mc1 == '0' && sc0 == '0' && sc1 == '0')
  {
    return true;
  }
  else if (SaveInterval < 600 && !(SaveInterval % 300) && (mc1 == '0' || mc1 == '5') && sc0 == '0' && sc1 == '0')
  {
    return true;
  }
  else if (SaveInterval < 300 && !(SaveInterval % 10) && sc1 == '0')
  {
    return true;
  }
  else if (SaveInterval < 10 && !(SaveInterval % 5) && (sc1 == '0' || sc1 == '5'))
  {
    return true;
  }

  return false;
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
    return YYYY + "/" + YYYY + MM;

  return YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD;
}