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
    if (count >= 60)
    {
      M5_LOGV("");
      NtpClient.updateTimeFromServer(ntpSrvIP_String, +9);
      count = 0;
      M5_LOGV("");
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
        int32_t frame_len = PoECAM.Camera.fb->len;
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
      nextBufferingEpoc = currentEpoch + storeData.imageBufferingInterval;

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
      uint8_t *bitmap_buf = (uint8_t *)ps_malloc(3 * jpegItem.width * jpegItem.height);
      fmt2rgb888(jpegItem.buf, jpegItem.len, jpegItem.pixformat, bitmap_buf);
      u_int16_t edgeX = ImageProcessingLoop_EdgePosition(bitmap_buf, jpegItem);
      u_int16_t edgeX_Last = edgeX;

      if (uxQueueSpacesAvailable(xQueueEdge_Store) < 1)
      {
        EdgeItem tempItem;
        xQueueReceive(xQueueEdge_Store, &tempItem, 0);
      }

      EdgeItem edgeItem = {jpegItem.epoc, edgeX};
      if (xQueueSend(xQueueEdge_Store, &edgeItem, 0) != pdPASS)
      {
        M5_LOGE();
      };

      EdgeItem edgeItemLast = {jpegItem.epoc, edgeX_Last};
      xQueueOverwrite(xQueueEdge_Last, &edgeItemLast);

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
      uint16_t *prof_buf_Last = (uint16_t *)ps_malloc(sizeof(uint16_t) * MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX);

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
          prof_buf_Last[index] = br;
        }
        bitmap_pix += pixLineStep * 3;
        index++;
      }

      free(bitmap_buf);
      count = index;
      while (index < MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX)
      {
        prof_buf[index] = 0u;
        prof_buf_Last[index] = 0u;
        index++;
      }

      ProfItem profileItem = {jpegItem.epoc, count, prof_buf};
      ProfItem profileItem_Last = {jpegItem.epoc, count, prof_buf_Last};
      if (uxQueueSpacesAvailable(xQueueProf_Store) < 1)
      {
        ProfItem tempItem;
        xQueueReceive(xQueueProf_Store, &tempItem, 0);
        free(tempItem.buf);
      }
      xQueueSend(xQueueProf_Store, &profileItem, 0);

      ProfItem lastProfItem;
      if (xQueueReceive(xQueueProf_Last, &lastProfItem, 0) == pdPASS)
      {
        free(lastProfItem.buf);
      }
      xQueueSend(xQueueProf_Last, &profileItem_Last, 0);

      //====================

      if (uxQueueSpacesAvailable(xQueueJpeg_Store) < 1)
      {
        JpegItem tempItem;
        xQueueReceive(xQueueJpeg_Store, &tempItem, 0);
        free(tempItem.buf);
      }
      xQueueSend(xQueueJpeg_Store, &jpegItem, 0);

      uint8_t *frame_Jpeg_Last = (uint8_t *)ps_malloc(jpegItem.len);
      memcpy(frame_Jpeg_Last, jpegItem.buf, jpegItem.len);

      JpegItem lastJpegItem;
      if (xQueueReceive(xQueueJpeg_Last, &lastJpegItem, 0) == pdPASS)
      {
        free(lastJpegItem.buf);
      }
      JpegItem jpegItem_Last = {jpegItem.epoc, frame_Jpeg_Last, jpegItem.len};
      xQueueSend(xQueueJpeg_Last, &jpegItem_Last, 0);

      delay(1);
    }

    delay(1);
  }
  vTaskDelete(NULL);
  M5_LOGE("");
}

uint16_t ImageProcessingLoop_EdgePosition(uint8_t *bitmap_buf, JpegItem taskArgs)
{
  int32_t fb_width = (int32_t)((taskArgs.width));
  int32_t fb_height = (int32_t)((taskArgs.height));
  int32_t xStartRate = (int32_t)(storeData.pixLineEdgeSearchStart);
  int32_t xEndRate = (int32_t)(storeData.pixLineEdgeSearchEnd);
  // M5_LOGI("xStartRate=%d , xEndRate=%d ", xStartRate, xEndRate);

  int32_t xStartPix = (int32_t)((fb_width * xStartRate) / 100);
  int32_t xEndPix = (int32_t)((fb_width * xEndRate) / 100);
  // M5_LOGI("xStartPix=%d , xEndPix=%d ", xStartPix, xEndPix);

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
    // M5_LOGI("%d : %d / %d", x, br, th);
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
    Ethernet.maintain();
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

  while (true)
  {
    if ((Ethernet.linkStatus() != LinkON))
    {
      M5_LOGW("Ethernet link is not on.");
      delay(10000);
      continue;
    }

    if (!ftp.isConnected())
    {
      M5_LOGW("ftp is not Connected.");
      ftp.OpenConnection();
      delay(1000);
    }
    else
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
          ftp.MakeDirRecursive(directoryPath);
          String filePath = directoryPath + "/" + createFilenameFromEpoc(jpegItem.epoc, storeData.ftpImageSaveInterval, false);
          ftp.AppendData(filePath + ".jpg", (u_char *)(jpegItem.buf), (int)(jpegItem.len));
        }
        free(jpegItem.buf);
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
          ftp.MakeDirRecursive(directoryPath);
          String filePath = directoryPath + "/edge_" + createFilenameFromEpoc(edgeItem.epoc, storeData.ftpImageSaveInterval, true);
          String testLine = NtpClient.convertTimeEpochToString(edgeItem.epoc) + "," + String(edgeItem.edgeX);
          ftp.AppendTextLine(filePath + ".csv", testLine);
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
          ftp.MakeDirRecursive(directoryPath);
          String filePath = directoryPath + "/prof_" + createFilenameFromEpoc(profItem.epoc, storeData.ftpImageSaveInterval, true);
          String headLine = NtpClient.convertTimeEpochToString(profItem.epoc);
          ftp.AppendDataArrayAsTextLine(filePath + ".csv", headLine, profItem.buf, profItem.len);
        }
        free(profItem.buf);
      }
    }

    Ethernet.maintain();
    delay(100);
  }

  if (ftp.isConnected())
    ftp.CloseConnection();

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