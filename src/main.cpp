#include <M5Unified.h>
#include <SPI.h>
// #include <Wire.h>
#include <M5_Ethernet.h>
// #include <time.h>
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
  /*
    PoECAM.Camera.sensor->set_pixformat(PoECAM.Camera.sensor, PIXFORMAT_JPEG);
    PoECAM.Camera.sensor->set_framesize(PoECAM.Camera.sensor, FRAMESIZE_QVGA);
    PoECAM.Camera.sensor->set_vflip(PoECAM.Camera.sensor, 1);
    PoECAM.Camera.sensor->set_hmirror(PoECAM.Camera.sensor, 0);
    PoECAM.Camera.sensor->set_gain_ctrl(PoECAM.Camera.sensor, 0);

    PoECAM.Camera.sensor->set_exposure_ctrl(PoECAM.Camera.sensor, 0);
    PoECAM.Camera.sensor->set_denoise(PoECAM.Camera.sensor, 1);
  */

  M5_LOGI("Camera Init Success");
  return UnitEnable;
}

void CameraSensorSetup()
{

  PoECAM.Camera.sensor->set_pixformat(PoECAM.Camera.sensor, storeData.pixformat);
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

  M5_LOGI("ESP_MAC_WIFI_STA  %s", getInterfaceMacAddress(ESP_MAC_WIFI_STA).c_str());
  M5_LOGI("ESP_MAC_WIFI_SOFTAP  %s", getInterfaceMacAddress(ESP_MAC_WIFI_SOFTAP).c_str());
  M5_LOGI("ESP_MAC_BT  %s", getInterfaceMacAddress(ESP_MAC_BT).c_str());
  M5_LOGI("ESP_MAC_ETH  %s", getInterfaceMacAddress(ESP_MAC_ETH).c_str());

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

  CameraSensorSetup();

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
  xTaskCreatePinnedToCore(ShotLoop, "ShotLoop", 4096, NULL, 1, NULL, 1);

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

  if(loopCount>=100){
    M5_LOGI("Call HTTPUI();");
    loopCount=0;
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
