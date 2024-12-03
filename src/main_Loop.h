#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

#define MAIN_LOOP_QUEUE_JPEG_SRC_SIZE 30
#define MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX 1024

#include "main.h"

typedef struct 
{
  unsigned long currentEpoch;
}SensorShotTaskParams;



void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);

void ButtonKeepCountLoop(void *arg);

void SensorShotLoop(void *arg);
void SensorShotTask(void *param);
bool SensorShotTaskRunTrigger(unsigned long currentEpoch);
unsigned long SensorShotStartOffset();

void ImageStoreLoop(void *arg);
void ImageProcessingLoop(void *arg);
uint16_t ImageProcessingLoop_EdgePosition(uint8_t *bitmap_buf, JpegItem taskArgs);
void DataSaveLoop(void *arg);

String createFilenameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);
String createDirectorynameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);

bool DataSave_Trigger(unsigned long currentEpoch, u_int16_t SaveInterval, unsigned long nextSaveEpoc);

#endif