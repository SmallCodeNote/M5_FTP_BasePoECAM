#include <M5Unified.h>
#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"

#include "main.h"
#include "main_EEPROM_handler.h"
#include "main_HTTP_UI.h"
#include "main_Loop.h"

#include <cmath>
#include <vector>

UBaseType_t stackDepthMax_TimeUpdateLoop;
UBaseType_t stackDepthMax_TimeServerAccessLoop;
UBaseType_t stackDepthMax_ButtonKeepCountLoop;
UBaseType_t stackDepthMax_HTTPLoop;
UBaseType_t stackDepthMax_ImageStoreLoop;
UBaseType_t stackDepthMax_ImageProcessingLoop;
UBaseType_t stackDepthMax_DataSortLoop_Jpeg;
UBaseType_t stackDepthMax_DataSaveLoop_Jpeg;
UBaseType_t stackDepthMax_DataSortLoop_Prof;
UBaseType_t stackDepthMax_DataSaveLoop_Prof;
UBaseType_t stackDepthMax_DataSortLoop_Edge;
UBaseType_t stackDepthMax_DataSaveLoop_Edge;

QueueHandle_t xQueueJpeg_Src;

QueueHandle_t xQueueJpeg_Store;
QueueHandle_t xQueueJpeg_Sorted;
QueueHandle_t xQueueJpeg_Last;

QueueHandle_t xQueueProf_Store;
QueueHandle_t xQueueProf_Sorted;
QueueHandle_t xQueueProf_Last;

QueueHandle_t xQueueEdge_Store;
QueueHandle_t xQueueEdge_Sorted;
QueueHandle_t xQueueEdge_Last;

void TimeUpdateLoop(void *arg)
{
  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_TimeUpdateLoop, pcTaskGetTaskName(NULL));
    delay(100);
    NtpClient.updateCurrentEpoch();
  }

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

void TimeServerAccessLoop(void *arg)
{
  unsigned long count = 60;
  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_TimeServerAccessLoop, pcTaskGetTaskName(NULL));
    delay(10000);

    if (NtpClient.currentEpoch == 0 && count >= 3 || count >= 60)
    {
      NtpClient.updateTimeFromServer(ntpSrvIP_String, +9);
      count = 0;
    }
    else
    {
      count++;
    }
  }
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

void ButtonKeepCountLoop(void *arg)
{
  bool pushBeforeCheckSpan = false;
  int PushKeepSubSecCounter = 0;

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_ButtonKeepCountLoop, pcTaskGetTaskName(NULL));

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
      pushBeforeCheckSpan = true;
    }
    if (pushBeforeCheckSpan && PoECAM.BtnA.releasedFor(1000))
    {
      pushBeforeCheckSpan = false;
    }
    if (pushBeforeCheckSpan && PoECAM.BtnA.pressedFor(1000))
    {
      break;
    }
  }

  M5_LOGW("ESP.restart()");
  delay(1000);
  ESP.restart();

  vTaskDelete(NULL);
}

void ImageStoreLoop(void *arg)
{
  xQueueJpeg_Src = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));

  unsigned long lastCheckEpoc = 0, nextBufferingEpoc = 0;
  unsigned long lastShotMillis = 0, nowShotMillis = 0;
  uint16_t *flashLength = &(storeData.flashLength);
  u_int8_t flashMode = (storeData.flashIntensityMode);

  while (true)
  {
    if (flashMode != storeData.flashIntensityMode)
    {
      unit_flash_set_brightness(storeData.flashIntensityMode);
      flashMode = storeData.flashIntensityMode;
    }

    stackDepthMaxUpdate(&stackDepthMax_ImageStoreLoop, pcTaskGetTaskName(NULL));
    unsigned long currentEpoch = NtpClient.currentEpoch;

    if (currentEpoch == 0)
    {
      currentEpoch = millis() / 1000;
    }

    if (currentEpoch - lastCheckEpoc > 1)
      M5_LOGW("EpocDeltaOver 1:");

    if (currentEpoch >= nextBufferingEpoc)
    {
      if (flashMode > 0)
      {
        digitalWrite(FLASH_EN_PIN, HIGH);
        delay(*flashLength);
      }

      nowShotMillis = millis();
      if (PoECAM.Camera.get())
      {
        digitalWrite(FLASH_EN_PIN, LOW);

        uint8_t *frame_buf = PoECAM.Camera.fb->buf;
        size_t frame_len = PoECAM.Camera.fb->len;
        pixformat_t pixmode = PoECAM.Camera.fb->format;
        u_int32_t fb_width = (u_int32_t)(PoECAM.Camera.fb->width);
        u_int32_t fb_height = (u_int32_t)(PoECAM.Camera.fb->height);

        uint8_t *frame_Jpeg = (uint8_t *)ps_malloc(frame_len);
        memcpy(frame_Jpeg, frame_buf, frame_len);
        PoECAM.Camera.free();

        JpegItem item = {currentEpoch, frame_Jpeg, frame_len, pixmode, fb_width, fb_height};
        addJpegItemToQueue(xQueueJpeg_Src, &item);
      }

      digitalWrite(FLASH_EN_PIN, LOW);

      lastCheckEpoc = currentEpoch;
      nextBufferingEpoc = currentEpoch + storeData.imageBufferingEpochInterval;

      // M5_LOGI("shot interval = %u", nowShotMillis - lastShotMillis);
      lastShotMillis = nowShotMillis;
    }

    delay(100);
  }
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

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
  if (!storeData.pixLineSideWidth)
    return channelSum(bitmap_pix);

  uint8_t *pixStart = bitmap_pix - storeData.pixLineSideWidth * 3;
  uint8_t *pixEnd = bitmap_pix + storeData.pixLineSideWidth * 3;

  uint16_t maxBr = 0;
  uint16_t minBr = 765;
  uint16_t tempBr = 0;

  for (uint8_t *pix = pixStart; pix <= pixEnd; pix += 3)
  {
    tempBr = channelSum(pix);
    if (maxBr < tempBr)
      maxBr = tempBr;
    if (minBr > tempBr)
      minBr = tempBr;
  }

  return maxBr - minBr;
}

