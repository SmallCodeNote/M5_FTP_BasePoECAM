#include <M5Unified.h>
#include <M5_Ethernet.h>
#include "main.h"

#ifndef MAIN_HTTP_UI_H
#define MAIN_HTTP_UI_H

#define HTTP_GET_PARAM_FROM_POST(paramName)                                              \
  {                                                                                      \
    int start##paramName = currentLine.indexOf(#paramName "=") + strlen(#paramName "="); \
    int end##paramName = currentLine.indexOf("&", start##paramName);                     \
    if (end##paramName == -1)                                                            \
    {                                                                                    \
      end##paramName = currentLine.length();                                             \
    }                                                                                    \
    paramName = currentLine.substring(start##paramName, end##paramName);                 \
  }

#define HTML_PUT_INFOWITHLABEL(labelString) \
  httpClient.print(#labelString ": ");          \
  httpClient.print(labelString);                \
  httpClient.println("<br />");

#define HTML_PUT_LI_INPUT(inputName)                                                             \
  {                                                                                              \
    httpClient.println("<li>");                                                                      \
    httpClient.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    httpClient.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    httpClient.print(inputName);                                                                     \
    httpClient.println("\" size=\"4\" required>");                                                   \
    httpClient.println("</li>");                                                                     \
  }
#define HTML_PUT_LI_WIDEINPUT(inputName)                                                         \
  {                                                                                              \
    httpClient.println("<li>");                                                                      \
    httpClient.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    httpClient.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    httpClient.print(inputName);                                                                     \
    httpClient.println("\" required>");                                                              \
    httpClient.println("</li>");                                                                     \
  }
#define HTML_PUT_LI_INPUT_WITH_COMMENT(inputName, comment)                                       \
  {                                                                                              \
    httpClient.println("<li>");                                                                      \
    httpClient.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    httpClient.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    httpClient.print(inputName);                                                                     \
    httpClient.println("\" size=\"4\" required>");                                                   \
    httpClient.printf("[%s]", comment);                                                              \
    httpClient.println("</li>");                                                                     \
  }
// used to image stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

typedef enum
{
  HTTP_UI_MODE_GET,
  HTTP_UI_MODE_POST
} HTTP_UI_MODE_t;

struct PageHandler
{
  HTTP_UI_MODE_t mode; // GET:0,POST:1
  const char *page;
  void (*handler)(EthernetClient);
};

struct HTTP_UI_JPEG_STORE_TaskArgs
{
  uint8_t **HTTP_UI_JPEG_buf;
  int32_t *HTTP_UI_JPEG_len;
  u_int32_t fb_width;
  u_int32_t fb_height;
  pixformat_t pixmode;
};

extern EthernetServer HttpUIServer;
extern String SensorValueString;

void HTTP_UI();
void HTTP_UI_PART_ResponceHeader(EthernetClient httpClient, String Content_Type);
void HTTP_UI_PART_HTMLHeader(EthernetClient httpClient);
void HTTP_UI_PART_HTMLFooter(EthernetClient httpClient);

void HTTP_UI_JSON_sensorValueNow(EthernetClient httpClient);
void HTTP_UI_JSON_unitTimeNow(EthernetClient httpClient);

void HTTP_UI_JSON_cameraLineNow(EthernetClient httpClient);
uint16_t HTTP_UI_JSON_cameraLineNow_EdgePosition(uint8_t *bitmap_buf, HTTP_UI_JPEG_STORE_TaskArgs taskArgs);

void HTTP_UI_JPEG_sensorImageNow(EthernetClient httpClient);
void HTTP_UI_JPEG_flashTestImage(EthernetClient httpClient);
void HTTP_UI_STREAM_JPEG(EthernetClient httpClient);

void HTTP_UI_PAGE_top(EthernetClient httpClient);
void HTTP_UI_PAGE_view(EthernetClient httpClient);
void HTTP_UI_PAGE_cameraLineView(EthernetClient httpClient);

void HTTP_UI_PAGE_chart(EthernetClient httpClient);
void HTTP_UI_PAGE_notFound(EthernetClient httpClient);

void HTTP_UI_PAGE_configParam(EthernetClient httpClient);
void HTTP_UI_POST_configParam(EthernetClient httpClient);

void HTTP_UI_PAGE_configCamera(EthernetClient httpClient);
void HTTP_UI_POST_configCamera(EthernetClient httpClient);

void HTTP_UI_PAGE_configChart(EthernetClient httpClient);
void HTTP_UI_POST_configChart(EthernetClient httpClient);

void HTTP_UI_PAGE_configTime(EthernetClient httpClient);
void HTTP_UI_POST_configTime(EthernetClient httpClient);

void HTTP_UI_PAGE_unitTime(EthernetClient httpClient);
void HTTP_UI_POST_configTime(EthernetClient httpClient);

String urlDecode(String input);
void TaskRestart(void *arg);


void HTTP_UI_JPEG_STORE_Task(void *arg);

#endif