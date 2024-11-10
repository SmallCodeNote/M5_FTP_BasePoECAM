#include <M5_Ethernet.h>
#include <EEPROM.h>

#ifndef MAIN_EEPROM_HANDLER_H
#define MAIN_EEPROM_HANDLER_H

#define STORE_DATA_SIZE 256 // byte
#define EEPROM_CHECK_CODE 0x42

IPAddress deviceIP(192, 168, 25, 177);

/// @brief Encorder Profile Struct
struct DATA_SET
{
    uint8_t romCheckCode;

    /// @brief IP address
    IPAddress deviceIP;
    IPAddress ftpSrvIP;
    IPAddress ntpSrvIP;

    u_int16_t interval;

    /// @brief deviceName
    char deviceName[32];
    char ftp_user[32];
    char ftp_pass[32];
};

String ftp_address = "192.168.25.77";
String ftp_user = "ftpusr";
String ftp_pass = "ftpword";
String ntp_address = "192.168.25.77";

/// @brief Encorder Profile
DATA_SET storeData;
String shotInterval;

String deviceName = "Device";
String deviceIP_String = "";
String ftpSrvIP_String = "";
String ntpSrvIP_String = "";

void SetStringsFromStoreData();
void SetStoreDataFromStrings();

void InitEEPROM()
{
    deviceName = "PoECAM-W";
    deviceIP_String = "192.168.25.177";
    ftpSrvIP_String = "192.168.25.77";
    ntpSrvIP_String = "192.168.25.77";
    ftp_user = "ftpusr";
    ftp_pass = "ftpword";
    shotInterval = 10;

    SetStoreDataFromStrings();

    EEPROM.put<DATA_SET>(0, storeData);
    EEPROM.commit();
}

void LoadEEPROM()
{
    EEPROM.get<DATA_SET>(0, storeData);
    if (storeData.romCheckCode != EEPROM_CHECK_CODE)
    {
        InitEEPROM();
    }

    SetStringsFromStoreData();
    Serial.println("LoadEEPROM: interval = " + String(storeData.interval));
}

void PutEEPROM()
{
    SetStoreDataFromStrings();
    Serial.println("PutEEPROM: interval = " + String(storeData.interval));
    EEPROM.put<DATA_SET>(0, storeData);
    EEPROM.commit();
}

void SetStringsFromStoreData()
{
    deviceName = storeData.deviceName;
    deviceIP = storeData.deviceIP;
    deviceIP_String = storeData.deviceIP.toString();
    ftpSrvIP_String = storeData.ftpSrvIP.toString();
    ntpSrvIP_String = storeData.ntpSrvIP.toString();
    ftp_user = storeData.ftp_user;
    ftp_pass = storeData.ftp_pass;
    shotInterval = String(storeData.interval);
}

void SetStoreDataFromStrings()
{
    strcpy(storeData.deviceName, deviceName.c_str());
    storeData.deviceIP.fromString(deviceIP_String);
    storeData.ntpSrvIP.fromString(ntpSrvIP_String);
    storeData.ftpSrvIP.fromString(ftpSrvIP_String);
    strcpy(storeData.ftp_user, ftp_user.c_str());
    strcpy(storeData.ftp_pass, ftp_pass.c_str());
    storeData.interval = (u_int16_t)(shotInterval.toInt());
}

#endif