void freeJpegItem(void *item)
{
  JpegItem *jpegItem = (JpegItem *)item;
  free(jpegItem->buf);
  free(jpegItem->dirPath);
  free(jpegItem->filePath);
}

void freeProfItem(void *item)
{
  ProfItem *profItem = (ProfItem *)item;
  free(profItem->buf);
  free(profItem->textLine);
  free(profItem->dirPath);
  free(profItem->filePath);
}

void freeEdgeItem(void *item)
{
  EdgeItem *edgeItem = (EdgeItem *)item;
  free(edgeItem->dirPath);
  free(edgeItem->filePath);
}

void freeAllItemFromQueue(QueueHandle_t queueHandle, size_t itemSize, void (*freeFunc)(void *))
{
  if (queueHandle != NULL)
  {
    while (uxQueueMessagesWaiting(queueHandle) > 0)
    {
      void *tempItem = malloc(itemSize);
      if (xQueueReceive(queueHandle, tempItem, 0) == pdTRUE)
      {
        freeFunc(tempItem);
      };
      free(tempItem);
    }
  }
  else
  {
    M5_LOGW("null queue");
  }
}

void freeAllJpegItemFromQueue(QueueHandle_t queueHandle)
{
  freeAllItemFromQueue(queueHandle, sizeof(JpegItem), freeJpegItem);
}

void freeAllProfItemFromQueue(QueueHandle_t queueHandle)
{
  freeAllItemFromQueue(queueHandle, sizeof(ProfItem), freeProfItem);
}

void freeAllEdgeItemFromQueue(QueueHandle_t queueHandle)
{
  freeAllItemFromQueue(queueHandle, sizeof(EdgeItem), freeEdgeItem);
}

void addItemToQueue(QueueHandle_t queueHandle, void *item, size_t itemSize, void (*freeFunc)(void *))
{
  if (queueHandle != NULL)
  {
    if (uxQueueSpacesAvailable(queueHandle) < 1 && uxQueueMessagesWaiting(queueHandle) > 0)
    {
      void *tempItem = malloc(itemSize);
      xQueueReceive(queueHandle, tempItem, 0);
      freeFunc(tempItem);
      free(tempItem);
    }
    xQueueSend(queueHandle, item, 0);
  }
  else
  {
    M5_LOGW("null queue");
  }
}

void addJpegItemToQueue(QueueHandle_t queueHandle, JpegItem *item)
{
  addItemToQueue(queueHandle, item, sizeof(JpegItem), freeJpegItem);
}

void addProfItemToQueue(QueueHandle_t queueHandle, ProfItem *item)
{
  addItemToQueue(queueHandle, item, sizeof(ProfItem), freeProfItem);
}

