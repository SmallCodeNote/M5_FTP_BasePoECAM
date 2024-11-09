// #include <M5Unified.h>
#include "M5PoECAM.h"
#include <M5_Ethernet.h>
#include <time.h>

#ifndef M5_Ethernet_NtpClient_H
#define M5_Ethernet_NtpClient_H

#define NTP_PACKET_SIZE 48 // NTP time stamp is in the first 48 bytes of the message

class M5_Ethernet_NtpClient
{
private:
public:
    unsigned int localPort = 8888; // local port to listen for UDP packets
    EthernetUDP Udp;
    byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
    void sendNTPpacket(const char *address);

    unsigned long lastEpoch = 0;
    unsigned long lastMillis = 0;
    unsigned long intMillis = 0;
    unsigned long Interval = 60;
    unsigned long currentEpoch = 0;

    int timezoneOffset = +9;

    void begin();
    String getTime(String address);
    String getTime(String address, int timezoneOffset);

    String readYear();
    String readMonth();
    String readDay();
    String readHour();
    String readMinute();
    String readSecond();
};

M5_Ethernet_NtpClient NtpClient;

void M5_Ethernet_NtpClient::begin()
{
    Udp.begin(localPort);
}

String M5_Ethernet_NtpClient::getTime(String address)
{
    return getTime(address, timezoneOffset);
}

String M5_Ethernet_NtpClient::getTime(String address, int timezone)
{
    timezoneOffset = timezone;
    if ((lastEpoch == 0 || (millis() - lastMillis) > Interval * 1000 && (millis() - intMillis) > Interval * 1000))
    {
        sendNTPpacket(address.c_str());
        delay(1000);
        if (Udp.parsePacket())
        {
            Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            const unsigned long seventyYears = 2208988800UL;
            unsigned long epoch = secsSince1900 - seventyYears;
            epoch += timezoneOffset * 3600;

            lastEpoch = epoch;
            lastMillis = millis();
            intMillis = millis();
            currentEpoch = lastEpoch;

            // create a time string
            char buffer[30];
            sprintf(buffer, "%02d:%02d:%02d", (epoch % 86400L) / 3600, (epoch % 3600) / 60, epoch % 60);
            return String(buffer);
        }
        intMillis = millis();
    }

    if (lastEpoch != 0)
    {
        currentEpoch = lastEpoch + ((millis() - lastMillis) / 1000);

        // create a time string
        char buffer[30];
        sprintf(buffer, "%02d:%02d:%02d", (currentEpoch % 86400L) / 3600, (currentEpoch % 3600) / 60, currentEpoch % 60);
        return String(buffer);
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

String M5_Ethernet_NtpClient::readYear()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        return String(ptm->tm_year + 1900); // tm_year is years since 1900
    }
    return String("Year not available");
}

String M5_Ethernet_NtpClient::readMonth()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_mon + 1); // tm_mon is months since January (0-11)
        return String(buffer);
    }
    return String("Month not available");
}

String M5_Ethernet_NtpClient::readDay()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_mday); // tm_mday is day of the month (1-31)
        return String(buffer);
    }
    return String("Day not available");
}

String M5_Ethernet_NtpClient::readHour()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_hour); // tm_hour is hour of the day (0-23)
        return String(buffer);
    }
    return String("Hour not available");
}

String M5_Ethernet_NtpClient::readMinute()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_min); // tm_min is minute of the hour (0-59)
        return String(buffer);
    }
    return String("Minute not available");
}

String M5_Ethernet_NtpClient::readSecond()
{
    if (currentEpoch != 0)
    {
        struct tm *ptm = gmtime((time_t *)&currentEpoch);
        char buffer[3];
        sprintf(buffer, "%02d", ptm->tm_sec); // tm_sec is second of the minute (0-59)
        return String(buffer);
    }
    return String("Second not available");
}

#endif
