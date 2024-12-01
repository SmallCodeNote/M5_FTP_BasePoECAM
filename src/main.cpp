#include <M5Unified.h>
#include <SPI.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>
#include "M5PoECAM.h"
#include "M5_Ethernet_FtpClient.h"
#include "M5_Ethernet_NtpClient.h"
#include "M5_GetBoardName.h"

#include "main.h"
#include "main_EEPROM_handler.h"
#include "main_HTTP_UI.h"
#include "main_Loop.h"

#include "esp_mac.h"

// == M5AtomS3R_Bus ==
/*#define SCK 5
#define MISO 7
#define MOSI 8
#define CS 6*/

// == M5PoECAM_Bus ==
#define SCK 23
#define MISO 38
#define MOSI 13
#define CS 4

void TimeUpdateLoop(void *arg);
void TimeServerAccessLoop(void *arg);
void ButtonKeepCountLoop(void *arg);
void ShotLoop(void *arg);
void ShotTask(void *arg);

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

EthernetClient FtpClient(21);
M5_Ethernet_FtpClient ftp(ftpSrvIP_String, ftp_user, ftp_pass, 60000);
M5_Ethernet_NtpClient NtpClient;

bool UnitEnable = true;

String getInterfaceMacAddress(esp_mac_type_t interface)
{

  String macString = "";
  unsigned char mac_base[6] = {0};
  if (esp_read_mac(mac_base, interface) == ESP_OK)
  {
    char buffer[18]; // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    macString = buffer;
  }

  return macString;
}

bool PoECamUnitBegin()
{
  PoECAM.begin();
  if (!PoECAM.Camera.begin())
  {
    M5_LOGW("Camera Init Fail");
    UnitEnable = false;
    return false;
  }
  M5_LOGI("Camera Init Success");
  return UnitEnable;
}

uint16_t CameraSensorFrameWidth(framesize_t framesize)
{
  switch (framesize)
  {
  case FRAMESIZE_96X96: // 96x96 pixels
    return 96u;
  case FRAMESIZE_QQVGA: // 160x120 pixels
    return 160u;
  case FRAMESIZE_QCIF: // 176x144 pixels
    return 176u;
  case FRAMESIZE_HQVGA: // 240x176 pixels
    return 240u;
  case FRAMESIZE_240X240: // 240x240 pixels
    return 240u;
  case FRAMESIZE_QVGA: // 320x240 pixels
    return 320u;
  case FRAMESIZE_CIF: // 400x296 pixels
    return 400u;
  case FRAMESIZE_HVGA: // 480x320 pixels
    return 480u;
  case FRAMESIZE_VGA: // 640x480 pixels
    return 640u;
  case FRAMESIZE_SVGA: // 800x600 pixels
    return 800u;
  case FRAMESIZE_XGA: // 1024x768 pixels
    return 1024u;
  case FRAMESIZE_HD: // 1280x720 pixels
    return 1280u;
  case FRAMESIZE_SXGA: // 1280x1024 pixels
    return 1280u;
  case FRAMESIZE_UXGA: // 1600x1200 pixels
    return 1600u;
  case FRAMESIZE_FHD: // 1920x1080 pixels
    return 1920u;
  case FRAMESIZE_P_HD: // 720x1280 pixels
    return 720u;
  case FRAMESIZE_P_3MP: // 864x1536 pixels
    return 864u;
  case FRAMESIZE_QXGA: // 2048x1536 pixels
    return 2048u;
  case FRAMESIZE_QHD: // 2560x1440 pixels
    return 2560u;
  case FRAMESIZE_WQXGA: // 2560x1600 pixels
    return 2560u;
  case FRAMESIZE_P_FHD: // 1080x1920 pixels
    return 1080u;
  case FRAMESIZE_QSXGA: // 2560x1920 pixels
    return 2560u;
  default: // Invalid framesize
    return 0u;
  }
}