void addEdgeItemToQueue(QueueHandle_t queueHandle, EdgeItem *item)
{
  addItemToQueue(queueHandle, item, sizeof(EdgeItem), freeEdgeItem);
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

void getImageProfile(uint8_t *bitmap_buf, uint16_t *prof, JpegItem item, size_t *profLen)
{
  uint32_t pixLineStep = (u_int32_t)(storeData.pixLineStep);
  pixLineStep = pixLineStep > item.width ? item.width : pixLineStep;

  int32_t addressStep = 3;
  uint32_t startX = 0, startY = 0, endX = 0, endY = 0;

  getProfStartAndEnd(storeData.pixLineAngle, item.width, item.height, &startX, &endX, &startY, &endY, profLen, &addressStep);

  uint8_t *bitmap_pix = bitmap_buf + startX * 3u + startY * item.width * 3u;
  uint8_t *bitmap_pix_lineEnd = bitmap_buf + endX * 3u + endY * item.width * 3u;

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

void stackDepthMaxUpdate(UBaseType_t *stackDepthMax, const char *functionName)
{
  UBaseType_t stackDepth = uxTaskGetStackHighWaterMark(NULL);
  if (*stackDepthMax < stackDepth)
  {
    M5_LOGW("%s : stackDepthMax : %u -> %u", functionName, *stackDepthMax, stackDepth);
    *stackDepthMax = stackDepth;
  }
}

void ImageProcessingLoop(void *arg)
{
  xQueueJpeg_Store = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));
  xQueueProf_Store = xQueueCreate(MAIN_LOOP_QUEUE_PROF_SRC_SIZE, sizeof(ProfItem));
  xQueueEdge_Store = xQueueCreate(MAIN_LOOP_QUEUE_EDGE_SRC_SIZE, sizeof(EdgeItem));

  xQueueJpeg_Last = xQueueCreate(1, sizeof(JpegItem));
  xQueueProf_Last = xQueueCreate(1, sizeof(ProfItem));
  xQueueEdge_Last = xQueueCreate(1, sizeof(EdgeItem));

  JpegItem item;

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_ImageProcessingLoop, __FUNCTION__);

    if (xQueueJpeg_Src == NULL)
    {
      M5_LOGW("null queue handle");
      delay(storeData.imageBufferingEpochInterval * 1000);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Src);
    while (queueWaitingCount > 0)
    {
      if (xQueueReceive(xQueueJpeg_Src, &item, 0) == pdPASS)
      {
        int bufIndexMax = 3 * item.width * item.height;
        uint8_t *bitmap_buf = (uint8_t *)ps_malloc(bufIndexMax);
        fmt2rgb888(item.buf, item.len, item.pixformat, bitmap_buf);

        uint16_t *prof_buf = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);
        uint16_t *prof_buf_Last = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

        size_t profLen = 0;
        getImageProfile(bitmap_buf, prof_buf, item, &profLen);
        free(bitmap_buf);

        int index = profLen;
        while (index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
        {
          prof_buf[index] = 0u;
          index++;
        }

        memcpy(prof_buf_Last, prof_buf, MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

        ProfItem profileItem = {item.epoc, profLen, prof_buf};
        ProfItem profileItem_Last = {item.epoc, profLen, prof_buf_Last};

        u_int16_t edgeX = ImageProcessingLoop_EdgePosition(profileItem);
        u_int16_t edgeX_Last = edgeX;

        EdgeItem edgeItem = {item.epoc, edgeX};
        EdgeItem edgeItemLast = {item.epoc, edgeX_Last};

        addEdgeItemToQueue(xQueueEdge_Store, &edgeItem);
        addEdgeItemToQueue(xQueueEdge_Last, &edgeItemLast);

        addProfItemToQueue(xQueueProf_Store, &profileItem);
        addProfItemToQueue(xQueueProf_Last, &profileItem_Last);

        //====================
        uint8_t *frame_Jpeg_Last = (uint8_t *)ps_malloc(item.len);
        memcpy(frame_Jpeg_Last, item.buf, item.len);
        JpegItem jpegItem_Last = {item.epoc, frame_Jpeg_Last, item.len, item.pixformat, item.width, item.height};

        addJpegItemToQueue(xQueueJpeg_Store, &item);
        addJpegItemToQueue(xQueueJpeg_Last, &jpegItem_Last);

        delay(1);
      }
      else
      {
        M5_LOGW("queue receive fale");
        delay(100);
      }

      queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Src);
    }

    delay(1);
  }

  vTaskDelete(NULL);
  M5_LOGE("Loop STOP");
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
    stackDepthMaxUpdate(&stackDepthMax_HTTPLoop, __FUNCTION__);
    HTTP_UI();
    delay(10);
  }

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

bool ftpOpenCheck()
{
  uint16_t responceCode = 0;
  if (!ftp.isConnected())
  {
    M5_LOGW("ftp is not Connected.");

    if (xSemaphoreTake(mutex_Eth_SocketOpen, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
    {
      mutex_Eth_SocketOpen_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      responceCode = ftp.OpenConnection();
      M5_LOGI("FTP connection socketIndex = %u", ftp.getSockIndex());
      mutex_Eth_SocketOpen_Take_FunctionName = String("Free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      xSemaphoreGive(mutex_Eth_SocketOpen);
    }
    else
    {
      M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_SocketOpen_Take_FunctionName.c_str());
      return false;
    }
  }

  if (ftp.isErrorCode(responceCode))
  {
    M5_LOGW("ERROR : %u", responceCode);
    return false;
  }
  M5_LOGI("SUCCESS : %u", responceCode);
  return true;
}

bool ftpCloseCheck()
{
  if (ftp.isConnected())
  {
    if (xSemaphoreTake(mutex_Eth_SocketOpen, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
    {
      mutex_Eth_SocketOpen_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      ftp.CloseConnection();
      mutex_Eth_SocketOpen_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      xSemaphoreGive(mutex_Eth_SocketOpen);
    }
    else
    {
      M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_SocketOpen_Take_FunctionName.c_str());
      return false;
    }
  }
  return true;
}

void DataSortLoop_Jpeg(void *arg)
{
  xQueueJpeg_Sorted = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));
  QueueHandle_t xQueue_FreeWaiting = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));

  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = millis();

  int queueCheckFrequency = storeData.ftpImageSaveInterval / storeData.imageBufferingEpochInterval;
  String directoryPath_Before = "";

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_DataSortLoop_Jpeg, __FUNCTION__);

    if (xQueueJpeg_Store == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Store);
    if (queueWaitingCount > queueCheckFrequency)
    {
      JpegItem item;
      u_int16_t saveInterval = storeData.ftpImageSaveInterval;
      directoryPath_Before = "";

      while (xQueueReceive(xQueueJpeg_Store, &item, 0) == pdTRUE)
      {
        if (item.epoc < lastCheckEpoc)
          lastCheckEpoc = 0;
        if (item.epoc - lastCheckEpoc > 1)
          M5_LOGW("JpegEpocDeltaOver 1:");
        lastCheckEpoc = item.epoc;

        if (DataSave_Trigger(item.epoc, saveInterval, nextSaveEpoc))
        {
          nextSaveEpoc = item.epoc + saveInterval;

          String dirPath = (String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(item.epoc, saveInterval, false));
          String filePath = (String(dirPath) + "/" + createFilenameFromEpoc(item.epoc, saveInterval, false));

          stringCopyToItemsCharArray(item, dirPath);
          stringCopyToItemsCharArray(item, filePath);

          addJpegItemToQueue(xQueueJpeg_Sorted, &item);
        }
        else
        {
          addJpegItemToQueue(xQueue_FreeWaiting, &item);
        }
      }
    }

    // M5_LOGI("loopTime = %u", millis() - loopStartMillis);
    freeAllJpegItemFromQueue(xQueue_FreeWaiting);

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
    loopStartMillis = millis();
  }

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

