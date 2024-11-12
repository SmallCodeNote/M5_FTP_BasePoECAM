#include "main_EEPROM_handler.h"
#include <M5_Ethernet.h>

#ifndef MAIN_HTTP_UI_H
#define MAIN_HTTP_UI_H

EthernetServer HttpServer(80);

#define HTTP_GET_PARAM_FROM_POST(paramName)                                              \
  {                                                                                      \
    int start##paramName = currentLine.indexOf(#paramName "=") + strlen(#paramName "="); \
    int end##paramName = currentLine.indexOf("&", start##paramName);                     \
    if (end##paramName == -1)                                                            \
    {                                                                                    \
      end##paramName = currentLine.length();                                             \
    }                                                                                    \
    paramName = currentLine.substring(start##paramName, end##paramName);                 \
    Serial.println(#paramName ": " + paramName);                                         \
  }

#define HTML_PUT_INFOWITHLABEL(labelString) \
  client.print(#labelString ": ");          \
  client.print(labelString);                \
  client.println("<br />");

#define HTML_PUT_LI_INPUT(inputName)                                                             \
  {                                                                                              \
    client.println("<li>");                                                                      \
    client.println("<label for=\"" #inputName "\">" #inputName "</label>");                      \
    client.print("<input type=\"text\" id=\"" #inputName "\" name=\"" #inputName "\" value=\""); \
    client.print(inputName);                                                                     \
    client.println("\" required>");                                                              \
    client.println("</li>");                                                                     \
  }

void HTTP_UI_LoadPost(EthernetClient *client)
{
  String currentLine = "";
  // Load post data
  while (client->available())
  {
    char c = client->read();
    if (c == '\n' && currentLine.length() == 0)
    {
      break;
    }
    currentLine += c;
  }

  HTTP_GET_PARAM_FROM_POST(deviceName);
  HTTP_GET_PARAM_FROM_POST(deviceIP_String);
  HTTP_GET_PARAM_FROM_POST(ntpSrvIP_String);
  HTTP_GET_PARAM_FROM_POST(ftpSrvIP_String);
  HTTP_GET_PARAM_FROM_POST(ftp_user);
  HTTP_GET_PARAM_FROM_POST(ftp_pass);
  HTTP_GET_PARAM_FROM_POST(shotInterval);

  PutEEPROM();
  delay(1000);
  ESP.restart();
}

void HTTP_UI_PrintConfigPage(EthernetClient client)
{
  String currentLine = "";
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<body>");
  client.println("<h1>PoE CAM Unit</h1>");
  client.println("<br />");

  client.println("<form action=\"/\" method=\"post\">");
  client.println("<ul>");

  HTML_PUT_LI_INPUT(deviceName);
  HTML_PUT_LI_INPUT(deviceIP_String);
  HTML_PUT_LI_INPUT(ntpSrvIP_String);
  HTML_PUT_LI_INPUT(ftpSrvIP_String);
  HTML_PUT_LI_INPUT(ftp_user);
  HTML_PUT_LI_INPUT(ftp_pass);
  HTML_PUT_LI_INPUT(shotInterval);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");

  client.println("</ul>");
  client.println("</form>");
  client.println("</body>");
  client.println("</html>");
}
// used to image stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static void jpegStream(EthernetClient *client, String *page)
{
  Serial.println("Image stream start");
  client->println("HTTP/1.1 200 OK");
  client->printf("Content-Type: %s\r\n", _STREAM_CONTENT_TYPE);
  client->println("Content-Disposition: inline; filename=capture.jpg");
  client->println("Access-Control-Allow-Origin: *");
  client->println();
  static int64_t last_frame = 0;
  if (!last_frame)
  {
    last_frame = esp_timer_get_time();
  }
  for (;;)
  {
    if (PoECAM.Camera.get())
    {
      PoECAM.setLed(true);
      Serial.printf("pic size: %d\n", PoECAM.Camera.fb->len);
      client->print(_STREAM_BOUNDARY);
      client->printf(_STREAM_PART, PoECAM.Camera.fb);
      int32_t to_sends = PoECAM.Camera.fb->len;
      int32_t now_sends = 0;
      uint8_t *out_buf = PoECAM.Camera.fb->buf;
      uint32_t packet_len = 1 * 1024;
      while (to_sends > 0)
      {
        now_sends = to_sends > packet_len ? packet_len : to_sends;
        if (client->write(out_buf, now_sends) == 0)
        {
          goto client_exit;
        }
        out_buf += now_sends;
        to_sends -= packet_len;
      }
      int64_t fr_end = esp_timer_get_time();
      int64_t frame_time = fr_end - last_frame;
      last_frame = fr_end;
      frame_time /= 1000;
      Serial.printf("MJPG: %luKB %lums (%.1ffps)\r\n", (long unsigned int)(PoECAM.Camera.fb->len / 1024), (long unsigned int)frame_time, 1000.0 / (long unsigned int)frame_time);
      PoECAM.Camera.free();
      PoECAM.setLed(false);
    }

    if(*page != "view.html")break;
  }
client_exit:
  PoECAM.Camera.free();
  PoECAM.setLed(0);
  client->stop();
  Serial.printf("Image stream end\r\n");
}

void sendPage(EthernetClient client, String page)
{
  Serial.println("page = " + page);

  if (page == "view.html")
  {
    jpegStream(&client, &page);
  }
  else if (page == "config.html")
  {
    HTTP_UI_PrintConfigPage(client);
  }
  else if (page == "top.html")
  {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<body>");
    client.println("<h1>PoECAM-W</h1>");
    client.println("<a href=\"/view.html\">View Page</a><br>");
    client.println("<a href=\"/config.html\">Config Page</a>");
    client.println("</body>");
    client.println("</html>");
  }
  else
  {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<body>");
    client.println("<h1>404 Not Found</h1>");
    client.println("</body>");
    client.println("</html>");
  }
}

void HTTP_UI()
{
  EthernetClient client = HttpServer.available();
  if (client)
  {
    Serial.println("new client");
    boolean currentLineIsBlank = true;
    String currentLine = "";
    bool isPost = false;
    bool getRequest = false;
    String page = "";

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          if (isPost)
          {
            HTTP_UI_LoadPost(&client);
          }

          if (getRequest)
          {
            sendPage(client, page);
            break;
          }
          else
          {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.println("<body>");
            client.println("<h1>404 Not Found</h1>");
            client.println("</body>");
            client.println("</html>");
            break;
          }

          break;
        }
        if (c == '\n')
        {
          currentLineIsBlank = true;
          currentLine = "";
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false;
          currentLine += c;
        }

        if (currentLine.startsWith("POST /"))
        {
          isPost = true;
        }
        // Check if the current line starts with "GET /view.html"
        if (currentLine.endsWith("GET /view.html"))
        {
          getRequest = true;
          page = "view.html";
        }
        else if (currentLine.endsWith("GET /config.html"))
        {
          getRequest = true;
          page = "config.html";
        }
        else if (currentLine.endsWith("GET /top.html") || currentLine.endsWith("GET /"))
        {
          getRequest = true;
          page = "top.html";
        }
      }
    }
    delay(1);
    client.stop();
    Serial.println("client disconnected");
  }
}

#endif