uint16_t CameraSensorFrameHeight(framesize_t framesize)
{
  switch (framesize)
  {
  case FRAMESIZE_96X96: // 96x96 pixels
    return 96u;
  case FRAMESIZE_QQVGA: // 160x120 pixels
    return 120u;
  case FRAMESIZE_QCIF: // 176x144 pixels
    return 144u;
  case FRAMESIZE_HQVGA: // 240x176 pixels
    return 176u;
  case FRAMESIZE_240X240: // 240x240 pixels
    return 240u;
  case FRAMESIZE_QVGA: // 320x240 pixels
    return 240u;
  case FRAMESIZE_CIF: // 400x296 pixels
    return 296u;
  case FRAMESIZE_HVGA: // 480x320 pixels
    return 320u;
  case FRAMESIZE_VGA: // 640x480 pixels
    return 480u;
  case FRAMESIZE_SVGA: // 800x600 pixels
    return 600u;
  case FRAMESIZE_XGA: // 1024x768 pixels
    return 768u;
  case FRAMESIZE_HD: // 1280x720 pixels
    return 720u;
  case FRAMESIZE_SXGA: // 1280x1024 pixels
    return 1024u;
  case FRAMESIZE_UXGA: // 1600x1200 pixels
    return 1200u;
  case FRAMESIZE_FHD: // 1920x1080 pixels
    return 1080u;
  case FRAMESIZE_P_HD: // 720x1280 pixels
    return 1280u;
  case FRAMESIZE_P_3MP: // 864x1536 pixels
    return 1536u;
  case FRAMESIZE_QXGA: // 2048x1536 pixels
    return 1536u;
  case FRAMESIZE_QHD: // 2560x1440 pixels
    return 1440u;
  case FRAMESIZE_WQXGA: // 2560x1600 pixels
    return 1600u;
  case FRAMESIZE_P_FHD: // 1080x1920 pixels
    return 1920u;
  case FRAMESIZE_QSXGA: // 2560x1920 pixels
    return 1920u;
  default: // Invalid framesize
    return 0u;
  }
}

void CameraSensorFullSetupFromStoreData()
{
  PoECAM.Camera.sensor->set_pixformat(PoECAM.Camera.sensor, PIXFORMAT_JPEG);

  PoECAM.Camera.sensor->set_framesize(PoECAM.Camera.sensor, storeData.framesize);
  PoECAM.Camera.sensor->set_contrast(PoECAM.Camera.sensor, storeData.contrast);
  PoECAM.Camera.sensor->set_brightness(PoECAM.Camera.sensor, storeData.brightness);
  PoECAM.Camera.sensor->set_saturation(PoECAM.Camera.sensor, storeData.saturation);
  PoECAM.Camera.sensor->set_sharpness(PoECAM.Camera.sensor, storeData.sharpness);
  PoECAM.Camera.sensor->set_denoise(PoECAM.Camera.sensor, storeData.denoise);
  PoECAM.Camera.sensor->set_quality(PoECAM.Camera.sensor, storeData.quality);
  PoECAM.Camera.sensor->set_colorbar(PoECAM.Camera.sensor, storeData.colorbar);
  PoECAM.Camera.sensor->set_whitebal(PoECAM.Camera.sensor, storeData.whitebal);
  PoECAM.Camera.sensor->set_gain_ctrl(PoECAM.Camera.sensor, storeData.gain_ctrl);
  PoECAM.Camera.sensor->set_exposure_ctrl(PoECAM.Camera.sensor, storeData.exposure_ctrl);
  PoECAM.Camera.sensor->set_hmirror(PoECAM.Camera.sensor, storeData.hmirror);
  PoECAM.Camera.sensor->set_vflip(PoECAM.Camera.sensor, storeData.vflip);
  PoECAM.Camera.sensor->set_aec2(PoECAM.Camera.sensor, storeData.aec2);
  PoECAM.Camera.sensor->set_awb_gain(PoECAM.Camera.sensor, storeData.awb_gain);
  PoECAM.Camera.sensor->set_agc_gain(PoECAM.Camera.sensor, storeData.agc_gain);
  PoECAM.Camera.sensor->set_aec_value(PoECAM.Camera.sensor, storeData.aec_value);
  PoECAM.Camera.sensor->set_special_effect(PoECAM.Camera.sensor, storeData.special_effect);
  PoECAM.Camera.sensor->set_wb_mode(PoECAM.Camera.sensor, storeData.wb_mode);
  PoECAM.Camera.sensor->set_ae_level(PoECAM.Camera.sensor, storeData.ae_level);
}