void DataSaveLoop_Jpeg(void *arg)
{
  QueueHandle_t xQueue_FreeWaiting = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(JpegItem));
  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = millis();
  unsigned long mutex_FTP_Take_Time = 0, mutex_Eth_Take_Time;
  unsigned long mutex_FTP_Give_Time = 0, mutex_Eth_Give_Time;
  unsigned long mutex_Eth_Sum_Time = 0, mutex_Eth_TakeCount = 0;
  String directoryPath_Before = "";

  while (true)
  {
    loopStartMillis = millis();
    mutex_Eth_TakeCount = 0;
    mutex_Eth_Sum_Time = 0;
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Jpeg, __FUNCTION__);

    if (xQueueJpeg_Sorted == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Sorted);
    if (queueWaitingCount > 0)
    {
      u_int16_t saveInterval = storeData.ftpImageSaveInterval;
      JpegItem item;

      directoryPath_Before = "";

      // take FTP mutex
      mutex_FTP_Take_Time = millis();
      if (xSemaphoreTake(mutex_FTP, (TickType_t)MUX_FTP_BLOCK_TIM) != pdTRUE)
      {
        M5_LOGW("ftp mutex can not take. : take function = %s", mutex_FTP_Take_FunctionName.c_str());
        delay(100);
        continue;
      }

      mutex_FTP_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(mutex_FTP_Take_Time) + "]";
      ftpOpenCheck();

      if (!ftp.isConnected())
      {
        xSemaphoreGive(mutex_FTP);
        mutex_FTP_Give_Time = millis();
        M5_LOGI("ftp mutex give");
        mutex_FTP_Take_FunctionName = String("mutex_FTP free by error ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
        delay(100);
        continue;
      }

      queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Sorted);
      uint16_t responce = 0;
      unsigned long responceTimeBase = 0;

      while (queueWaitingCount > 0)
      {
        if (xQueueReceive(xQueueJpeg_Sorted, &item, 0) == pdTRUE)
        {
          // take Eth mutex
          mutex_Eth_Take_Time = millis();
          mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(mutex_Eth_Take_Time) + "]";
          if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
          {
            // Create Directory

            if (directoryPath_Before != item.dirPath)
            {
              directoryPath_Before = item.dirPath;
              responceTimeBase = millis();
              responce = ftp.MakeDirRecursive(item.dirPath);
              M5_LOGI("item.dirPath = %s", item.dirPath);
              M5_LOGI("ftp res = %u :: %u", responce, millis() - responceTimeBase);
            }

            responceTimeBase = millis();
            responce = ftp.AppendData(String(item.filePath) + ".jpg", (u_char *)(item.buf), (int)(item.len));
            M5_LOGI("ftp res = %u :: %u", responce, millis() - responceTimeBase);

            xSemaphoreGive(mutex_Eth);
            mutex_Eth_Give_Time = millis();
            mutex_Eth_Take_FunctionName = String("mutex_Eth free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

            mutex_Eth_Sum_Time += (mutex_Eth_Give_Time - mutex_Eth_Take_Time);
            mutex_Eth_TakeCount++;
          }
          else
          {
            M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
          }

          addJpegItemToQueue(xQueue_FreeWaiting, &item);
        }

        delay(1);

        queueWaitingCount = uxQueueMessagesWaiting(xQueueJpeg_Sorted);
      }

      xSemaphoreGive(mutex_FTP);
      mutex_FTP_Give_Time = millis();
      mutex_FTP_Take_FunctionName = String("mutex_FTP free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]" + " ((" + String(mutex_FTP_Give_Time - mutex_FTP_Take_Time) + "))";

      M5_LOGI("mutex report :: ftp: %u  / eth: %u | count: %u", mutex_FTP_Give_Time - mutex_FTP_Take_Time, mutex_Eth_Sum_Time / mutex_Eth_TakeCount, mutex_Eth_TakeCount);
      // M5_LOGI("loopTime = %u ms", millis() - loopStartMillis);

      delay(100);
      freeAllJpegItemFromQueue(xQueue_FreeWaiting);
    }

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
    loopStartMillis = millis();
  }

  ftpCloseCheck();
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

String ProfDataStringCreate(u_int16_t *buf, int32_t len)
{
  String dataLine = String(buf[0]);
  dataLine.reserve(len * 6);
  for (int32_t i = 1; i < len; i++)
  {
    dataLine += ("," + String(buf[i]));
  }
  return dataLine;
}

void DataSortLoop_Prof(void *arg)
{
  xQueueProf_Sorted = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(ProfItem));
  QueueHandle_t xQueue_FreeWaiting = xQueueCreate(MAIN_LOOP_QUEUE_PROF_SRC_SIZE, sizeof(ProfItem));

  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = millis();

  int queueCheckFrequency = storeData.ftpProfileSaveInterval / storeData.imageBufferingEpochInterval;
  String directoryPath_Before = "";

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Prof, __FUNCTION__);

    if (xQueueProf_Store == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueProf_Store);
    // M5_LOGI("uxQueueMessagesWaiting = %u", queueWaitingCount);
    if (queueWaitingCount > queueCheckFrequency)
    {
      ProfItem item;
      u_int16_t saveInterval = storeData.ftpProfileSaveInterval;
      directoryPath_Before = "";

      while (xQueueReceive(xQueueProf_Store, &item, 0))
      {
        if (item.epoc < lastCheckEpoc)
          lastCheckEpoc = 0;
        if (item.epoc - lastCheckEpoc > 1)
          M5_LOGW("ProfEpocDeltaOver 1:");
        lastCheckEpoc = item.epoc;

        if (DataSave_Trigger(item.epoc, saveInterval, nextSaveEpoc))
        {
          nextSaveEpoc = item.epoc + saveInterval;

          String dirPath = (String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(item.epoc, saveInterval, true));
          String filePath = (String(dirPath) + "/prof_" + createFilenameFromEpoc(item.epoc, saveInterval, true));

          stringCopyToItemsCharArray(item, dirPath);
          stringCopyToItemsCharArray(item, filePath);

          String headLine = NtpClient.convertTimeEpochToString(item.epoc);
          String dataLine = ProfDataStringCreate(item.buf, item.len);
          String textLine = headLine + "," + dataLine;

          item.textLineLen = textLine.length();
          stringCopyToItemsCharArray(item, textLine);

          addProfItemToQueue(xQueueProf_Sorted, &item);
        }
        else
        {
          addProfItemToQueue(xQueue_FreeWaiting, &item);
        }
      }
    }

    // M5_LOGI("loopTime = %u", millis() - loopStartMillis);
    freeAllProfItemFromQueue(xQueue_FreeWaiting);

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
    loopStartMillis = millis();
  }

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

