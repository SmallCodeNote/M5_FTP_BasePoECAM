#include "M5_Ethernet_NtpClient.h"

void M5_Ethernet_NtpClient::begin()
{
    Udp.begin(localPort);
}

void M5_Ethernet_NtpClient::updateTimeFromServer(String address)
{
    return updateTimeFromServer(address, timezoneOffset);
}

void M5_Ethernet_NtpClient::updateTimeFromServer(String address, int timezone)
{
    timezoneOffset = timezone;

    sendNTPpacket(address.c_str());
    delay(50);
    if (Udp.parsePacket())
    {
        M5_LOGD("Udp parsePacket()");
        Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        const unsigned long seventyYears = 2208988800UL;
        unsigned long localEpoch = (secsSince1900 - seventyYears) + timezoneOffset * 3600;

        lastEpoch = localEpoch;
        lastMillis = millis();
        intMillis = millis();
        currentEpoch = lastEpoch;

        M5_LOGI("Success UpdateFromNTPserver %s , timezone = %d", address, timezone);
        return;
    }
    else
    {
        M5_LOGD("Udp can not parsePacket()");
        currentEpoch = millis() / 1000u;
    }
    M5_LOGI("False UpdateFromNTPserver %s , timezone = %d", address, timezone);
    intMillis = millis();
}

void M5_Ethernet_NtpClient::updateTimeFromString(String timeString)
{
    return updateTimeFromString(timeString, timezoneOffset);
}

void M5_Ethernet_NtpClient::updateTimeFromString(String timeString, int timezone)
{
    timezoneOffset = timezone;

    unsigned long localEpoch = convertTimeStringToEpoch(timeString);

    lastEpoch = localEpoch;
    lastMillis = millis();
    intMillis = millis();
    currentEpoch = lastEpoch;

    return;
}

unsigned long M5_Ethernet_NtpClient::convertTimeStringToEpoch(String timeString)
{
    M5_LOGD("timeString = %s", timeString.c_str());
    tmElements_t tm;
    int year, month, day, hour, minute, second;
    sscanf(timeString.c_str(), "%d/%d/%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    M5_LOGD("%d/%d/%d %d:%d:%d", year, month, day, hour, minute, second);

    tm.Year = year - 1970;
    tm.Month = month;
    tm.Day = day;
    tm.Hour = hour;
    tm.Minute = minute;
    tm.Second = second;
    return makeTime(tm);
}

String M5_Ethernet_NtpClient::convertTimeEpochToString()
{
    return convertTimeEpochToString("yyyy/mm/dd HH:mm:ss", currentEpoch);
}
String M5_Ethernet_NtpClient::convertTimeEpochToString(unsigned long _currentEpoch)
{
    return convertTimeEpochToString("yyyy/mm/dd HH:mm:ss", _currentEpoch);
}

String M5_Ethernet_NtpClient::convertTimeEpochToString(String format)
{
    return convertTimeEpochToString(format, currentEpoch);
}

String M5_Ethernet_NtpClient::convertTimeEpochToString(String format, unsigned long _currentEpoch)
{
    if (_currentEpoch == 0)
        return String("Time information is not available.");

    String ss = readSecond(_currentEpoch);
    String mm = readMinute(_currentEpoch);
    String HH = readHour(_currentEpoch);
    String DD = readDay(_currentEpoch);
    String MM = readMonth(_currentEpoch);
    String YYYY = readYear(_currentEpoch);

    if (format == "HH:mm:ss")
        return HH + ":" + mm + ":" + ss;

    if (format == "HH:mm")
        return HH + ":" + mm;

    if (format == "MM/dd" || format == "mm/dd" || format == "MM/DD" || format == "mm/DD")
        return MM + "/" + DD;

    if (format == "mm/dd HH:mm:ss")
        return MM + "/" + DD + " " + HH + ":" + mm + ":" + ss;

    if (format == "mm/dd HH:mm")
        return MM + "/" + DD + " " + HH + ":" + mm;

    return YYYY + "/" + MM + "/" + DD + " " + HH + ":" + mm + ":" + ss;
}

void M5_Ethernet_NtpClient::updateCurrentEpoch()
{
    currentEpoch = lastEpoch + ((millis() - lastMillis) / 1000);
}

String M5_Ethernet_NtpClient::getTime()
{
    updateCurrentEpoch();
    char buffer[30];
    sprintf(buffer, "%02d:%02d:%02d", (currentEpoch % 86400L) / 3600, (currentEpoch % 3600) / 60, currentEpoch % 60);
    return String(buffer);
}

String M5_Ethernet_NtpClient::getTime(String address)
{
    return getTime(address, timezoneOffset);
}

String M5_Ethernet_NtpClient::getTime(String address, int timezone)
{
    timezoneOffset = timezone;
    if (lastEpoch == 0)
    {
        updateTimeFromServer(address, timezone);
    }
    else
    {
        return getTime();
    }
    return String("Failed to get time");
}

// send an NTP request to the time server at the given address
void M5_Ethernet_NtpClient::sendNTPpacket(const char *address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); // NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

bool M5_Ethernet_NtpClient::enable()
{
    return lastEpoch != 0;
}

String M5_Ethernet_NtpClient::readYear()
{
    return readYear(currentEpoch);
}

String M5_Ethernet_NtpClient::readYear(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        return String(ptm->tm_year + 1900); // tm_year is years since 1900
    }
    return String("Year not available");
}

String M5_Ethernet_NtpClient::readMonth()
{
    return readMonth(currentEpoch);
}

String M5_Ethernet_NtpClient::readMonth(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_mon + 1); // tm_mon is months since January (0-11)
        return String(buffer);
    }
    return String("Month not available");
}

String M5_Ethernet_NtpClient::readDay()
{
    return readDay(currentEpoch);
}

String M5_Ethernet_NtpClient::readDay(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_mday); // tm_mday is day of the month (1-31)
        return String(buffer);
    }
    return String("Day not available");
}

String M5_Ethernet_NtpClient::readHour()
{
    return readHour(currentEpoch);
}

String M5_Ethernet_NtpClient::readHour(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_hour); // tm_hour is hour of the day (0-23)
        return String(buffer);
    }
    return String("Hour not available");
}

String M5_Ethernet_NtpClient::readHour(unsigned long Epoch, int div)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", (ptm->tm_hour / div) * div); // tm_hour is hour of the day (0-23)
        return String(buffer);
    }
    return String("Hour not available");
}

String M5_Ethernet_NtpClient::readMinute()
{
    return readMinute(currentEpoch);
}

String M5_Ethernet_NtpClient::readMinute(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_min); // tm_min is minute of the hour (0-59)
        return String(buffer);
    }
    return String("Minute not available");
}

String M5_Ethernet_NtpClient::readSecond()
{
    return readSecond(currentEpoch);
}

String M5_Ethernet_NtpClient::readSecond(unsigned long Epoch)
{
    if (Epoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&Epoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_sec); // tm_sec is second of the minute (0-59)
        return String(buffer);
    }
    return String("Second not available");
}
