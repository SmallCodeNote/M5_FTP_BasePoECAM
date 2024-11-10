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

void HTTP_UI()
{
  EthernetClient client = HttpServer.available();
  if (client)
  {
    Serial.println("new client");
    boolean currentLineIsBlank = true;
    String currentLine = "";
    bool isPost = false;

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
            // Load post data
            while (client.available())
            {
              char c = client.read();
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
      }
    }
    delay(1);
    client.stop();
    Serial.println("client disconnected");
  }
}

#endif