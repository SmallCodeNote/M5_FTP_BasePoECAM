#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <EEPROM.h>

#include "M5PoECAM.h"
#include "main_EEPROM_handler.h"

/// @brief UnitProfile
DATA_SET storeData;
String ftpSaveInterval = "0";
String chartShowPointCount = "60";
String chartUpdateInterval = "1000";

String timeZoneOffset = "9";

String flashIntensityMode = "6";
String flashLength = "60";

String pixformat = "4"; // 0:RGB565, 1:YUV422, 2:YUV420, 3:GRAYSCALE, 4:JPEG, 5:RGB888, 6:RAW, 7:RGB444, 8:RGB555
String framesize = "5"; // 0:96X96(96x96), 1:QQVGA(160x120), 2:QCIF(176x144), 3:HQVGA(240x176), 4:240X240(240x240), 5:QVGA(320x240), 6:CIF(400x296), 7:HVGA(480x320), 8:VGA(640x480), 9:SVGA(800x600), 10:XGA(1024x768), 11:HD(1280x720), 12:SXGA(1280x1024), 13:UXGA(1600x1200), 14:FHD(1920x1080), 15:P_HD(720x1280), 16:P_3MP(864x1536), 17:QXGA(2048x1536), 18:QHD(2560x1440), 19:WQXGA(2560x1600), 20:P_FHD(1080x1920), 21:QSXGA(2560x1920)

String contrast = "0";      // Contrast [-2 - 2]
String brightness = "0";    // Brightness [-2 - 2]
String saturation = "0";    // Saturation [-2 - 2]
String sharpness = "0";     // Sharpness [-2 - 2]
String denoise = "1";       // Denoise level
String gainceiling = "0";   // Initial value [0 - 6]
String quality = "10";      // Quality initial value [0 - 63]
String colorbar = "0";      // Color bar disabled|enabled [0 | 1]
String whitebal = "1";      // White balance initial value [0 | 1]
String gain_ctrl = "1";     // Gain control disabled|enabled [0 | 1]
String exposure_ctrl = "1"; // Exposure control disabled|enabled [0 | 1]
String hmirror = "0";       // Horizontal flip [0 | 1]
String vflip = "1";         // Vertical flip [0 | 1]

String aec2 = "1";      // Automatic exposure control 2 disabled|enabled [0 | 1]
String awb_gain = "1";  // Automatic white balance gain disabled|enabled [0 | 1]
String agc_gain = "0";  // Automatic gain control [0 - 30]
String aec_value = "0"; // Automatic exposure control [0 - 1200]

