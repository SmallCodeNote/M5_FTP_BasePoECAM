/*
MIT License

Copyright (c) 2019 Leonardo Bispo
Copyright (c) 2022 Khoi Hoang
Copyright (c) 2024 SmallCodeNote

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <M5Unified.h>
#include <Arduino.h>
#include <SPI.h>
#include <M5_Ethernet.h>
#include <vector>
#include <pgmspace.h>

#ifndef M5_Ethernet_FtpClient_H
#define M5_Ethernet_FtpClient_H

#define PSTR(s) (s)
#define FPSTR(str_pointer) (reinterpret_cast<const __FlashStringHelper *>(str_pointer))
#define F(string_literal) (FPSTR(PSTR(string_literal)))

#define FTP_PORT 21
//#define FTP_BUFFER_SIZE 1500
#define FTP_BUFFER_SIZE 3500
#define FTP_TIMEOUT_MS 3000UL
#define FTP_ENTERING_PASSIVE_MODE 227

#define FTP_RESCODE_SERVER_ISNOT_FOUND 404
#define FTP_RESCODE_CLIENT_ISNOT_CONNECTED 426
#define FTP_RESCODE_DATA_CONNECTION_ERROR 425
#define FTP_RESCODE_ACTION_SUCCESS 200 // The requested action has been successfully.
#define FTP_RESCODE_SYNTAX_ERROR 500

#define FTP_RESCODE_NOT_LOGIN 530

#define FTP_COMMAND_QUIT F("QUIT")
#define FTP_COMMAND_USER F("USER ")
#define FTP_COMMAND_PASS F("PASS ")

#define FTP_COMMAND_RENAME_FILE_FROM F("RNFR ")
#define FTP_COMMAND_RENAME_FILE_TO F("RNTO ")

#define FTP_COMMAND_FILE_LAST_MOD_TIME F("MDTM ")

#define FTP_COMMAND_APPEND_FILE F("APPE ")
#define FTP_COMMAND_DELETE_FILE F("DELE ")

#define FTP_COMMAND_CURRENT_WORKING_DIR F("CWD ")
#define FTP_COMMAND_MAKE_DIR F("MKD ")
#define FTP_COMMAND_REMOVE_DIR F("RMD ")
#define FTP_COMMAND_LIST_DIR_STANDARD F("MLSD ")
#define FTP_COMMAND_LIST_DIR F("LIST ")

#define FTP_COMMAND_DOWNLOAD F("RETR ")
#define FTP_COMMAND_FILE_UPLOAD F("STOR ")

#define FTP_COMMAND_PASSIVE_MODE F("PASV")

#define FTP_COMMAND_SERVER_SYSTEM_INFO F("SYST")
#define FTP_COMMAND_SERVER_STATUS_INFO F("STAT")

class M5_Ethernet_FtpClient
{
private:
    uint16_t WriteClientBuffered(EthernetClient *cli, unsigned char *data, int dataLength);

    EthernetClient ftpClient;
    EthernetClient dclient;

    char outBuf[1024];
    unsigned char outCount;

    String userName;
    String passWord;
    String serverAdress;

    uint16_t port;

    bool _isConnected = false;
    unsigned char clientBuf[FTP_BUFFER_SIZE];
    size_t bufferSize = FTP_BUFFER_SIZE;
    uint16_t timeout = FTP_TIMEOUT_MS;

    EthernetClient *GetDataClient();

    IPAddress _dataAddress;
    uint16_t _dataPort;

    bool inASCIIMode = false;

    std::vector<String> SplitPath(const String &path);

    unsigned long nextConnectionCheckMillis = 0;

public:
    //    M5_Ethernet_FtpClient(char *_serverAdress, uint16_t _port, char *_userName, char *_passWord, uint16_t _timeout = 10000);
    //    M5_Ethernet_FtpClient(char *_serverAdress, char *_userName, char *_passWord, uint16_t _timeout = 10000);

    M5_Ethernet_FtpClient(String _serverAdress, uint16_t _port, String _userName, String _passWord, uint16_t _timeout = 3000);
    M5_Ethernet_FtpClient(String _serverAdress, String _userName, String _passWord, uint16_t _timeout = 3000);

    bool isErrorCode(uint16_t responseCode);
    bool isSuccessCode(uint16_t responseCode);
    u_int8_t getSockIndex();

    uint16_t OpenConnection();
    void CloseConnection();
    bool isConnected();
    uint16_t InitAsciiPassiveMode();
    uint16_t NewFile(String fileName);
    uint16_t AppendFile(String fileName);
    uint16_t AppendText(String filePath, String textLine);
    uint16_t AppendTextLine(String filePath, String textLine);
    uint16_t AppendDataArrayAsTextLine(String filePath, String headLine, u_int16_t *buf, int32_t len);
    uint16_t AppendData(String filePath, unsigned char *data, int datalength);
    uint16_t WriteData(unsigned char *data, int dataLength);
    uint16_t WriteData(String data);
    uint16_t CloseDataClient();
    uint16_t GetCmdAnswer(char *result = NULL, int offsetStart = 0);
    uint16_t GetLastModifiedTime(const char *fileName, char *result);
    uint16_t RenameFile(String from, String to);
    uint16_t Write(const char *str);
    uint16_t ChangeWorkDir(String dir);
    uint16_t DeleteFile(String file);
    bool DirExists(String dir);
    uint16_t MakeDir(String dir);
    uint16_t MakeDirRecursive(String dir);
    uint16_t RemoveDir(String dir);
    uint16_t ContentList(const char *dir, String *list);
    uint16_t ContentListWithListCommand(const char *dir, String *list);
    uint16_t DownloadString(const char *filename, String &str);
    uint16_t DownloadFile(const char *filename, unsigned char *buf, size_t length, bool printUART = false);

    String GetServerAddress();
    String GetDirectoryPath(const String &path);
    bool SetUserName(String _userName);
    bool SetPassWord(String _passWord);
    bool SetServerAddress(String _serverAdress);

    bool CheckServerAnswerReturn(); // server return answer : true
    bool CheckServerLogin();        // user login : true
};

// #define FTP_DEBUG_OUTPUT M5.Lcd
// #define FTP_DEBUG_OUTPUT      Serial
#define FTP_DEBUG_OUTPUT M5.Log

const char FTP_MARK[] = "[FTP] ";
const char FTP_SPACE[] = " ";
const char FTP_LINE[] = "========================================\n";

#define FTP_PRINT_MARK FTP_PRINT(FTP_MARK)
#define FTP_PRINT_SP FTP_PRINT(FTP_SPACE)
#define FTP_PRINT_LINE FTP_PRINT(FTP_LINE)

#define FTP_PRINT FTP_DEBUG_OUTPUT.print
#define FTP_PRINTLN FTP_DEBUG_OUTPUT.println

///////////////////////////////////////

#define FTP_LOGERROR(x)     \
    if (_FTP_LOGLEVEL_ > 0) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
    }
#define FTP_LOGERROR_LINE(x) \
    if (_FTP_LOGLEVEL_ > 0)  \
    {                        \
        FTP_PRINT_MARK;      \
        FTP_PRINTLN(x);      \
        FTP_PRINT_LINE;      \
    }
#define FTP_LOGERROR0(x)    \
    if (_FTP_LOGLEVEL_ > 0) \
    {                       \
        FTP_PRINT(x);       \
    }
#define FTP_LOGERROR1(x, y) \
    if (_FTP_LOGLEVEL_ > 0) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINT(x);       \
        FTP_PRINT_SP;       \
        FTP_PRINTLN(y);     \
    }
#define FTP_LOGERROR2(x, y, z) \
    if (_FTP_LOGLEVEL_ > 0)    \
    {                          \
        FTP_PRINT_MARK;        \
        FTP_PRINT(x);          \
        FTP_PRINT_SP;          \
        FTP_PRINT(y);          \
        FTP_PRINT_SP;          \
        FTP_PRINTLN(z);        \
    }
#define FTP_LOGERROR3(x, y, z, w) \
    if (_FTP_LOGLEVEL_ > 0)       \
    {                             \
        FTP_PRINT_MARK;           \
        FTP_PRINT(x);             \
        FTP_PRINT_SP;             \
        FTP_PRINT(y);             \
        FTP_PRINT_SP;             \
        FTP_PRINT(z);             \
        FTP_PRINT_SP;             \
        FTP_PRINTLN(w);           \
    }
#define FTP_LOGERROR5(x, y, z, w, xx, yy) \
    if (_FTP_LOGLEVEL_ > 0)               \
    {                                     \
        FTP_PRINT_MARK;                   \
        FTP_PRINT(x);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(y);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(z);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(w);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(xx);                    \
        FTP_PRINT_SP;                     \
        FTP_PRINTLN(yy);                  \
    }

///////////////////////////////////////

#define FTP_LOGWARN(x)      \
    if (_FTP_LOGLEVEL_ > 1) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
    }
#define FTP_LOGWARN_LINE(x) \
    if (_FTP_LOGLEVEL_ > 1) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
        FTP_PRINT_LINE;     \
    }
#define FTP_LOGWARN0(x)     \
    if (_FTP_LOGLEVEL_ > 1) \
    {                       \
        FTP_PRINT(x);       \
    }
#define FTP_LOGWARN1(x, y)  \
    if (_FTP_LOGLEVEL_ > 1) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINT(x);       \
        FTP_PRINT_SP;       \
        FTP_PRINTLN(y);     \
    }
#define FTP_LOGWARN2(x, y, z) \
    if (_FTP_LOGLEVEL_ > 1)   \
    {                         \
        FTP_PRINT_MARK;       \
        FTP_PRINT(x);         \
        FTP_PRINT_SP;         \
        FTP_PRINT(y);         \
        FTP_PRINT_SP;         \
        FTP_PRINTLN(z);       \
    }
#define FTP_LOGWARN3(x, y, z, w) \
    if (_FTP_LOGLEVEL_ > 1)      \
    {                            \
        FTP_PRINT_MARK;          \
        FTP_PRINT(x);            \
        FTP_PRINT_SP;            \
        FTP_PRINT(y);            \
        FTP_PRINT_SP;            \
        FTP_PRINT(z);            \
        FTP_PRINT_SP;            \
        FTP_PRINTLN(w);          \
    }
#define FTP_LOGWARN5(x, y, z, w, xx, yy) \
    if (_FTP_LOGLEVEL_ > 1)              \
    {                                    \
        FTP_PRINT_MARK;                  \
        FTP_PRINT(x);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(y);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(z);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(w);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(xx);                   \
        FTP_PRINT_SP;                    \
        FTP_PRINTLN(yy);                 \
    }

///////////////////////////////////////

#define FTP_LOGINFO(x)      \
    if (_FTP_LOGLEVEL_ > 2) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
    }
#define FTP_LOGINFO_LINE(x) \
    if (_FTP_LOGLEVEL_ > 2) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
        FTP_PRINT_LINE;     \
    }
#define FTP_LOGINFO0(x)     \
    if (_FTP_LOGLEVEL_ > 2) \
    {                       \
        FTP_PRINT(x);       \
    }
#define FTP_LOGINFO1(x, y)  \
    if (_FTP_LOGLEVEL_ > 2) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINT(x);       \
        FTP_PRINT_SP;       \
        FTP_PRINTLN(y);     \
    }
#define FTP_LOGINFO2(x, y, z) \
    if (_FTP_LOGLEVEL_ > 2)   \
    {                         \
        FTP_PRINT_MARK;       \
        FTP_PRINT(x);         \
        FTP_PRINT_SP;         \
        FTP_PRINT(y);         \
        FTP_PRINT_SP;         \
        FTP_PRINTLN(z);       \
    }
#define FTP_LOGINFO3(x, y, z, w) \
    if (_FTP_LOGLEVEL_ > 2)      \
    {                            \
        FTP_PRINT_MARK;          \
        FTP_PRINT(x);            \
        FTP_PRINT_SP;            \
        FTP_PRINT(y);            \
        FTP_PRINT_SP;            \
        FTP_PRINT(z);            \
        FTP_PRINT_SP;            \
        FTP_PRINTLN(w);          \
    }
#define FTP_LOGINFO5(x, y, z, w, xx, yy) \
    if (_FTP_LOGLEVEL_ > 2)              \
    {                                    \
        FTP_PRINT_MARK;                  \
        FTP_PRINT(x);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(y);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(z);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(w);                    \
        FTP_PRINT_SP;                    \
        FTP_PRINT(xx);                   \
        FTP_PRINT_SP;                    \
        FTP_PRINTLN(yy);                 \
    }

///////////////////////////////////////

#define FTP_LOGDEBUG(x)     \
    if (_FTP_LOGLEVEL_ > 3) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINTLN(x);     \
    }
#define FTP_LOGDEBUG_LINE(x) \
    if (_FTP_LOGLEVEL_ > 3)  \
    {                        \
        FTP_PRINT_MARK;      \
        FTP_PRINTLN(x);      \
        FTP_PRINT_LINE;      \
    }
#define FTP_LOGDEBUG0m(x)   \
    if (_FTP_LOGLEVEL_ > 3) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINT(x);       \
    }
#define FTP_LOGDEBUG0(x)    \
    if (_FTP_LOGLEVEL_ > 3) \
    {                       \
        FTP_PRINT(x);       \
    }
#define FTP_LOGDEBUG1(x, y) \
    if (_FTP_LOGLEVEL_ > 3) \
    {                       \
        FTP_PRINT_MARK;     \
        FTP_PRINT(x);       \
        FTP_PRINT_SP;       \
        FTP_PRINTLN(y);     \
    }
#define FTP_LOGHEXDEBUG1(x, y) \
    if (_FTP_LOGLEVEL_ > 3)    \
    {                          \
        FTP_PRINT_MARK;        \
        FTP_PRINT(x);          \
        FTP_PRINT_SP;          \
        FTP_PRINTLN(y, HEX);   \
    }
#define FTP_LOGDEBUG2(x, y, z) \
    if (_FTP_LOGLEVEL_ > 3)    \
    {                          \
        FTP_PRINT_MARK;        \
        FTP_PRINT(x);          \
        FTP_PRINT_SP;          \
        FTP_PRINT(y);          \
        FTP_PRINT_SP;          \
        FTP_PRINTLN(z);        \
    }
#define FTP_LOGDEBUG3(x, y, z, w) \
    if (_FTP_LOGLEVEL_ > 3)       \
    {                             \
        FTP_PRINT_MARK;           \
        FTP_PRINT(x);             \
        FTP_PRINT_SP;             \
        FTP_PRINT(y);             \
        FTP_PRINT_SP;             \
        FTP_PRINT(z);             \
        FTP_PRINT_SP;             \
        FTP_PRINTLN(w);           \
    }
#define FTP_LOGDEBUG5(x, y, z, w, xx, yy) \
    if (_FTP_LOGLEVEL_ > 3)               \
    {                                     \
        FTP_PRINT_MARK;                   \
        FTP_PRINT(x);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(y);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(z);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(w);                     \
        FTP_PRINT_SP;                     \
        FTP_PRINT(xx);                    \
        FTP_PRINT_SP;                     \
        FTP_PRINTLN(yy);                  \
    }

/**
 * @brief need val ... uint16_t responceCode;
 */
#define FTP_CMD_SEND_AND_ERROR_RETURN_UINT16T(cmd) \
    FTP_LOGINFO(cmd);                              \
    ftpClient.println(cmd);                        \
    responseCode = GetCmdAnswer();                 \
    if (isErrorCode(responseCode))                 \
        return responseCode;

#endif