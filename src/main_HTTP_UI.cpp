#include <M5Unified.h>
#include <M5_Ethernet.h>
#include "M5_Ethernet_NtpClient.h"

#include "main.h"
#include "main_HTTP_UI.h"
#include "main_HTTP_UI_ChartJS.h"
#include "main_EEPROM_handler.h"

EthernetServer HttpUIServer(80);
String SensorValueString = "";

void HTTP_UI_PART_ResponceHeader(EthernetClient client, String Content_Type)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + Content_Type);
  client.println("Connection: close");
  client.println();
}

void HTTP_UI_PART_HTMLHeader(EthernetClient client)
{
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<body>");
}

void HTTP_UI_PART_HTMLFooter(EthernetClient client)
{
  client.println("</body>");
  client.println("</html>");
}

void HTTP_UI_JSON_sensorValueNow(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "application/json");
  client.print("{");
  client.print("\"distance\":");
  client.print(String(SensorValueString.toInt()));
  client.println("}");
}

static void HTTP_UI_STREAM_JPEG(EthernetClient client, String page)
{
  M5_LOGI("Image stream start");

  client.println("HTTP/1.1 200 OK");
  client.printf("Content-Type: %s\r\n", _STREAM_CONTENT_TYPE);
  client.println("Content-Disposition: inline; filename=capture.jpg");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  static int64_t last_frame = 0;
  if (!last_frame)
  {
    last_frame = esp_timer_get_time();
  }
  while (true)
  {
    if (PoECAM.Camera.get())
    {
      PoECAM.setLed(true);
      M5_LOGI("pic size: %d\n", PoECAM.Camera.fb->len);
      client.print(_STREAM_BOUNDARY);
      client.printf(_STREAM_PART, PoECAM.Camera.fb);
      int32_t to_sends = PoECAM.Camera.fb->len;
      int32_t now_sends = 0;
      uint8_t *out_buf = PoECAM.Camera.fb->buf;
      uint32_t packet_len = 1 * 1024;
      while (to_sends > 0)
      {
        now_sends = to_sends > packet_len ? packet_len : to_sends;
        if (client.write(out_buf, now_sends) == 0)
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
      M5_LOGI("MJPG: %luKB %lums (%.1ffps)\r\n", (long unsigned int)(PoECAM.Camera.fb->len / 1024), (long unsigned int)frame_time, 1000.0 / (long unsigned int)frame_time);
      PoECAM.Camera.free();
      PoECAM.setLed(false);
    }

    if (page != "view.html")
      break;
  }
client_exit:
  PoECAM.Camera.free();
  PoECAM.setLed(0);
  client.stop();
  M5_LOGI("Image stream end\r\n");
}

void HTTP_UI_PAGE_view(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<ul id=\"sensorData\">");
  client.println("<li>Distance: <span id=\"distance\"></span> mm</li>");
  client.println("</ul>");

  client.println("<script>");
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('distance').innerText = data.distance;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/sensorValueNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("setInterval(fetchData, 1000);");
  client.println("fetchData();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_chart(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<ul id=\"sensorData\">");
  client.println("<li>Distance: <span id=\"distance\"></span> mm</li>");
  client.println("</ul>");

  client.println("<canvas id=\"distanceChart\" width=\"400\" height=\"200\"></canvas>");

  client.println("<script src=\"/chart.js\"></script>");
  client.println("<script>");
  client.printf("var distanceData = Array(%s).fill(0);\n", chartShowPointCount.c_str());
  client.println("var myChart = null;");
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('distance').innerText = data.distance;");
  client.println("      distanceData.push(data.distance);");
  client.printf("      if (distanceData.length > %s) { distanceData.shift(); }", chartShowPointCount.c_str());
  client.println("      updateChart();");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/sensorValueNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("function updateChart() {");
  client.println("  var ctx = document.getElementById('distanceChart').getContext('2d');");

  client.println("  if (myChart) {");
  client.println("    myChart.destroy();");
  client.println("  }");

  client.println("  myChart = new Chart(ctx, {");
  client.println("    type: 'line',");
  client.println("    data: {");
  client.println("      labels: Array.from({length: distanceData.length}, (_, i) => i + 1),");
  client.println("      datasets: [{");
  client.println("        label: 'Distance',");
  client.println("        data: distanceData,");
  client.println("        borderColor: 'rgba(75, 192, 192, 1)',");
  client.println("        borderWidth: 1");
  client.println("      }]");
  client.println("    },");
  client.println("    options: {");
  client.println("      animation: false,");
  client.println("      scales: {");
  client.println("        x: { beginAtZero: true },");
  client.println("        y: { beginAtZero: true }");
  client.println("      }");
  client.println("    }");
  client.println("  });");
  client.println("}");
  client.printf("setInterval(fetchData, %s);", chartUpdateInterval.c_str());
  client.println("fetchData();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_configParam(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/configParamSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  HTML_PUT_LI_INPUT(deviceName);
  HTML_PUT_LI_INPUT(deviceIP_String);
  HTML_PUT_LI_INPUT(ntpSrvIP_String);
  HTML_PUT_LI_INPUT(ftpSrvIP_String);
  HTML_PUT_LI_INPUT(ftp_user);
  HTML_PUT_LI_INPUT(ftp_pass);
  HTML_PUT_LI_INPUT(ftpSaveInterval);
  HTML_PUT_LI_INPUT(chartShowPointCount);
  HTML_PUT_LI_INPUT(chartUpdateInterval);
  HTML_PUT_LI_INPUT(timeZoneOffset);
  HTML_PUT_LI_INPUT(flashIntensityMode);
  HTML_PUT_LI_INPUT(flashLength);


  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_POST_configParam(EthernetClient client)
{
  String currentLine = "";
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
  HTTP_GET_PARAM_FROM_POST(ftpSaveInterval);
  HTTP_GET_PARAM_FROM_POST(chartShowPointCount);
  HTTP_GET_PARAM_FROM_POST(chartUpdateInterval);
  HTTP_GET_PARAM_FROM_POST(timeZoneOffset);
  HTTP_GET_PARAM_FROM_POST(flashIntensityMode);
  HTTP_GET_PARAM_FROM_POST(flashLength);

  PutEEPROM();

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);
  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");
  client.println("SUCCESS PARAMETER UPDATE.");
  HTTP_UI_PART_HTMLFooter(client);

  delay(1000);
  ESP.restart();
}

void HTTP_UI_PAGE_configCamera(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/configCameraSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  HTML_PUT_LI_INPUT(flashIntensityMode);
  HTML_PUT_LI_INPUT(flashLength);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_POST_configCamera(EthernetClient client)
{
  String currentLine = "";
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

  HTTP_GET_PARAM_FROM_POST(flashIntensityMode);
  HTTP_GET_PARAM_FROM_POST(flashLength);

  PutEEPROM();

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);
  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");
  client.println("SUCCESS PARAMETER UPDATE.");
  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_configTime(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/configTimeSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  String timeString = "";
  HTML_PUT_LI_INPUT(timeString);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<script>");
  client.println("function updateTimeString() {");
  client.println(" document.getElementById('timeString').value = new Date().toLocaleString();");
  client.println("}");
  client.println("setInterval(updateTimeString, 1000);");
  client.println("updateTimeString();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
}

String urlDecode(String input)
{
  String decoded = "";
  char temp[] = "0x00";
  unsigned int i, j;
  for (i = 0; i < input.length(); i++)
  {
    if (input[i] == '%')
    {
      temp[2] = input[i + 1];
      temp[3] = input[i + 2];
      decoded += (char)strtol(temp, NULL, 16);
      i += 2;
    }
    else if (input[i] == '+')
    {
      decoded += ' ';
    }
    else
    {
      decoded += input[i];
    }
  }
  return decoded;
}

void HTTP_UI_POST_configTime(EthernetClient client)
{
  String currentLine = "";
  String timeString = "";
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
  HTTP_GET_PARAM_FROM_POST(timeString);

  timeString = urlDecode(timeString);
  M5_LOGI("posted timeString = %s", timeString.c_str());
  NtpClient.updateTimeFromString(timeString);

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);
  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");
  client.println("SUCCESS TIME UPDATE.");
  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_top(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<a href=\"/view.html\">View Page</a><br>");
  client.println("<a href=\"/chart.html\">Chart Page</a><br>");
  client.println("<a href=\"/configParam.html\">Config Parameter Page</a><br>");
  client.println("<a href=\"/configCamera.html\">Config Camera Page</a><br>");
  client.println("<a href=\"/configTime.html\">Config Time Page</a><br>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_notFound(EthernetClient client)
{
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>404 Not Found</h1>");

  HTTP_UI_PART_HTMLFooter(client);
}

void sendPage(EthernetClient client, String page)
{
  M5_LOGV("page = %s", page.c_str());

  if (page == "sensorValueNow.json")
  {
    HTTP_UI_JSON_sensorValueNow(client);
  }
  else if (page == "view.html")
  {
    // HTTP_UI_PAGE_view(client);
    HTTP_UI_STREAM_JPEG(client, page);
  }
  else if (page == "chart.js")
  {
    HTTP_UI_JS_ChartJS(client);
  }
  else if (page == "chart.html")
  {
    HTTP_UI_PAGE_chart(client);
  }
  else if (page == "configParam.html")
  {
    HTTP_UI_PAGE_configParam(client);
  }
  else if (page == "configParamSuccess.html")
  {
    HTTP_UI_POST_configParam(client);
  }
  else if (page == "configCamera.html")
  {
    HTTP_UI_PAGE_configCamera(client);
  }
  else if (page == "configCameraSuccess.html")
  {
    HTTP_UI_POST_configCamera(client);
  }
  else if (page == "configTime.html")
  {
    HTTP_UI_PAGE_configTime(client);
  }
  else if (page == "configTimeSuccess.html")
  {
    HTTP_UI_POST_configTime(client);
  }
  else if (page == "top.html")
  {
    HTTP_UI_PAGE_top(client);
  }
  else
  {
    HTTP_UI_PAGE_notFound(client);
  }
}

void HTTP_UI()
{
  EthernetClient client = HttpUIServer.available();
  if (client)
  {
    M5_LOGV("new client");

    boolean currentLineIsBlank = true;
    String currentLine = "";
    String page = "";

    bool getRequest = false;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        M5.Log.printf("%c", c); // Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          M5_LOGI("%s", currentLine);

          if (getRequest)
          {
            sendPage(client, page);
            break;
          }
          else
          {
            HTTP_UI_PAGE_notFound(client);
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

        if (currentLine.endsWith("GET /sensorValueNow.json"))
        {
          getRequest = true;
          page = "sensorValueNow.json";
        }
        else if (currentLine.endsWith("GET /chart.js"))
        {
          getRequest = true;
          page = "chart.js";
        }
        else if (currentLine.endsWith("GET /view.html"))
        {
          getRequest = true;
          page = "view.html";
        }
        else if (currentLine.endsWith("GET /chart.html"))
        {
          getRequest = true;
          page = "chart.html";
        }
        else if (currentLine.endsWith("GET /configParam.html"))
        {
          getRequest = true;
          page = "configParam.html";
        }
        else if (currentLine.endsWith("GET /configCamera.html"))
        {
          getRequest = true;
          page = "configCamera.html";
        }
        else if (currentLine.endsWith("GET /configTime.html"))
        {
          getRequest = true;
          page = "configTime.html";
        }
        else if (currentLine.endsWith("GET /top.html") || currentLine.endsWith("GET /"))
        {
          getRequest = true;
          page = "top.html";
        }
        else if (currentLine.startsWith("POST /configParamSuccess.html"))
        {
          getRequest = true;
          page = "configParamSuccess.html";
        }
        else if (currentLine.startsWith("POST /configCameraSuccess.html"))
        {
          getRequest = true;
          page = "configCameraSuccess.html";
        }
        else if (currentLine.startsWith("POST /configTimeSuccess.html"))
        {
          getRequest = true;
          page = "configTimeSuccess.html";
        }
      }
    }
    delay(1);
    client.stop();
    M5_LOGV("client disconnected");
  }
}