#ifndef MAIN_LOOP_FUNCTION_H
#define MAIN_LOOP_FUNCTION_H

#define MAIN_LOOP_QUEUE_JPEG_SRC_SIZE 30
#define MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX 1024

typedef struct
{
  unsigned long currentEpoch;
} SensorShotTaskParams;

struct JpegItem
{
  unsigned long epoc;
  uint8_t *buf;
  int32_t len;
  pixformat_t pixformat;
  u_int32_t width;
  u_int32_t height;
};

struct ProfItem
{
  unsigned long epoc;
  int32_t len;
  u_int16_t *buf;
};

struct EdgeItem
{
  unsigned long epoc;
  u_int16_t edgeX;
};

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

String createFilenameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);
String createDirectorynameFromEpoc(unsigned long currentEpoch, u_int16_t interval, bool multiLine);



extern QueueHandle_t xQueueJpeg_Store;
extern QueueHandle_t xQueueEdge_Store;
extern QueueHandle_t xQueueProf_Store;

#endif