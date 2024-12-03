#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"
#include "M5PoECAM.h"

#ifndef MAIN_BASE_FUNCTION_H
#define MAIN_BASE_FUNCTION_H

#define FLASH_EN_PIN 25

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

#endif