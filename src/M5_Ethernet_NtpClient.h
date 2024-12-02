#include <M5Unified.h>
#include <M5_Ethernet.h>
#include <time.h>
#include <TimeLib.h>

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

    void updateCurrentEpoch();
    String getTime();
    String getTime(String address);
    String getTime(String address, int timezoneOffset);

    void updateTimeFromServer(String address);
    void updateTimeFromServer(String address, int timezoneOffset);
    void updateTimeFromString(String timeString);
    void updateTimeFromString(String timeString, int timezone);

    bool enable();

    String readYear();
    String readMonth();
    String readDay();
    String readHour();
    String readMinute();
    String readSecond();

    String readYear(unsigned long Epoch);
    String readMonth(unsigned long Epoch);
    String readDay(unsigned long Epoch);
    String readHour(unsigned long Epoch);
    String readHour(unsigned long Epoch,int div);
    String readMinute(unsigned long Epoch);
    String readSecond(unsigned long Epoch);

    String convertTimeEpochToString();
    String convertTimeEpochToString(unsigned long _currentEpoch);
    String convertTimeEpochToString(String format);
    String convertTimeEpochToString(String format,unsigned long _currentEpoch);
    unsigned long convertTimeStringToEpoch(String timeString);

};

#endif