#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

#include "main.h"
/*
typedef struct
{
  unsigned long currentEpoch;
} SensorShotTaskParams;
*/

//void SensorShotLoop(void *arg);
//void SensorShotTask(void *param);


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