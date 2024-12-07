#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>

#ifndef MAIN_EEPROM_HANDLER_H
#define MAIN_EEPROM_HANDLER_H

#define STORE_DATA_SIZE 512                // byte
#define STORE_DATA_DEVICENAME_MAXLENGTH 31 // byte
#define EEPROM_CHECK_CODE 0x58

/// @brief Encorder Profile Struct
struct DATA_SET
{
    uint8_t romCheckCode;

    /// @brief IP address
    IPAddress deviceIP;
    IPAddress ftpSrvIP;
    IPAddress ntpSrvIP;

    u_int16_t ftpImageSaveInterval;
    u_int16_t ftpEdgeSaveInterval;
    u_int16_t ftpProfileSaveInterval;

    u_int16_t imageBufferingInterval;

    u_int16_t chartUpdateInterval;//[ms]
    u_int16_t chartShowPointCount;
    int8_t timeZoneOffset;
    u_int8_t flashIntensityMode;
    u_int16_t flashLength; //[ms]

    uint8_t pixLineStep;
    uint8_t pixLineRange;

    uint8_t pixLineEdgeSearchStart; //[0-100%]
    uint8_t pixLineEdgeSearchEnd;   // [0-100%]
    uint8_t pixLineEdgeUp;          // 1: dark -> light / 2: light -> dark
    uint16_t pixLineThrethold;      //[0-765]

    // Sensor values
    pixformat_t pixformat;
    framesize_t framesize;
    int contrast;
    int brightness;
    int saturation;
    int sharpness;
    int denoise;
    gainceiling_t gainceiling;
    int quality;
    int colorbar;
    int whitebal;
    int gain_ctrl;
    int exposure_ctrl;
    int hmirror;
    int vflip;

    int aec2;
    int awb_gain;
    int agc_gain;
    int aec_value;

    int special_effect;
    int wb_mode;
    int ae_level;
    /*
        int dcw;
        int bpc;
        int wpc;

        int raw_gma;
        int lenc;

        int get_reg;
        int set_reg;
        int set_res_raw;
        int set_pll;
        int set_xclk;
    */

    /// @brief deviceName
    char deviceName[STORE_DATA_DEVICENAME_MAXLENGTH + 1];
    char ftp_user[32];
    char ftp_pass[32];
};

extern String ftp_user;
extern String ftp_pass;

/// @brief Encorder Profile
extern DATA_SET storeData;
extern String ftpSaveInterval;

extern String ftpImageSaveInterval;
extern String ftpEdgeSaveInterval;
extern String ftpProfileSaveInterval;

extern String imageBufferingInterval;


extern String chartUpdateInterval;
extern String chartShowPointCount;

extern String timeZoneOffset;

extern String flashIntensityMode;
extern String flashLength;

extern String pixLineStep;
extern String pixLineRange;

extern String pixLineEdgeSearchStart; //[0-100%]
extern String pixLineEdgeSearchEnd;   // [0-100%]
extern String pixLineEdgeUp;          // 1: dark -> light / 2: light -> dark
extern String pixLineThrethold;       //[0-765]

extern String pixformat;
extern String framesize;
extern String contrast;
extern String brightness;
extern String saturation;
extern String sharpness;
extern String denoise;
extern String gainceiling;
extern String quality;
extern String colorbar;
extern String whitebal;
extern String gain_ctrl;
extern String exposure_ctrl;
extern String hmirror;
extern String vflip;

extern String aec2;
extern String awb_gain;
extern String agc_gain;
extern String aec_value;

extern String special_effect;
extern String wb_mode;
extern String ae_level;
/*
extern String dcw;
extern String bpc;
extern String wpc;

extern String raw_gma;
extern String lenc;

extern String get_reg;
extern String set_reg;
extern String set_res_raw;
extern String set_pll;
extern String set_xclk;
*/

extern String deviceName;
extern String deviceIP_String;
extern String ftpSrvIP_String;
extern String ntpSrvIP_String;

void InitEEPROM();
void LoadEEPROM();
void PutEEPROM();
void SetStringsFromStoreData();
void SetStoreDataFromStrings();

#endif