bool EthernetBegin()
{
  // CONFIG_IDF_TARGET_ESP32S3;
  // CONFIG_IDF_TARGET_ESP32;

  M5_LOGI("MAX_SOCK_NUM = %s", String(MAX_SOCK_NUM));
  if (MAX_SOCK_NUM < 8)
  {
    M5_LOGE("need overwrite MAX_SOCK_NUM = 8 in M5_Ethernet.h");
    UnitEnable = false;
    return false;
  }
  SPI.begin(SCK, MISO, MOSI, -1);
  Ethernet.init(CS);
  esp_read_mac(mac, ESP_MAC_ETH);
  Ethernet.begin(mac, storeData.deviceIP);

  M5_LOGI("Ethernet.begin  %s", deviceIP_String.c_str());

  return UnitEnable;
}

void updateFTP_ParameterFromGrobalStrings()
{
  M5_LOGD("ftpSrvIP_String: %s", ftpSrvIP_String);
  ftp.SetServerAddress(ftpSrvIP_String);
  ftp.SetUserName(ftp_user);
  ftp.SetPassWord(ftp_pass);

  M5_LOGD("GetServerAddress: %s", ftp.GetServerAddress());
}

void unit_flash_init(void)
{
  pinMode(FLASH_EN_PIN, OUTPUT);
  digitalWrite(FLASH_EN_PIN, LOW);
}

void setup()
{
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
  delay(500); // M5_Log starting wait

  M5_LOGI("PoECamUnitBegin");
  if (!PoECamUnitBegin())
    return;

  // auto cfg = M5.config();
  // M5.begin(cfg);

  M5_LOGI("BoardName = %s", getBoardName(M5.getBoard()));

  EEPROM.begin(STORE_DATA_SIZE);
  LoadEEPROM();

  CameraSensorFullSetupFromStoreData();

  updateFTP_ParameterFromGrobalStrings();

  M5_LOGI("deviceIP_String = %s", deviceIP_String.c_str());
  M5_LOGI("DisplayCount = %d", M5.getDisplayCount());

  if (!EthernetBegin())
    return;

  HttpUIServer.begin();
  NtpClient.begin();
  NtpClient.getTime(ntpSrvIP_String, (int)(storeData.timeZoneOffset));

  unit_flash_init();

  xTaskCreatePinnedToCore(TimeUpdateLoop, "TimeUpdateLoop", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TimeServerAccessLoop, "TimeServerAccessLoop", 4096, NULL, 0, NULL, 0);
  xTaskCreatePinnedToCore(ButtonKeepCountLoop, "ButtonKeepCountLoop", 4096, NULL, 0, NULL, 1);
  xTaskCreatePinnedToCore(SensorShotLoop, "SensorShotLoop", 4096, NULL, 1, NULL, 1);

  if (M5.getDisplayCount() > 0)
  {
    M5.Display.println(String(storeData.deviceName));
    M5.Display.println("Board: " + getBoardName(M5.getBoard()));
    M5.Display.println("HOST: " + deviceIP_String);
  }
}

int loopCount = 1000;
void loop()
{
  delay(100);

  if (loopCount >= 100)
  {
    M5_LOGI("Call HTTPUI();");
    loopCount = 0;
  }
  loopCount++;
  HTTP_UI();

  if (M5.getDisplayCount() > 0)
  {
    M5.Display.setTextFont(6);
    M5.Display.setCursor(8, 30);
    M5.Display.println(SensorValueString);

    M5.Display.setTextFont(1);
    M5.Display.println("  " + getInterfaceMacAddress(ESP_MAC_WIFI_STA));
    M5.Display.println("  " + getInterfaceMacAddress(ESP_MAC_WIFI_SOFTAP));
    M5.Display.println("  " + getInterfaceMacAddress(ESP_MAC_BT));
    M5.Display.println("  " + getInterfaceMacAddress(ESP_MAC_ETH));

    String ntpAddressLine = "NTP:" + ntpSrvIP_String;
    for (u_int8_t i = 0; i < (21 - ntpAddressLine.length()) / 2; i++)
    {
      M5.Display.print(" ");
    }

    M5.Display.println(ntpAddressLine);

    if (NtpClient.enable())
    {
      M5.Display.println(" " + NtpClient.convertTimeEpochToString() + "   ");
    }
    else
    {
      M5.Display.println("time information can not use now.     ");
    }
  }
  Ethernet.maintain();
}
