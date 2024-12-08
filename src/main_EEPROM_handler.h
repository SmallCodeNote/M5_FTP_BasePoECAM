#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>

#ifndef MAIN_EEPROM_HANDLER_H
#define MAIN_EEPROM_HANDLER_H

#define STORE_DATA_SIZE 512                // byte
#define STORE_DATA_DEVICENAME_MAXLENGTH 31 // byte
#define EEPROM_CHECK_CODE 0x60

/// @brief Encorder Profile Struct
struct DATA_SET
{
    uint8_t romCheckCode;

    /// @brief IP address
    IPAddress deviceIP;
    IPAddress ftpSrvIP;
    IPAddress ntpSrvIP;

u_int16_t ftpImageSaveInterval; // Interval for saving images to FTP [0 - 65535]
u_int16_t ftpEdgeSaveInterval;  // Interval for saving edge data to FTP [0 - 65535]
u_int16_t ftpProfileSaveInterval; // Interval for saving profile data to FTP [0 - 65535]

u_int16_t imageBufferingEpochInterval; //[1- (sec.)]

u_int16_t chartUpdateInterval; //[ms]
u_int16_t chartShowPointCount; // Number of points to show on the chart [0 - 65535]
int8_t timeZoneOffset; // Time zone offset in hours [-12 - 14]
u_int8_t flashIntensityMode; // Flash intensity mode [0 - 255]
u_int16_t flashLength; //[1- (ms)]

uint8_t pixLineStep;      //[0-255 (px)]
uint8_t pixLineRange;     //[0-100%]
uint8_t pixLineAngle;     //[0-90(deg) 0:horizontal 90:vertical]
uint8_t pixLineShiftUp;   //[0-255 (px)]
uint8_t pixLineSideWidth; //[0-255 (px) 0:LineWidth->1,1->3,2->5]

uint8_t pixLineEdgeSearchStart; //[0-100%]
uint8_t pixLineEdgeSearchEnd;   // [0-100%]
uint8_t pixLineEdgeUp;          // 1: dark -> light / 2: light -> dark
uint16_t pixLineThrethold;      //[0-765]

// Sensor values
pixformat_t pixformat; // Pixel format type
framesize_t framesize; // Frame size type
int contrast;          // Contrast level [-2 - 2]
int brightness;        // Brightness level [-2 - 2]
int saturation;        // Saturation level [-2 - 2]
int sharpness;         // Sharpness level [-2 - 2]
int denoise;           // Denoise level [0 - 1]
gainceiling_t gainceiling; // Gain ceiling level [0 - 6]
int quality;           // Image quality [0 - 63]
int colorbar;          // Color bar enabled/disabled [0 | 1]
int whitebal;          // White balance enabled/disabled [0 | 1]
int gain_ctrl;         // Gain control enabled/disabled [0 | 1]
int exposure_ctrl;     // Exposure control enabled/disabled [0 | 1]
int hmirror;           // Horizontal mirror enabled/disabled [0 | 1]
int vflip;             // Vertical flip enabled/disabled [0 | 1]

int aec2;              // Automatic exposure control 2 enabled/disabled [0 | 1]
int awb_gain;          // Automatic white balance gain enabled/disabled [0 | 1]
int agc_gain;          // Automatic gain control [0 - 30]
int aec_value;         // Automatic exposure control value [0 - 1200]

int special_effect;    // Special effect type [0 - 6]
int wb_mode;           // White balance mode [0 - 4]
int ae_level;          // Automatic exposure level [-2 - 2]


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

extern String imageBufferingEpochInterval;

extern String chartUpdateInterval;
extern String chartShowPointCount;

extern String timeZoneOffset;

extern String flashIntensityMode;
extern String flashLength;

extern String pixLineStep;      //[0-255 (px)]
extern String pixLineRange;     //[0-100%]
extern String pixLineAngle;     //[0-90deg 0:horizontal 90:vertical]
extern String pixLineShiftUp;   //[0-255 (px)]
extern String pixLineSideWidth; //[0-255 (px) 0:LineWidth->1,1->3,2->5]

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
