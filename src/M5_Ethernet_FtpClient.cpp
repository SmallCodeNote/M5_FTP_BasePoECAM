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

bool M5_Ethernet_FtpClient::isErrorCode(uint16_t responseCode)
{
  return responseCode >= 400 && responseCode < 600;
}

/**
 * @brief Open command connection
 */
uint16_t M5_Ethernet_FtpClient::OpenConnection()
{
  int responceCode = FTP_RESCODE_ACTION_SUCCESS;
  FTP_LOGINFO1(F("Connecting to: "), serverAdress);

#if ((ESP32) && !FTP_CLIENT_USING_ETHERNET)
  if (client.connect(serverAdress, port, timeout))
#else
  if (client.connect(serverAdress.c_str(), port))
#endif
  {
    FTP_LOGINFO(F("Command connected"));
  }

  responceCode = GetCmdAnswer();
  if (isErrorCode(responceCode))
    return responceCode;

  FTP_LOGINFO1("Send USER =", userName);
  client.print(FTP_COMMAND_USER);
  client.println(userName);

  responceCode = GetCmdAnswer();
  if (isErrorCode(responceCode))
    return responceCode;

  FTP_LOGINFO1("Send PASSWORD =", passWord);
  client.print(FTP_COMMAND_PASS);
  client.println(passWord);
  return GetCmdAnswer();
}

/**
 * @brief Close command connection
 */
void M5_Ethernet_FtpClient::CloseConnection()
{
  client.println(FTP_COMMAND_QUIT);
  client.stop();
  FTP_LOGINFO(F("Connection closed"));
}

/**
 * @brief Retrieves and processes the response from the FTP server, updating the connection status and storing the result.
 */
uint16_t M5_Ethernet_FtpClient::GetCmdAnswer(char *result, int offsetStart)
{
  char thisByte;
  outCount = 0;
  unsigned long _m = millis();

  while (!client.available() && millis() < _m + timeout)
  {
    delay(100);
  }

  if (!client.available())
  {
    memset(outBuf, 0, sizeof(outBuf));
    strcpy(outBuf, "Offline");
    _isConnected = false;
    isConnected();
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  while (client.available())
  {
    thisByte = client.read();
    if (outCount < sizeof(outBuf))
    {
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  uint16_t responseCode = atoi(outBuf);
  if (isErrorCode(responseCode))
  {
    _isConnected = false;
    isConnected();
    return responseCode;
  }
  else
  {
    _isConnected = true;
  }

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
  client.println("TYPE A"); // Set ASCII mode
  responseCode = GetCmdAnswer();

  if (isErrorCode(responseCode))
    return responseCode;

  FTP_LOGINFO("Send PASV");
  client.println(FTP_COMMAND_PASSIVE_MODE);

  responseCode = GetCmdAnswer();
  if (isErrorCode(responseCode))
    return responseCode;

  char *tmpPtr;
  while (strtol(outBuf, &tmpPtr, 10) != FTP_ENTERING_PASSIVE_MODE)
  {
    client.println(FTP_COMMAND_PASSIVE_MODE);

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
  client.print(FTP_COMMAND_LIST_DIR_STANDARD);
  client.println(dir);

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
  client.print(FTP_COMMAND_LIST_DIR);
  client.println(dir);

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
  client.print(FTP_COMMAND_FILE_LAST_MOD_TIME);
  client.println(fileName);
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
  client.print(FTP_COMMAND_RENAME_FILE_FROM);
  client.println(from);

  uint16_t responseCode = GetCmdAnswer();
  if (isErrorCode(responseCode))
    return responseCode;

  FTP_LOGINFO("Send RNTO");
  client.print(FTP_COMMAND_RENAME_FILE_TO);
  client.println(to);
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
  client.print(FTP_COMMAND_FILE_UPLOAD);
  client.println(fileName);
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
  client.print(FTP_COMMAND_CURRENT_WORKING_DIR);
  client.println(dir);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::MakeDir(String dir)
{
  if (!isConnected())
  {
    FTP_LOGERROR("MakeDir: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send MKD");
  client.print(FTP_COMMAND_MAKE_DIR);
  client.println(dir);
  return GetCmdAnswer();
}

uint16_t M5_Ethernet_FtpClient::MakeDirRecursive(String dir)
{
  if (!isConnected())
  {
    FTP_LOGERROR("MakeDirRecursive: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send MKD Recursive");

  std::vector<String> paths = SplitPath(dir);
  String currentPath = "";

  for (const String &subDir : paths)
  {
    currentPath += "/" + subDir;
    client.print(FTP_COMMAND_MAKE_DIR);
    client.println(currentPath);
    uint16_t res = GetCmdAnswer();
    if (isErrorCode(res) && res != 550)
    { // Ignore "Directory already exists" error
      return res;
    }
  }
  return FTP_RESCODE_ACTION_SUCCESS;
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
  client.print(FTP_COMMAND_REMOVE_DIR);
  client.println(dir);
  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendFile(String fileName)
{
  if (!isConnected())
  {
    FTP_LOGERROR("AppendFile: Not connected error");
    return FTP_RESCODE_CLIENT_ISNOT_CONNECTED;
  }

  FTP_LOGINFO("Send APPE");
  client.print(FTP_COMMAND_APPEND_FILE);
  client.println(fileName);

  return GetCmdAnswer();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendTextLine(String filePath, String textLine)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  if (isErrorCode(InitAsciiPassiveMode()))
    return responseCode;

  if (isErrorCode(AppendFile(filePath)))
    return responseCode;

  if (isErrorCode(WriteData(textLine + "\r\n")))
    return responseCode;

  return CloseDataClient();
}

/////////////////////////////////////////////

uint16_t M5_Ethernet_FtpClient::AppendData(String filePath, unsigned char *data, int datalength)
{
  uint16_t responseCode = FTP_RESCODE_ACTION_SUCCESS;
  if (isErrorCode(InitAsciiPassiveMode()))
    return responseCode;

  if (isErrorCode(AppendFile(filePath)))
    return responseCode;

  if (isErrorCode(WriteData(data, datalength)))
    return responseCode;

  return CloseDataClient();
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

  client.print(FTP_COMMAND_DOWNLOAD);
  client.println(filename);

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

  client.print(FTP_COMMAND_DOWNLOAD);
  client.println(filename);

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
  client.print(FTP_COMMAND_DELETE_FILE);
  client.println(file);

  return GetCmdAnswer();
}
