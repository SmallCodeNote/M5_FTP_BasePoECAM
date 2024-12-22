#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

#include "main.h"
/*
typedef struct
{
  unsigned long currentEpoch;
} SensorShotTaskParams;
*/

// void SensorShotLoop(void *arg);
// void SensorShotTask(void *param);

#define stringCopyToItemsCharArray(item, field)      \
  do                                                 \
  {                                                  \
    item.field = (char *)malloc(field.length() + 1); \
    if (item.field != NULL)                          \
    {                                                \
      strcpy(item.field, field.c_str());             \
    }                                                \
  } while (0)

#define mutexTake_AndFaleContinue(mutex , blockTime)                                                          \
  unsigned long ##mutex _TakeMillis = millis();                                                    \
  if (xSemaphoreTake(mutex, (TickType_t) blockTime ) != pdTRUE)                         \
  {                                                                                               \
    M5_LOGW("mutex can not take. : take function = %s", (##mutex _Take_FunctionName).c_str()); \
    delay(100);                                                                                   \
    loopStartMillis = millis();                                                                   \
    continue;                                                                                     \
  }                                                                                               \
  M5_LOGD("ftp mutex take");                                                                      \
  ##mutex _Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(##mutex _TakeMillis) + "]";


#define mutexGive(mutex)      \
  xSemaphoreGive(mutex);      \
  M5_LOGI("%s give", #mutex); \
  ##mutex _Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " ((" + String(millis() - ##mutex _TakeMillis) + "))";

void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);

void ButtonKeepCountLoop(void *arg);

void ImageStoreLoop(void *arg);
void ImageProcessingLoop(void *arg);

void HTTPLoop(void *arg);

void DataSortLoop_Jpeg(void *arg);
void DataSaveLoop_Jpeg(void *arg);

void DataSortLoop_Prof(void *arg);
void DataSaveLoop_Prof(void *arg);

void DataSortLoop_Edge(void *arg);
void DataSaveLoop_Edge(void *arg);

uint16_t ImageProcessingLoop_EdgePosition(ProfItem profItem);

void stackDepthMaxUpdate(UBaseType_t *stackDepthMax, const char *functionName);

String createFilenameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);
String createDirectorynameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);

bool DataSave_Trigger(unsigned long currentEpoch, u_int16_t SaveInterval, unsigned long nextSaveEpoc);

void addEdgeItemToQueue(QueueHandle_t queueHandle, EdgeItem *edgeItem);
void addProfItemToQueue(QueueHandle_t queueHandle, ProfItem *profileItem);
void addJpegItemToQueue(QueueHandle_t queueHandle, JpegItem *jpegItem);

#endif