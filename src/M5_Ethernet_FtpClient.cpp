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
#include "M5_Ethernet_FtpClient.h"

/* Add Log_Class.hpp
    /// Overload
    void print(const __FlashStringHelper* string);
    void println(const __FlashStringHelper* string);
    void println(const String& string);
    void print(const IPAddress& ip);
    void print(uint16_t value);
    void println(uint16_t value);
*/

// Log_Class overload
namespace m5
{
  void Log_Class::print(const __FlashStringHelper *string)
  {
    return printf_P(reinterpret_cast<const char *>(string));
  }

  void Log_Class::println(const __FlashStringHelper *string)
  {
    return printf_P(reinterpret_cast<const char *>(string));
  }

  void Log_Class::println(const String &string)
  {
    return printf("%s\n", string.c_str());
  }

  void Log_Class::print(const IPAddress &ip)
  {
    return printf("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
  void Log_Class::print(uint16_t value)
  {
    return printf("%u", value);
  }

  void Log_Class::println(uint16_t value)
  {
    return printf("%u\n", value);
  }
}

M5_Ethernet_FtpClient::M5_Ethernet_FtpClient(String _serverAdress, uint16_t _port, String _userName, String _passWord, uint16_t _timeout)
{
  userName = _userName;
  passWord = _passWord;
  serverAdress = _serverAdress;
  port = _port;
  timeout = _timeout;
}

M5_Ethernet_FtpClient::M5_Ethernet_FtpClient(String _serverAdress, String _userName, String _passWord, uint16_t _timeout)
{
  userName = _userName;
  passWord = _passWord;
  serverAdress = _serverAdress;
  port = FTP_PORT;
  timeout = _timeout;
}

String M5_Ethernet_FtpClient::GetServerAddress() { return serverAdress; };

bool M5_Ethernet_FtpClient::SetUserName(String _userName)
{
  userName = _userName;
  return true;
};
bool M5_Ethernet_FtpClient::SetPassWord(String _passWord)
{
  passWord = _passWord;
  return true;
};
bool M5_Ethernet_FtpClient::SetServerAddress(String _serverAdress)
{
  serverAdress = _serverAdress;
  return true;
};

EthernetClient *M5_Ethernet_FtpClient::GetDataClient()
{
  return &dclient;
}

bool M5_Ethernet_FtpClient::isConnected()
{
  return _isConnected;
}

u_int8_t M5_Ethernet_FtpClient::getSockIndex()
{
  return ftpClient.getSocketNumber();
}

bool M5_Ethernet_FtpClient::isErrorCode(uint16_t responseCode)
{
  return responseCode >= 400 && responseCode < 600;
}
bool M5_Ethernet_FtpClient::isSuccessCode(uint16_t responseCode)
{
  return !isErrorCode(responseCode);
}
/**
 * @brief Open command connection
 */
uint16_t M5_Ethernet_FtpClient::OpenConnection()
{
  uint16_t responceCode = FTP_RESCODE_ACTION_SUCCESS;

  if (nextConnectionCheckMillis > millis())
  {
    FTP_LOGINFO1(F("not found server wait: "), serverAdress);
    return FTP_RESCODE_SERVER_ISNOT_FOUND;
  }

  FTP_LOGINFO1(F("Connecting to: "), serverAdress);

#if ((ESP32) && !FTP_CLIENT_USING_ETHERNET)
  if (ftpClient.connect(serverAdress, port, timeout))
#else
  if (ftpClient.connect(serverAdress.c_str(), port))
#endif
  {
    FTP_LOGINFO(F("Command connected"));
  }
  else
  {
    nextConnectionCheckMillis = millis() + 20000UL;
    FTP_LOGINFO1(F("not found server : "), serverAdress);
    return FTP_RESCODE_SERVER_ISNOT_FOUND;
  }

  responceCode = GetCmdAnswer();
  if (isErrorCode(responceCode))
    return responceCode;

  FTP_LOGINFO1("Send USER =", userName);
  ftpClient.print(FTP_COMMAND_USER);
  ftpClient.println(userName);

  responceCode = GetCmdAnswer();
  if (isErrorCode(responceCode))
    return responceCode;

  FTP_LOGINFO1("Send PASSWORD =", passWord);
  ftpClient.print(FTP_COMMAND_PASS);
  ftpClient.println(passWord);

  responceCode = GetCmdAnswer();

  if (isSuccessCode(responceCode))
  {
    _isConnected = true;
  }
  return responceCode;
}

/**
 * @brief Close command connection
 */
void M5_Ethernet_FtpClient::CloseConnection()
{
  ftpClient.println(FTP_COMMAND_QUIT);
  ftpClient.stop();
  FTP_LOGINFO(F("Connection closed"));
  _isConnected = false;
}
/////////////////////////////////////////////

bool M5_Ethernet_FtpClient::CheckServerAnswerReturn()
{
  uint16_t responseCode = FTP_RESCODE_DATA_CONNECTION_ERROR;
  if (!isConnected())
  {
    FTP_LOGERROR("CheckServerAnswerReturn: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send STAT");
  ftpClient.print(FTP_COMMAND_SERVER_SYSTEM_INFO);
  responseCode = GetCmdAnswer();

  return isSuccessCode(responseCode);
}
/////////////////////////////////////////////

bool M5_Ethernet_FtpClient::CheckServerLogin()
{
  uint16_t responseCode = FTP_RESCODE_DATA_CONNECTION_ERROR;
  if (!isConnected())
  {
    FTP_LOGERROR("ChangeWorkDir: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send STAT");
  ftpClient.print(FTP_COMMAND_SERVER_SYSTEM_INFO);
  responseCode = GetCmdAnswer();

  return responseCode == 215; // NAME system type return for login user code.
}

/**
 * @brief Retrieves and processes the response from the FTP server, updating the connection status and storing the result.
 */
uint16_t M5_Ethernet_FtpClient::GetCmdAnswer(char *result , int offsetStart )
{
  char thisByte;
  outCount = 0;
  unsigned long _m = millis();

  while (!ftpClient.available() && millis() < _m + timeout)
  {
    delay(1);
  }

  if (!ftpClient.available())
  {
    M5_LOGE("!ftpClient.available()");
    memset(outBuf, 0, sizeof(outBuf));
    strcpy(outBuf, "NoAnswer");

    //_isConnected = false;
    // isConnected();
    M5_LOGD("LOGIN and get ANSWER");
    return 0;
  }

  while (ftpClient.available())
  {
    thisByte = ftpClient.read();
    if (outCount < sizeof(outBuf))
    {
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  uint16_t responseCode = atoi(outBuf);

  if (result != NULL)
  {
    // Deprecated
    for (uint32_t i = offsetStart; i < sizeof(outBuf); i++)
    {
      result[i] = outBuf[i - offsetStart];
    }
    FTP_LOGDEBUG0("!>");
  }
  else
  {
    FTP_LOGDEBUG0("->");
  }

  FTP_LOGDEBUG0(outBuf);
  return responseCode;
}

/**
 * @brief Initializes the FTP client in passive mode.
 *
 * This function sets the FTP client to ASCII mode, sends the PASV command to the FTP server,
 * and processes the server's response to establish a data connection in passive mode.
 */
uint16_t M5_Ethernet_FtpClient::InitAsciiPassiveMode()
{
  uint16_t responseCode = FTP_RESCODE_SYNTAX_ERROR;

  FTP_LOGINFO("Send TYPE A");
  ftpClient.println("TYPE A"); // Set ASCII mode
  responseCode = GetCmdAnswer();

  if (isErrorCode(responseCode))
    return responseCode;

  FTP_LOGINFO("Send PASV");
  ftpClient.println(FTP_COMMAND_PASSIVE_MODE);

  responseCode = GetCmdAnswer();
  if (isErrorCode(responseCode))
    return responseCode;

  char *tmpPtr;
  while (strtol(outBuf, &tmpPtr, 10) != FTP_ENTERING_PASSIVE_MODE)
  {
    ftpClient.println(FTP_COMMAND_PASSIVE_MODE);

    responseCode = GetCmdAnswer();
    if (isErrorCode(responseCode))
      return responseCode;

    delay(1000);
  }

  // Test to know which format
  // 227 Entering Passive Mode (192,168,2,112,157,218)
  // 227 Entering Passive Mode (4043483328, port 55600)
  char *passiveIP = strchr(outBuf, '(') + 1;

  if (atoi(passiveIP) <= 0xFF) // 227 Entering Passive Mode (192,168,2,112,157,218)
  {
    char *tStr = strtok(outBuf, "(,");
    int array_pasv[6];

    for (int i = 0; i < 6; i++)
    {
      tStr = strtok(NULL, "(,");
      if (tStr == NULL)
      {
        FTP_LOGDEBUG(F("Bad PASV Answer"));
        CloseConnection();
        return FTP_RESCODE_SYNTAX_ERROR;
      }
      array_pasv[i] = atoi(tStr);
    }
    unsigned int hiPort, loPort;
    hiPort = array_pasv[4] << 8;
    loPort = array_pasv[5]; // & 255;

    _dataAddress = IPAddress(array_pasv[0], array_pasv[1], array_pasv[2], array_pasv[3]);
    _dataPort = hiPort | loPort;
  }
  else // 227 Entering Passive Mode (4043483328, port 55600)
  {
    // Using with old style PASV answer, such as `FTP_Server_Teensy41` library
    char *ptr = strtok(passiveIP, ",");
    uint32_t ret = strtoul(ptr, &tmpPtr, 10);

    passiveIP = strchr(outBuf, ')');
    ptr = strtok(passiveIP, "port ");

    _dataAddress = IPAddress(ret);
    _dataPort = strtol(ptr, &tmpPtr, 10);
  }

  FTP_LOGINFO3(F("dataAddress:"), _dataAddress, F(", dataPort:"), _dataPort);

// data connection create
#if ((ESP32) && !FTP_CLIENT_USING_ETHERNET)
  if (dclient.connect(_dataAddress, _dataPort, timeout))
#else
  if (dclient.connect(_dataAddress, _dataPort))
#endif
  {
    FTP_LOGDEBUG(F("Data connection established"));
    responseCode = FTP_RESCODE_ACTION_SUCCESS;
  }
  else
  {
    FTP_LOGDEBUG(F("Data connection not established error"));
    responseCode = FTP_RESCODE_DATA_CONNECTION_ERROR;
  }

  return responseCode;
}

/**
 * @brief Sends a directory listing command to the FTP server and retrieves the list of directory contents.
 */
uint16_t M5_Ethernet_FtpClient::ContentList(const char *dir, String *list)
{
  if (!isConnected())
  {
    FTP_LOGERROR("ContentList: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  char _resp[sizeof(outBuf)];

  FTP_LOGINFO("Send MLSD");
  ftpClient.print(FTP_COMMAND_LIST_DIR_STANDARD);
  ftpClient.println(dir);

  uint16_t responseCode = GetCmdAnswer(_resp);
  if (isErrorCode(responseCode))
    return responseCode;

  // Convert char array to string to manipulate and find response size
  // each server reports it differently, TODO = FEAT
  // String resp_string = _resp;
  // resp_string.substring(resp_string.lastIndexOf('matches')-9);
  // FTP_LOGDEBUG(resp_string);

  unsigned long _m = millis();

  while (!dclient.available() && millis() < _m + timeout)
    delay(1);

  uint16_t _b = 0;
  while (dclient.available())
  {
    if (_b < 256)
    {
      list[_b] = dclient.readStringUntil('\n');
      FTP_LOGDEBUG(String(_b) + ":" + list[_b]);
      _b++;
    }
  }

  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::ContentListWithListCommand(const char *dir, String *list)
{
  if (!isConnected())
  {
    FTP_LOGERROR("ContentListWithListCommand: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  char _resp[sizeof(outBuf)];
  uint16_t _b = 0;

  FTP_LOGINFO("Send LIST");
  ftpClient.print(FTP_COMMAND_LIST_DIR);
  ftpClient.println(dir);

  uint16_t responseCode = GetCmdAnswer(_resp);
  if (isErrorCode(responseCode))
    return responseCode;

  // Convert char array to string to manipulate and find response size
  // each server reports it differently, TODO = FEAT
  // String resp_string = _resp;
  // resp_string.substring(resp_string.lastIndexOf('matches')-9);
  // FTP_LOGDEBUG(resp_string);

  FTP_LOGINFO("Expand LIST");
  unsigned long _m = millis();

  while (!dclient.available() && millis() < _m + timeout)
  {
    delay(1);
  }

  while (dclient.available())
  {
    if (_b < 128)
    {
      String tmp = dclient.readStringUntil('\n');
      list[_b] = tmp.substring(tmp.lastIndexOf(" ") + 1, tmp.length());
      FTP_LOGDEBUG(String(_b) + ":" + tmp);
      _b++;
    }
  }

  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::GetLastModifiedTime(const char *fileName, char *result)
{
  if (!isConnected())
  {
    FTP_LOGERROR("GetLastModifiedTime: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send MDTM");
  ftpClient.print(FTP_COMMAND_FILE_LAST_MOD_TIME);
  ftpClient.println(fileName);
  return GetCmdAnswer(result, 4);
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::Write(const char *str)
{
  if (!isConnected())
  {
    FTP_LOGERROR("Write: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGDEBUG(F("Write File"));
  GetDataClient()->print(str);

  return FTP_RESCODE_ACTION_SUCCESS;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::CloseDataClient()
{
  if (!isConnected())
  {
    FTP_LOGERROR("CloseFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGDEBUG(F("Close File"));
  dclient.stop();

  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::RenameFile(String from, String to)
{
  if (!isConnected())
  {
    FTP_LOGERROR("RenameFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send RNFR");
  ftpClient.print(FTP_COMMAND_RENAME_FILE_FROM);
  ftpClient.println(from);

  uint16_t responseCode = GetCmdAnswer();
  if (isErrorCode(responseCode))
    return responseCode;

  FTP_LOGINFO("Send RNTO");
  ftpClient.print(FTP_COMMAND_RENAME_FILE_TO);
  ftpClient.println(to);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::NewFile(String fileName)
{
  if (!isConnected())
  {
    FTP_LOGERROR("NewFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send STOR");
  ftpClient.print(FTP_COMMAND_FILE_UPLOAD);
  ftpClient.println(fileName);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::ChangeWorkDir(String dir)
{
  if (!isConnected())
  {
    FTP_LOGERROR("ChangeWorkDir: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send CWD");
  ftpClient.print(FTP_COMMAND_CURRENT_WORKING_DIR);
  ftpClient.println(dir);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

bool M5_Ethernet_FtpClient::DirExists(String dir)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;

  if (!isConnected())
  {
    FTP_LOGERROR("DirExists: Not connected error");
    return false;
  }

  FTP_LOGINFO("Send CWD");

  unsigned long startMillis = millis();
  ftpClient.print(FTP_COMMAND_CURRENT_WORKING_DIR);
  ftpClient.println(dir);

  responseCode = GetCmdAnswer();

  ftpClient.print(FTP_COMMAND_CURRENT_WORKING_DIR);
  ftpClient.println("/");
  GetCmdAnswer();

  M5_LOGD("ans = %u ((%u))\r\n:: %s", responseCode, millis() - startMillis, dir.c_str());
  return responseCode == 250;
}

uint16_t M5_Ethernet_FtpClient::MakeDir(String dir)
{
  if (!isConnected())
  {
    FTP_LOGERROR("MakeDir: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send MKD");
  ftpClient.print(FTP_COMMAND_MAKE_DIR);
  ftpClient.println(dir);
  return GetCmdAnswer();
}

uint16_t M5_Ethernet_FtpClient::MakeDirRecursive(String dir)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;

  if (!isConnected())
  {
    M5_LOGI("MakeDirRecursive: Not connected error");
    FTP_LOGERROR("MakeDirRecursive: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send MKD");
  ftpClient.print(FTP_COMMAND_MAKE_DIR);
  ftpClient.println(dir);
  responseCode = GetCmdAnswer();

  if (isSuccessCode(responseCode))
  { // Success
    M5_LOGI();
    return responseCode;
  }

  if (responseCode == 550 && DirExists(dir))
  {
    M5_LOGI();
    return FTP_RESCODE_ACTION_SUCCESS;
  }

  FTP_LOGINFO("Send MKD Recursive");
  std::vector<String> paths = SplitPath(dir);
  String currentPath = "";

  for (const String &subDir : paths)
  {
    currentPath += "/" + subDir;
    ftpClient.print(FTP_COMMAND_MAKE_DIR);
    ftpClient.println(currentPath);
    responseCode = GetCmdAnswer();
    M5_LOGI("%u , %s", responseCode, currentPath.c_str());

    if (isErrorCode(responseCode) && responseCode != 550)
    { // Ignore "Directory already exists" error
      M5_LOGI("%u", responseCode);
      return responseCode;
    }
  }
  M5_LOGI("%u", responseCode);
  return FTP_RESCODE_ACTION_SUCCESS;
}

String M5_Ethernet_FtpClient::GetDirectoryPath(const String &filePath)
{
  std::vector<String> paths = SplitPath(filePath);
  String directoryPath = "";
  for (size_t i = 0; i < paths.size() - 1; ++i)
  {
    directoryPath += paths[i];
    if (i < paths.size() - 2)
      directoryPath += "/";
  }
  return directoryPath;
}

std::vector<String> M5_Ethernet_FtpClient::SplitPath(const String &path)
{
  std::vector<String> paths;
  String tempPath = "";
  for (char c : path)
  {
    if (c == '/')
    {
      if (tempPath.length() > 0)
      {
        paths.push_back(tempPath);
      }
      tempPath = "";
    }
    else
    {
      tempPath += c;
    }
  }
  if (tempPath.length() > 0)
  {
    paths.push_back(tempPath);
  }
  return paths;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::RemoveDir(String dir)
{
  if (!isConnected())
  {
    FTP_LOGERROR("RemoveDir: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send RMD");
  ftpClient.print(FTP_COMMAND_REMOVE_DIR);
  ftpClient.println(dir);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendFile(String fileName)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  if (!isConnected())
  {
    FTP_LOGERROR("AppendFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send APPE");
  ftpClient.print(FTP_COMMAND_APPEND_FILE);
  ftpClient.println(fileName);
  M5_LOGI();
  responseCode = GetCmdAnswer();
  M5_LOGI();
  return responseCode;
}
/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendText(String filePath, String textLine)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  responseCode = InitAsciiPassiveMode();
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }
  responseCode = AppendFile(filePath);
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }
  responseCode = WriteData(textLine);
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }

  responseCode = CloseDataClient();
  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendTextLine(String filePath, String textLine)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  responseCode = InitAsciiPassiveMode();
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }
  responseCode = AppendFile(filePath);
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }
  responseCode = WriteData(textLine + "\r\n");
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return responseCode;
  }

  responseCode = CloseDataClient();
  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendDataArrayAsTextLine(String filePath, String headLine, u_int16_t *buf, int32_t len)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  responseCode = InitAsciiPassiveMode();
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return CloseDataClient();
  }
  responseCode = AppendFile(filePath);
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return CloseDataClient();
  }

  WriteData(headLine);

  for (int32_t i = 0; i < len; i++)
  {
    WriteData("," + String(buf[i]));
  }
  responseCode = WriteData("\r\n");
  if (isErrorCode(responseCode))
  {
    M5_LOGE("FTP responce code : %u", responseCode);
    return CloseDataClient();
  }

  responseCode = CloseDataClient();
  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendData(String filePath, unsigned char *data, int datalength)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;

  responseCode = InitAsciiPassiveMode();
  if (isErrorCode(responseCode))
    return responseCode;

  responseCode = AppendFile(filePath);
  if (isErrorCode(responseCode))
  {
    responseCode = CloseDataClient();
    return responseCode;
  }

  responseCode = WriteData(data, datalength);
  if (isErrorCode(responseCode))
    return responseCode;

  responseCode = CloseDataClient();
  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::WriteData(String data)
{
  return WriteData((unsigned char *)data.c_str(), data.length());
}

uint16_t M5_Ethernet_FtpClient::WriteData(unsigned char *data, int dataLength)
{
  if (!isConnected())
  {
    M5_LOGE("WriteData: Not connected error");
    FTP_LOGERROR("WriteData: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }
  FTP_LOGDEBUG1("WriteData: datalen =", dataLength);
  return WriteClientBuffered(&dclient, &data[0], dataLength);
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::WriteClientBuffered(EthernetClient *cli, unsigned char *data, int dataLength)
{
  if (!isConnected())
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;

  size_t clientCount = 0;
  for (int i = 0; i < dataLength; i++)
  {
    clientBuf[clientCount] = data[i];
    clientCount++;

    if (clientCount > bufferSize - 1)
    {
#if FTP_CLIENT_USING_QNETHERNET
      cli->writeFully(clientBuf, bufferSize);
#else
      cli->write(clientBuf, bufferSize);
#endif
      FTP_LOGDEBUG3("Written: num bytes =", bufferSize, ", index =", i);
      FTP_LOGDEBUG3("Written: clientBuf =", (uint32_t)clientBuf, ", clientCount =", clientCount);
      clientCount = 0;
    }
  }

  if (clientCount > 0)
  {
    cli->write(clientBuf, clientCount);
    FTP_LOGDEBUG1("Last Written: num bytes =", clientCount);
  }
  return FTP_RESCODE_ACTION_SUCCESS;
}

/////////////////////////////////////////////
uint16_t M5_Ethernet_FtpClient::DownloadString(const char *filename, String &str)
{
  FTP_LOGINFO("Send RETR");

  if (!isConnected())
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;

  ftpClient.print(FTP_COMMAND_DOWNLOAD);
  ftpClient.println(filename);

  char _resp[sizeof(outBuf)];
  uint16_t responseCode = GetCmdAnswer(_resp);

  unsigned long _m = millis();

  while (!GetDataClient()->available() && millis() < _m + timeout)
    delay(1);

  while (GetDataClient()->available())
    str += GetDataClient()->readString();

  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::DownloadFile(const char *filename, unsigned char *buf, size_t length, bool printUART)
{
  FTP_LOGINFO("Send RETR");

  if (!isConnected())
  {
    FTP_LOGERROR("DownloadFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  ftpClient.print(FTP_COMMAND_DOWNLOAD);
  ftpClient.println(filename);

  char _resp[sizeof(outBuf)];
  uint16_t responseCode = GetCmdAnswer(_resp);

  char _buf[2];

  unsigned long _m = millis();

  while (!dclient.available() && millis() < _m + timeout)
    delay(1);

  while (dclient.available())
  {
    if (!printUART)
      dclient.readBytes(buf, length);
    else
    {
      for (size_t _b = 0; _b < length; _b++)
      {
        dclient.readBytes(_buf, 1);
        FTP_LOGDEBUG0(_buf[0]);
      }
    }
  }

  return responseCode;
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::DeleteFile(String file)
{
  if (!isConnected())
  {
    FTP_LOGERROR("DeleteFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send DELE");
  ftpClient.print(FTP_COMMAND_DELETE_FILE);
  ftpClient.println(file);

  return GetCmdAnswer();
}