String special_effect = "0"; // Special effect [0 - 6]
String wb_mode = "0";        // White balance mode [0 - 4]
String ae_level = "0";       // Automatic exposure level [-2 - 2]

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

    pixformat = "4"; // 4:JPEG
    framesize = "5"; // 5:QVGA(320x240)

    contrast = "0";      // Contrast [-2 - 2]
    brightness = "0";    // Brightness [-2 - 2]
    saturation = "0";    // Saturation [-2 - 2]
    sharpness = "0";     // Sharpness [-2 - 2]
    denoise = "1";       // Denoise level
    gainceiling = "0";   // Initial value [0 - 6]
    quality = "10";      // Quality initial value [0 - 63]
    colorbar = "0";      // Color bar disabled|enabled [0 | 1]
    whitebal = "1";      // White balance initial value [0 | 1]
    gain_ctrl = "1";     // Gain control disabled|enabled [0 | 1]
    exposure_ctrl = "1"; // Exposure control disabled|enabled [0 | 1]
    hmirror = "0";       // Horizontal flip [0 | 1]
    vflip = "1";         // Vertical flip [0 | 1]

    aec2 = "0";      // Automatic exposure control 2 disabled|enabled [0 | 1]
    awb_gain = "0";  // Automatic white balance gain disabled|enabled [0 | 1]
    agc_gain = "0";  // Automatic gain control [0 - 30]
    aec_value = "0"; // Automatic exposure control [0 - 1200]

    special_effect = "0"; // Special effect [0 - 6]
    wb_mode = "0";        // White balance mode [0 - 4]
    ae_level = "0";       // Automatic exposure level [-2 - 2]

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

    M5_LOGI("PutEEPROM: pixformat = %s, %u", pixformat.c_str(), (storeData.pixformat));
    M5_LOGI("PutEEPROM: framesize = %s, %u", framesize.c_str(), (storeData.framesize));
    M5_LOGI("PutEEPROM: contrast = %s, %d", contrast.c_str(), (storeData.contrast));
    M5_LOGI("PutEEPROM: brightness = %s, %d", brightness.c_str(), (storeData.brightness));
    M5_LOGI("PutEEPROM: saturation = %s, %d", saturation.c_str(), (storeData.saturation));
    M5_LOGI("PutEEPROM: sharpness = %s, %d", sharpness.c_str(), (storeData.sharpness));
    M5_LOGI("PutEEPROM: denoise = %s, %d", denoise.c_str(), (storeData.denoise));
    M5_LOGI("PutEEPROM: gainceiling = %s, %u", gainceiling.c_str(), (storeData.gainceiling));
    M5_LOGI("PutEEPROM: quality = %s, %d", quality.c_str(), (storeData.quality));
    M5_LOGI("PutEEPROM: colorbar = %s, %d", colorbar.c_str(), (storeData.colorbar));
    M5_LOGI("PutEEPROM: whitebal = %s, %d", whitebal.c_str(), (storeData.whitebal));
    M5_LOGI("PutEEPROM: gain_ctrl = %s, %d", gain_ctrl.c_str(), (storeData.gain_ctrl));
    M5_LOGI("PutEEPROM: exposure_ctrl = %s, %d", exposure_ctrl.c_str(), (storeData.exposure_ctrl));
    M5_LOGI("PutEEPROM: hmirror = %s, %d", hmirror.c_str(), (storeData.hmirror));
    M5_LOGI("PutEEPROM: vflip = %s, %d", vflip.c_str(), (storeData.vflip));

    M5_LOGI("PutEEPROM: aec2 = %s, %d", aec2.c_str(), (storeData.aec2));
    M5_LOGI("PutEEPROM: awb_gain = %s, %d", awb_gain.c_str(), (storeData.awb_gain));
    M5_LOGI("PutEEPROM: agc_gain = %s, %d", agc_gain.c_str(), (storeData.agc_gain));
    M5_LOGI("PutEEPROM: aec_value = %s, %d", aec_value.c_str(), (storeData.aec_value));

    M5_LOGI("PutEEPROM: special_effect = %s, %d", special_effect.c_str(), (storeData.special_effect));
    M5_LOGI("PutEEPROM: wb_mode = %s, %d", wb_mode.c_str(), (storeData.wb_mode));
    M5_LOGI("PutEEPROM: ae_level = %s, %d", ae_level.c_str(), (storeData.ae_level));

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

    pixformat = String(storeData.pixformat);
    framesize = String(storeData.framesize);
    contrast = String(storeData.contrast);
    brightness = String(storeData.brightness);
    saturation = String(storeData.saturation);
    sharpness = String(storeData.sharpness);
    denoise = String(storeData.denoise);
    gainceiling = String(storeData.gainceiling);
    quality = String(storeData.quality);
    colorbar = String(storeData.colorbar);
    whitebal = String(storeData.whitebal);
    gain_ctrl = String(storeData.gain_ctrl);
    exposure_ctrl = String(storeData.exposure_ctrl);
    hmirror = String(storeData.hmirror);
    vflip = String(storeData.vflip);

    aec2 = String(storeData.aec2);
    awb_gain = String(storeData.awb_gain);
    agc_gain = String(storeData.agc_gain);
    aec_value = String(storeData.aec_value);

    special_effect = String(storeData.special_effect);
    wb_mode = String(storeData.wb_mode);
    ae_level = String(storeData.ae_level);
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

    storeData.pixformat = static_cast<pixformat_t>(pixformat.toInt());
    storeData.framesize = static_cast<framesize_t>(framesize.toInt());
    storeData.contrast = contrast.toInt();

    storeData.pixformat = static_cast<pixformat_t>(pixformat.toInt());
    storeData.framesize = static_cast<framesize_t>(framesize.toInt());
    storeData.contrast = contrast.toInt();
    storeData.brightness = brightness.toInt();
    storeData.saturation = saturation.toInt();
    storeData.sharpness = sharpness.toInt();
    storeData.denoise = denoise.toInt();
    storeData.gainceiling = static_cast<gainceiling_t>(gainceiling.toInt());
    storeData.quality = quality.toInt();
    storeData.colorbar = colorbar.toInt();
    storeData.whitebal = whitebal.toInt();
    storeData.gain_ctrl = gain_ctrl.toInt();
    storeData.exposure_ctrl = exposure_ctrl.toInt();
    storeData.hmirror = hmirror.toInt();
    storeData.vflip = vflip.toInt();

    storeData.aec2 = aec2.toInt();
    storeData.awb_gain = awb_gain.toInt();
    storeData.agc_gain = agc_gain.toInt();
    storeData.aec_value = aec_value.toInt();

    storeData.special_effect = special_effect.toInt();
    storeData.wb_mode = wb_mode.toInt();
    storeData.ae_level = ae_level.toInt();
}