String dataSaveLoop_Prof_DataLines; // dataLines Buff
void DataSaveLoop_Prof(void *arg)
{
  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = 0;
  unsigned long mutex_FTP_Take_Time = 0, mutex_Eth_Take_Time;
  unsigned long mutex_FTP_Give_Time = 0, mutex_Eth_Give_Time;
  String directoryPath_Before = "";

  while (true)
  {
    loopStartMillis = millis();
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Prof, __FUNCTION__);

    if (xQueueProf_Sorted == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueProf_Sorted);
    if (queueWaitingCount > 0)
    {
      std::vector<String> directryPath_CheckList;
      u_int16_t saveInterval = storeData.ftpProfileSaveInterval;
      String dataLines;
      ProfItem item;

      directoryPath_Before = "";
      queueWaitingCount = uxQueueMessagesWaiting(xQueueProf_Sorted);

      while (queueWaitingCount > 0)
      {
        if (xQueueReceive(xQueueProf_Sorted, &item, 0) == pdTRUE)
        {
          if (directoryPath_Before != item.dirPath)
          {
            directoryPath_Before = item.dirPath;
            directryPath_CheckList.push_back(String(item.dirPath));
          }
          dataLines += String(item.textLine) + "\r\n";
        }
        queueWaitingCount = uxQueueMessagesWaiting(xQueueProf_Sorted);
      }

      dataSaveLoop_Prof_DataLines += dataLines;

      // take FTP mutex
      mutex_FTP_Take_Time = millis();
      if (xSemaphoreTake(mutex_FTP, (TickType_t)MUX_FTP_BLOCK_TIM) != pdTRUE)
      {
        M5_LOGW("ftp mutex can not take. : take function = %s", mutex_FTP_Take_FunctionName.c_str());
        delay(100);
        continue;
      }

      mutex_FTP_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(mutex_FTP_Take_Time) + "]";
      ftpOpenCheck();

      if (!ftp.isConnected())
      {
        xSemaphoreGive(mutex_FTP);
        mutex_FTP_Give_Time = millis();
        M5_LOGW("ftp mutex give by connection error");
        mutex_FTP_Take_FunctionName = String("mutex_FTP free by error ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
        delay(100);
        continue;
      }

      // take Eth mutex
      mutex_Eth_Take_Time = millis();
      mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(mutex_Eth_Take_Time) + "]";
      if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
      {
        // Create Directory

        for (size_t i = 0; i < directryPath_CheckList.size(); ++i)
        {
          M5_LOGI("%s", directryPath_CheckList[i].c_str());
          ftp.MakeDirRecursive(directryPath_CheckList[i]);
        }

        unsigned char *dataLinesBuff = (unsigned char *)dataSaveLoop_Prof_DataLines.c_str();
        unsigned int dataLinesLength = dataSaveLoop_Prof_DataLines.length();
        ftp.AppendData(String(item.filePath) + ".csv", dataLinesBuff, dataLinesLength);

        xSemaphoreGive(mutex_Eth);
        mutex_Eth_Give_Time = millis();
        mutex_Eth_Take_FunctionName = String("mutex_Eth free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
        directoryPath_Before = item.dirPath;
      }
      else
      {
        M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
      }

      xSemaphoreGive(mutex_FTP);
      mutex_FTP_Give_Time = millis();
      mutex_FTP_Take_FunctionName = String("mutex_FTP free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]" + " ((" + String(mutex_FTP_Give_Time - mutex_FTP_Take_Time) + "))";

      M5_LOGI("mutex report :: ftp: %u  / eth: %u", mutex_FTP_Give_Time - mutex_FTP_Take_Time, mutex_Eth_Give_Time - mutex_Eth_Take_Time);
      // M5_LOGI("loopTime = %u ms", millis() - loopStartMillis);

      delay(100);
      dataSaveLoop_Prof_DataLines.clear();
    }

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
    loopStartMillis = millis();
  }

  ftpCloseCheck();
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

/*void DataSaveLoop_Prof(void *arg)
{
  QueueHandle_t xQueue_FreeWaiting = xQueueCreate(MAIN_LOOP_QUEUE_PROF_SRC_SIZE, sizeof(ProfItem));

  unsigned long lastCheckEpoc_Prof = 0, nextSaveEpoc_Prof = 0;
  unsigned long loopStartMillis = millis();

  int queueCheckFrequency = storeData.ftpProfileSaveInterval / storeData.imageBufferingEpochInterval;
  String directoryPath_Before = "";

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Prof, __FUNCTION__);

    if (xSemaphoreTake(mutex_FTP, MUX_FTP_BLOCK_TIM) != pdTRUE)
    {
      M5_LOGW("ftp mutex can not take. : take function = %s", mutex_FTP_Take_FunctionName.c_str());
      delay(100);
      continue;
    }
    M5_LOGD("ftp mutex take");
    mutex_FTP_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

    ftpOpenCheck();

    if (ftp.isConnected())
    {
      M5_LOGD("ftp is Connected.");

      UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueProf_Store);
      M5_LOGI("uxQueueMessagesWaiting = %u", queueWaitingCount);
      if (queueWaitingCount > queueCheckFrequency)
      {

        ProfItem profItem;
        u_int16_t saveInterval = storeData.ftpProfileSaveInterval;
        directoryPath_Before = "";

        while (xQueueReceive(xQueueProf_Store, &profItem, 0))
        {
          if (profItem.epoc < lastCheckEpoc_Prof)
            lastCheckEpoc_Prof = 0;
          if (profItem.epoc - lastCheckEpoc_Prof > 1)
            M5_LOGW("ProfEpocDeltaOver 1:");
          lastCheckEpoc_Prof = profItem.epoc;

          if (DataSave_Trigger(profItem.epoc, saveInterval, nextSaveEpoc_Prof))
          {
            M5_LOGI("prof %u", saveInterval);
            nextSaveEpoc_Prof = profItem.epoc + saveInterval;
            String directoryPath = String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(profItem.epoc, storeData.ftpProfileSaveInterval, true);
            String filePath = directoryPath + "/prof_" + createFilenameFromEpoc(profItem.epoc, storeData.ftpProfileSaveInterval, true);
            String headLine = NtpClient.convertTimeEpochToString(profItem.epoc);

            String dataLine = ProfDataStringCreate(profItem.buf, profItem.len);

            M5_LOGD("Eth mutex take");
            mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
            if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
            {
              if (directoryPath_Before != directoryPath)
                ftp.MakeDirRecursive(directoryPath);
              unsigned long intstartmillis = millis();
              // ftp.AppendDataArrayAsTextLine(filePath + ".csv", headLine, profItem.buf, profItem.len);
              ftp.AppendTextLine(filePath + ".csv", headLine + "," + dataLine);
              M5_LOGD("prof out : %u", millis() - intstartmillis);

              xSemaphoreGive(mutex_Eth);
              M5_LOGI("Eth mutex give");
              mutex_Eth_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

              M5_LOGV("\n    FTP: %s", filePath.c_str());
              directoryPath_Before = directoryPath;
            }
            else
            {
              M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
            }
          }

          addProfItemToQueue(xQueue_FreeWaiting, &profItem);
        }
      }
    }
    else
    {
      delay(100);
      xSemaphoreGive(mutex_FTP);
      M5_LOGI("ftp mutex give");
      mutex_FTP_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

      loopStartMillis = millis();
      continue;
    }
    delay(100);

    xSemaphoreGive(mutex_FTP);
    M5_LOGI("ftp mutex give");
    mutex_FTP_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

    freeAllProfItemFromQueue(xQueue_FreeWaiting);

    M5_LOGI("loopTime = %u", millis() - loopStartMillis);
    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);
    delay(loopEndDelay < 0 ? 1 : loopEndDelay);

    loopStartMillis = millis();
  }

  ftpCloseCheck();

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}*/

void DataSortLoop_Edge(void *arg)
{
  xQueueEdge_Sorted = xQueueCreate(MAIN_LOOP_QUEUE_JPEG_SRC_SIZE, sizeof(EdgeItem));

  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = millis();

  int queueCheckFrequency = storeData.ftpEdgeSaveInterval / storeData.imageBufferingEpochInterval;
  queueCheckFrequency = queueCheckFrequency < 10 ? 10 : queueCheckFrequency;

  String directoryPath_Before = "";

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_DataSortLoop_Edge, __FUNCTION__);

    if (xQueueEdge_Store == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueEdge_Store);
    if (queueWaitingCount > queueCheckFrequency)
    {
      EdgeItem item;
      u_int16_t saveInterval = storeData.ftpEdgeSaveInterval;
      directoryPath_Before = "";

      while (xQueueReceive(xQueueEdge_Store, &item, 0) == pdTRUE)
      {
        if (item.epoc < lastCheckEpoc)
          lastCheckEpoc = 0;
        if (item.epoc - lastCheckEpoc > 1)
          M5_LOGW("EdgeEpocDeltaOver 1:");
        lastCheckEpoc = item.epoc;

        if (DataSave_Trigger(item.epoc, saveInterval, nextSaveEpoc))
        {
          nextSaveEpoc = item.epoc + saveInterval;

          String dirPath = (String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(item.epoc, saveInterval, true));
          String filePath = (String(dirPath) + "/edge_" + createFilenameFromEpoc(item.epoc, saveInterval, true));

          stringCopyToItemsCharArray(item, dirPath);
          stringCopyToItemsCharArray(item, filePath);

          addEdgeItemToQueue(xQueueEdge_Sorted, &item);
        }
      }
    }

    // M5_LOGI("loopTime = %u", millis() - loopStartMillis);

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);
    loopStartMillis = millis();
  }

  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

String dataSaveLoop_Edge_DataLines; // dataLines Buff
void DataSaveLoop_Edge(void *arg)
{
  unsigned long lastCheckEpoc = 0, nextSaveEpoc = 0;
  unsigned long loopStartMillis = 0;
  unsigned long mutex_FTP_Take_Time = 0, mutex_Eth_Take_Time;
  unsigned long mutex_FTP_Give_Time = 0, mutex_Eth_Give_Time;
  String directoryPath_Before = "";

  while (true)
  {
    loopStartMillis = millis();
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Edge, __FUNCTION__);

    if (xQueueEdge_Sorted == NULL)
    {
      M5_LOGW("null queue");
      delay(100);
      continue;
    }

    UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueEdge_Sorted);
    if (queueWaitingCount > 0)
    {
      std::vector<String> directryPath_CheckList;
      u_int16_t saveInterval = storeData.ftpEdgeSaveInterval;
      String dataLines;
      EdgeItem item;

      directoryPath_Before = "";
      queueWaitingCount = uxQueueMessagesWaiting(xQueueEdge_Sorted);
      dataLines.reserve((queueWaitingCount + 2) * 30);

      while (queueWaitingCount > 0)
      {
        if (xQueueReceive(xQueueEdge_Sorted, &item, 0) == pdTRUE)
        {
          if (directoryPath_Before != item.dirPath)
          {
            directoryPath_Before = item.dirPath;
            directryPath_CheckList.push_back(String(item.dirPath));
          }
          dataLines += NtpClient.convertTimeEpochToString(item.epoc) + "," + String(item.edgeX) + "\r\n";
        }
        queueWaitingCount = uxQueueMessagesWaiting(xQueueEdge_Sorted);
      }

      dataSaveLoop_Edge_DataLines += dataLines;

      // take FTP
      mutex_FTP_Take_Time = millis();
      if (xSemaphoreTake(mutex_FTP, (TickType_t)MUX_FTP_BLOCK_TIM) != pdTRUE)
      {
        M5_LOGW("ftp mutex can not take. : take function = %s", mutex_FTP_Take_FunctionName.c_str());
        delay(100);
        continue;
      }

      mutex_FTP_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      ftpOpenCheck();

      if (!ftp.isConnected())
      {
        xSemaphoreGive(mutex_FTP);
        M5_LOGW("ftp mutex give by connection error");
        mutex_FTP_Take_FunctionName = String("mutex_FTP free by error ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
        delay(100);
        continue;
      }

      // take Eth
      mutex_Eth_Take_Time = millis();
      mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(mutex_Eth_Take_Time) + "]";
      if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
      {
        // Create Directory
        for (size_t i = 0; i < directryPath_CheckList.size(); ++i)
        {
          M5_LOGI("%s", directryPath_CheckList[i].c_str());
          ftp.MakeDirRecursive(directryPath_CheckList[i]);
        }

        unsigned char *dataLinesBuff = (unsigned char *)dataSaveLoop_Edge_DataLines.c_str();
        unsigned int dataLinesLength = dataSaveLoop_Edge_DataLines.length();
        ftp.AppendData(String(item.filePath) + ".csv", dataLinesBuff, dataLinesLength);

        xSemaphoreGive(mutex_Eth);
        mutex_Eth_Give_Time = millis();
        mutex_Eth_Take_FunctionName = String("mutex_Eth free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
        directoryPath_Before = item.dirPath;
      }
      else
      {
        M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
      }

      xSemaphoreGive(mutex_FTP);
      mutex_FTP_Give_Time = millis();
      mutex_FTP_Take_FunctionName = String("mutex_FTP free ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]" + " ((" + String(mutex_FTP_Give_Time - mutex_FTP_Take_Time) + "))";

      M5_LOGI("mutex report :: ftp: %u  / eth: %u", mutex_FTP_Give_Time - mutex_FTP_Take_Time, mutex_Eth_Give_Time - mutex_Eth_Take_Time);
      // M5_LOGI("loopTime = %u ms", millis() - loopStartMillis);

      delay(100);
      dataSaveLoop_Edge_DataLines.clear();
    }

    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    // M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);

    delay(loopEndDelay < 0 ? 1 : loopEndDelay);

    loopStartMillis = millis();
  }

  ftpCloseCheck();
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}

/*
void DataSaveLoop_Edge(void *arg)
{
  unsigned long lastCheckEpoc_Edge = 0;
  unsigned long nextSaveEpoc_Edge = 0;
  unsigned long loopStartMillis = millis();

  int queueCheckFrequency = storeData.ftpEdgeSaveInterval / storeData.imageBufferingEpochInterval;
  queueCheckFrequency = queueCheckFrequency < 10 ? 10 : queueCheckFrequency;

  String directoryPath_Before = "";

  while (true)
  {
    stackDepthMaxUpdate(&stackDepthMax_DataSaveLoop_Edge, __FUNCTION__);
    if (xSemaphoreTake(mutex_FTP, MUX_FTP_BLOCK_TIM) != pdTRUE)
    {
      M5_LOGW("ftp mutex can not take. : last take function = %s", mutex_FTP_Take_FunctionName.c_str());
      delay(100);
      continue;
    }
    M5_LOGD("ftp mutex take");
    mutex_FTP_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

    ftpOpenCheck();

    if (ftp.isConnected())
    {
      M5_LOGD("ftp is Connected.");

      UBaseType_t queueWaitingCount = uxQueueMessagesWaiting(xQueueEdge_Store);
      M5_LOGI("uxQueueMessagesWaiting = %u", queueWaitingCount);
      if (queueWaitingCount > queueCheckFrequency)
      {
        EdgeItem edgeItem;
        u_int16_t saveInterval = storeData.ftpEdgeSaveInterval;
        directoryPath_Before = "";

        while (xQueueReceive(xQueueEdge_Store, &edgeItem, 0))
        {
          if (edgeItem.epoc < lastCheckEpoc_Edge)
            lastCheckEpoc_Edge = 0;
          if (edgeItem.epoc - lastCheckEpoc_Edge > 1)
            M5_LOGW("EdgeEpocDeltaOver 1:");
          lastCheckEpoc_Edge = edgeItem.epoc;

          if (DataSave_Trigger(edgeItem.epoc, saveInterval, nextSaveEpoc_Edge))
          {
            M5_LOGI("Edge save : interval = %u", saveInterval);
            nextSaveEpoc_Edge = edgeItem.epoc + saveInterval;
            String directoryPath = String(storeData.deviceName) + "/" + createDirectorynameFromEpoc(edgeItem.epoc, storeData.ftpEdgeSaveInterval, true);
            String filePath = directoryPath + "/edge_" + createFilenameFromEpoc(edgeItem.epoc, storeData.ftpEdgeSaveInterval, true);
            String testLine = NtpClient.convertTimeEpochToString(edgeItem.epoc) + "," + String(edgeItem.edgeX);

            M5_LOGD("Eth mutex take");
            mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
            if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
            {
              if (directoryPath_Before != directoryPath)
                // if (!ftp.DirExists(directoryPath))
                ftp.MakeDirRecursive(directoryPath);
              ftp.AppendTextLine(filePath + ".csv", testLine);

              xSemaphoreGive(mutex_Eth);
              M5_LOGI("Eth mutex give");
              mutex_Eth_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

              M5_LOGV("\n    FTP: %s", filePath.c_str());
              directoryPath_Before = directoryPath;
            }
            else
            {
              M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
            }
          }
        }
      }
    }
    else
    {
      delay(100);
      xSemaphoreGive(mutex_FTP);
      M5_LOGI("ftp mutex give");
      mutex_FTP_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

      delay(100);
      loopStartMillis = millis();
      continue;
    }

    delay(100);
    xSemaphoreGive(mutex_FTP);
    M5_LOGI("ftp mutex give");
    mutex_FTP_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";

    M5_LOGI("loopTime = %u", millis() - loopStartMillis);
    int loopEndDelay = storeData.imageBufferingEpochInterval * 1000 - (millis() - loopStartMillis);
    M5_LOGI("%d, %u", loopEndDelay, storeData.imageBufferingEpochInterval);
    delay(loopEndDelay < 0 ? 1 : loopEndDelay);

    loopStartMillis = millis();
  }

  ftpCloseCheck();
  M5_LOGE("Loop STOP");
  vTaskDelete(NULL);
}*/

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

  if (SaveInterval == 0)
    return false;

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
    return YYYY + MM + DD + "_" + HH + String(mm[0]) + "0";
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

  if (multiLine)
  {
    if (interval >= 30)
      return YYYY + "/" + YYYY + MM;

    return YYYY + "/" + YYYY + MM + DD;
  }
  else
  {
    if (interval >= 3600) // ~ 744files / directory
      return YYYY + "/" + YYYY + MM;
    if (interval >= 120) // 24 ~ 720files / directory
      return YYYY + "/" + YYYY + MM + DD;
    if (interval >= 60) // 360~ 720files / directory
      return YYYY + "/" + YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 12) + "00";
    if (interval >= 30) // 360~ 720files / directory
      return YYYY + "/" + YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 6) + "00";
    if (interval >= 10) // 240~ 720files / directory
      return YYYY + "/" + YYYY + MM + DD + "/" + YYYY + MM + DD + "_" + NtpClient.readHour(currentEpoch, 2) + "00";
    if (interval >= 5) // 360~ 720files / directory
      return YYYY + "/" + YYYY + MM + DD + "/" + YYYY + MM + DD + "_" + HH + "00";
    // 120~ 600files / directory
    return YYYY + "/" + YYYY + MM + DD + "/" + YYYY + MM + DD + "_" + HH + String(mm[0]) + "0";
  }
  return YYYY + "/" + YYYY + MM + "/" + YYYY + MM + DD;
}