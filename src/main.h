#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"
#include "M5PoECAM.h"

#ifndef MAIN_BASE_FUNCTION_H
#define MAIN_BASE_FUNCTION_H

#define FLASH_EN_PIN 25

#define MUX_BLOCK_TIM 1000 // mutex block time

#define MAIN_LOOP_QUEUE_JPEG_SRC_SIZE 60
#define MAIN_LOOP_QUEUE_EDGE_SRC_SIZE 120
#define MAIN_LOOP_QUEUE_PROF_SRC_SIZE 60
#define MAIN_LOOP_QUEUE_PROFILE_WIDTH_MAX 1024

struct JpegItem
{
  unsigned long epoc;
  uint8_t *buf;
  size_t len;
  pixformat_t pixformat;
  u_int32_t width;
  u_int32_t height;
};

struct ProfItem
{
  unsigned long epoc;
  size_t len;
  u_int16_t *buf;
};

struct EdgeItem
{
  unsigned long epoc;
  u_int16_t edgeX;
};

void CameraSensorFullSetupFromStoreData();
void CameraSensorSetJPEG();
void CameraSensorSetGRAYSCALE();

void unit_flash_set_brightness(uint8_t brightness);

uint16_t CameraSensorFrameWidth(framesize_t framesize);
uint16_t CameraSensorFrameHeight(framesize_t framesize);

extern M5_Ethernet_FtpClient ftp;
extern M5_Ethernet_NtpClient NtpClient;

extern QueueHandle_t xQueueJpeg_Store;
extern QueueHandle_t xQueueEdge_Store;
extern QueueHandle_t xQueueProf_Store;

extern QueueHandle_t xQueueJpeg_Last;
extern QueueHandle_t xQueueEdge_Last;
extern QueueHandle_t xQueueProf_Last;

extern SemaphoreHandle_t mutex_Ethernet;
extern SemaphoreHandle_t mutex_FTP;


// extern portMUX_TYPE mutex_Ethernet;
bool xSemaphoreTakeRetry(SemaphoreHandle_t mutex, int retrySec);

#endif