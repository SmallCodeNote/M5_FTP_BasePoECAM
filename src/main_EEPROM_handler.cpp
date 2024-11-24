#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>
#include "main_EEPROM_handler.h"

/// @brief UnitProfile
DATA_SET storeData;
String ftpSaveInterval = "0";
String chartShowPointCount = "60";
String chartUpdateInterval = "1000";

String timeZoneOffset = "9";
String flashIntensityMode = "6";
String flashLength = "60";

String ftp_user = "ftpusr";
String ftp_pass = "ftpword";

String deviceName = "Device";
String deviceIP_String = "";
String ftpSrvIP_String = "";
String ntpSrvIP_String = "";

void InitEEPROM()
{
    deviceName = "PoECAM-W V1.1";
    deviceIP_String = "192.168.25.177";
    ftpSrvIP_String = "192.168.25.74";
    ntpSrvIP_String = "192.168.25.74";
    ftp_user = "ftpusr";
    ftp_pass = "ftpword";
    ftpSaveInterval = "0";
    chartShowPointCount = "60";
    chartUpdateInterval = "1000";

    timeZoneOffset = "9";
    flashIntensityMode = "6";
    flashLength = "60";

    SetStoreDataFromStrings();
    storeData.romCheckCode = EEPROM_CHECK_CODE;

    EEPROM.put<DATA_SET>(0, storeData);
    EEPROM.commit();
}

void LoadEEPROM()
{
    EEPROM.get<DATA_SET>(0, storeData);
    if (storeData.romCheckCode != EEPROM_CHECK_CODE)
    {
        M5_LOGD("storeData.romCheckCode = %x", storeData.romCheckCode);
        InitEEPROM();
    }
    else
    {
        M5_LOGV("storeData.romCheckCode = %x", storeData.romCheckCode);
    }

    SetStringsFromStoreData();
}

void PutEEPROM()
{
    M5_LOGI("PutEEPROM: romCheckCode = %x", (storeData.romCheckCode));
    M5_LOGI("PutEEPROM: deviceName = %s, %s", deviceName.c_str(), (storeData.deviceName));
    M5_LOGI("PutEEPROM: ftpSaveInterval = %s, %u", ftpSaveInterval.c_str(), (storeData.ftpSaveInterval));
    M5_LOGI("PutEEPROM: chartUpdateInterval = %s, %u", chartUpdateInterval.c_str(), (storeData.chartUpdateInterval));
    M5_LOGI("PutEEPROM: chartShowPointCount = %s, %u", chartShowPointCount.c_str(), (storeData.chartShowPointCount));
    M5_LOGI("PutEEPROM: timeZoneOffset = %s, %d", timeZoneOffset.c_str(), (storeData.timeZoneOffset));
    M5_LOGI("PutEEPROM: flashIntensityMode = %s, %u", flashIntensityMode.c_str(), (storeData.flashIntensityMode));
    M5_LOGI("PutEEPROM: flashLength = %s, %u", flashLength.c_str(), (storeData.flashLength));

    SetStoreDataFromStrings();
    storeData.romCheckCode = EEPROM_CHECK_CODE;

    EEPROM.put<DATA_SET>(0, storeData);
    EEPROM.commit();
}

void SetStringsFromStoreData()
{
    deviceName = storeData.deviceName;
    deviceIP_String = storeData.deviceIP.toString();
    ftpSrvIP_String = storeData.ftpSrvIP.toString();
    ntpSrvIP_String = storeData.ntpSrvIP.toString();
    ftp_user = storeData.ftp_user;
    ftp_pass = storeData.ftp_pass;
    ftpSaveInterval = String(storeData.ftpSaveInterval);
    chartUpdateInterval = String(storeData.chartUpdateInterval);
    chartShowPointCount = String(storeData.chartShowPointCount);
    timeZoneOffset = String(storeData.timeZoneOffset);
    flashIntensityMode = String(storeData.flashIntensityMode);
    flashLength = String(storeData.flashLength);
}

void SetStoreDataFromStrings()
{
    M5_LOGI("deviceName.length = %u", deviceName.length());

    if (deviceName.length() > STORE_DATA_DEVICENAME_MAXLENGTH)
    {
        deviceName = deviceName.substring(0, STORE_DATA_DEVICENAME_MAXLENGTH - 1);
    }

    strcpy(storeData.deviceName, deviceName.c_str());
    storeData.deviceIP.fromString(deviceIP_String);
    storeData.ntpSrvIP.fromString(ntpSrvIP_String);
    storeData.ftpSrvIP.fromString(ftpSrvIP_String);
    strcpy(storeData.ftp_user, ftp_user.c_str());
    strcpy(storeData.ftp_pass, ftp_pass.c_str());
    storeData.ftpSaveInterval = (u_int16_t)(ftpSaveInterval.toInt());
    storeData.chartUpdateInterval = (u_int16_t)(chartUpdateInterval.toInt());
    storeData.chartShowPointCount = (u_int16_t)(chartShowPointCount.toInt());
    storeData.timeZoneOffset = (int8_t)(timeZoneOffset.toInt());
    storeData.flashIntensityMode = (int8_t)(flashIntensityMode.toInt());
    storeData.flashLength = (int8_t)(flashLength.toInt());
}
