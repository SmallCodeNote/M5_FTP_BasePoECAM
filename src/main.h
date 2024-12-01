#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"
#include "M5PoECAM.h"

#ifndef MAIN_BASE_FUNCTION_H
#define MAIN_BASE_FUNCTION_H

#define FLASH_EN_PIN 25


void CameraSensorFullSetupFromStoreData();
void CameraSensorSetJPEG();
void CameraSensorSetGRAYSCALE();

void unit_flash_set_brightness(uint8_t brightness);

uint16_t CameraSensorFrameWidth(framesize_t framesize);
uint16_t CameraSensorFrameHeight(framesize_t framesize);


extern M5_Ethernet_FtpClient ftp;
extern M5_Ethernet_NtpClient NtpClient;

#endif