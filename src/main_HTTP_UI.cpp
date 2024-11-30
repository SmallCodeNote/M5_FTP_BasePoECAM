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

void HTTP_UI_JSON_unitTimeNow(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "application/json");
  client.print("{");
  client.print("\"unitTime\":");
  client.printf("\"%s\"", NtpClient.convertTimeEpochToString().c_str());
  client.println("}");
}

void HTTP_UI_JSON_cameraLineNow(EthernetClient client)
{
  CameraSensorSetJPEG();
  M5_LOGD("");
  if (PoECAM.Camera.get())
  {
    PoECAM.setLed(true);

    int32_t to_sends = PoECAM.Camera.fb->len;
    uint8_t *out_buf = PoECAM.Camera.fb->buf;
    u_int16_t c_width = PoECAM.Camera.fb->width;
    u_int16_t c_height = PoECAM.Camera.fb->height;
    pixformat_t pixmode = PoECAM.Camera.fb->format;
    M5_LOGI("Sending cameraLineNow %u, %u, %u", c_width, c_height, pixmode);
    M5_LOGI("to_sends = %d ", to_sends);

    uint8_t *map_buf = (uint8_t *)ps_malloc(3 * c_width * c_height);

    fmt2rgb888(out_buf, to_sends, pixmode, map_buf);

    PoECAM.Camera.free();

    u_int32_t offset = c_width * c_height / 2u * 3;
    u_int32_t lineEnd = offset + c_width * 3;

    HTTP_UI_PART_ResponceHeader(client, "application/json");
    client.print("{");
    client.print("\"unitTime\":");
    client.printf("\"%s\"", NtpClient.convertTimeEpochToString().c_str());
    client.println(",");

    client.print("");
    client.print("\"CameraLineValue\":[");
    client.printf("%u", *(map_buf + offset));
    for (u_int32_t i = offset + 1; i < lineEnd; i += 3)
    {
      client.printf(",%u", *(map_buf + i));
    }
    client.println("]}");

    free(map_buf);
  }
  M5_LOGD("");
}

void HTTP_UI_JPEG_sensorImageNow(EthernetClient client)
{
  M5_LOGI("Sending JPEG image");
  CameraSensorSetJPEG();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
  client.println("Access-Control-Allow-Origin: *");
  client.println();

  if (PoECAM.Camera.get())
  {
    PoECAM.setLed(true);
    int32_t to_sends = PoECAM.Camera.fb->len;
    int32_t now_sends = 0;
    uint8_t *out_buf = PoECAM.Camera.fb->buf;
    uint32_t packet_len = 1 * 1024; // 1KBごとのパケットに分割して送信

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (client.write(out_buf, now_sends) == 0)
      {
        goto client_exit;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
    }

    M5_LOGI("JPEG sent successfully");
    PoECAM.Camera.free();
    PoECAM.setLed(false);
  }

client_exit:
  PoECAM.Camera.free();
  PoECAM.setLed(0);
  client.stop();
  M5_LOGI("JPEG transmission end");
}

void HTTP_UI_STREAM_JPEG(EthernetClient client)
{
  M5_LOGI("Image stream start");
  CameraSensorSetJPEG();

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
    delay(10);
  }
client_exit:
  PoECAM.Camera.free();
  PoECAM.setLed(0);
  client.stop();
  M5_LOGI("Image stream end\r\n");
}

void HTTP_UI_PAGE_top(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<a href=\"/capture.jpg\">Stream</a><br>");
  client.println("<a href=\"/view.html\">View Page</a><br>");
  client.println("<a href=\"/chart.html\">Chart Page</a><br>");
  client.println("<a href=\"/cameraLineView.html\">CameraLineView Page</a><br>");
  client.println("<a href=\"/unitTime.html\">Unit Time</a><br>");

  client.println("<br>");
  client.println("<hr>");
  client.println("<br>");

  client.println("<a href=\"/configParam.html\">Config Parameter Page</a><br>");
  client.println("<a href=\"/configCamera.html\">Config Camera Page</a><br>");
  client.println("<a href=\"/configChart.html\">Config Chart Page</a><br>");
  client.println("<a href=\"/configTime.html\">Config Time Page</a><br>");

  HTTP_UI_PART_HTMLFooter(client);
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

  client.println("<img id=\"sensorImage\" src=\"/sensorImageNow.jpg\" alt=\"Sensor Image\" />");

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
  client.println("function refreshImage() {");
  client.println("  var img = document.getElementById('sensorImage');");
  client.println("  img.src = '/sensorImageNow.jpg?' + new Date().getTime();"); // add timestamp
  client.println("}");
  client.printf("setInterval(fetchData, %u);", storeData.chartUpdateInterval);
  client.printf("setInterval(refreshImage, %u);", storeData.chartUpdateInterval);
  client.println("fetchData();");
  client.println("refreshImage();");
  client.println("</script>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_cameraLineView(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>Camera Line View</h1>");

  client.println("<ul id=\"unitTime\">");
  client.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  client.println("</ul>");

  client.println("<canvas id=\"cameraLineChart\" width=\"400\" height=\"50\"></canvas>");

  client.println("<script src=\"/chart.js\"></script>");
  client.println("<script>");

  client.println("var chart = null;");

  client.println("function fetchCameraLineData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      updateChart(data.CameraLineValue);");
  client.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/cameraLineNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");

  client.println("function updateChart(data) {");
  client.println("  var ctx = document.getElementById('cameraLineChart').getContext('2d');");

  client.println("  if (chart) {");
  client.println("    chart.destroy();");
  client.println("  }");

  client.println("  chart = new Chart(ctx, {");
  client.println("    type: 'line',");
  client.println("    data: {");
  client.println("      labels: data.map((_, index) => index),");
  client.println("      datasets: [{");
  client.println("        label: 'Camera Line Data',");
  client.println("        data: data,");
  client.println("        borderColor: 'rgba(75, 192, 192, 1)',");
  client.println("        borderWidth: 1,");
  client.println("        fill: false");
  client.println("      }]");
  client.println("    },");
  client.println("    options: {");
  client.println("      animation: false,");
  client.println("      scales: {");
  client.println("        x: {");
  client.println("          type: 'linear',");
  client.println("          position: 'bottom'");
  client.println("        }");
  client.println("      }");
  client.println("    }");
  client.println("  });");
  client.println("}");

  client.printf("setInterval(fetchCameraLineData, %s);", chartUpdateInterval.c_str());

  client.println("fetchCameraLineData();");
  client.println("</script>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

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

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

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

void HTTP_UI_PAGE_configParam(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/configParamSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  HTML_PUT_LI_WIDEINPUT(deviceName);
  HTML_PUT_LI_WIDEINPUT(deviceIP_String);
  HTML_PUT_LI_WIDEINPUT(ntpSrvIP_String);
  HTML_PUT_LI_WIDEINPUT(ftpSrvIP_String);
  HTML_PUT_LI_WIDEINPUT(ftp_user);
  HTML_PUT_LI_WIDEINPUT(ftp_pass);
  HTML_PUT_LI_INPUT(ftpSaveInterval);
  HTML_PUT_LI_INPUT(chartShowPointCount);
  HTML_PUT_LI_INPUT(chartUpdateInterval);
  HTML_PUT_LI_INPUT(timeZoneOffset);
  HTML_PUT_LI_INPUT(flashIntensityMode);
  HTML_PUT_LI_INPUT(flashLength);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save and reboot</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

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

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(client);

  xTaskCreatePinnedToCore(TaskRestart, "TaskRestart", 4096, NULL, 0, NULL, 1);
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
  HTML_PUT_LI_INPUT_WITH_COMMENT(flashLength, "ms");

  String optionString = " selected";
  int pixformat_i = pixformat.toInt();

  client.println("<label for=\"pixformat\">Pixformat:</label>");
  client.println("<select id=\"pixformat\" name=\"pixformat\">");

  client.printf("<option value=\"%u\" %s>RGB565</option>", PIXFORMAT_RGB565, pixformat_i == PIXFORMAT_RGB565 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>YUV422</option>", PIXFORMAT_YUV422, pixformat_i == PIXFORMAT_YUV422 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>YUV420</option>", PIXFORMAT_YUV420, pixformat_i == PIXFORMAT_YUV420 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>Grayscale</option>", PIXFORMAT_GRAYSCALE, pixformat_i == PIXFORMAT_GRAYSCALE ? "selected" : "");
  client.printf("<option value=\"%u\" %s>JPEG</option>", PIXFORMAT_JPEG, pixformat_i == PIXFORMAT_JPEG ? "selected" : "");
  client.printf("<option value=\"%u\" %s>RGB888</option>", PIXFORMAT_RGB888, pixformat_i == PIXFORMAT_RGB888 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>RAW</option>", PIXFORMAT_RAW, pixformat_i == PIXFORMAT_RAW ? "selected" : "");
  client.printf("<option value=\"%u\" %s>RGB444</option>", PIXFORMAT_RGB444, pixformat_i == PIXFORMAT_RGB444 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>RGB555</option>", PIXFORMAT_RGB555, pixformat_i == PIXFORMAT_RGB555 ? "selected" : "");

  client.println("</select><br>");

  int framesize_i = framesize.toInt();

  client.println("<label for=\"framesize\">Framesize:</label>");
  client.println("<select id=\"framesize\" name=\"framesize\">");

  client.printf("<option value=\"%u\" %s>96x96</option>", FRAMESIZE_96X96, framesize_i == FRAMESIZE_96X96 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>160x120</option>", FRAMESIZE_QQVGA, framesize_i == FRAMESIZE_QQVGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>176x144</option>", FRAMESIZE_QCIF, framesize_i == FRAMESIZE_QCIF ? "selected" : "");
  client.printf("<option value=\"%u\" %s>240x176</option>", FRAMESIZE_HQVGA, framesize_i == FRAMESIZE_HQVGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>240x240</option>", FRAMESIZE_240X240, framesize_i == FRAMESIZE_240X240 ? "selected" : "");
  client.printf("<option value=\"%u\" %s>320x240</option>", FRAMESIZE_QVGA, framesize_i == FRAMESIZE_QVGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>400x296</option>", FRAMESIZE_CIF, framesize_i == FRAMESIZE_CIF ? "selected" : "");
  client.printf("<option value=\"%u\" %s>480x320</option>", FRAMESIZE_HVGA, framesize_i == FRAMESIZE_HVGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>640x480</option>", FRAMESIZE_VGA, framesize_i == FRAMESIZE_VGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>800x600</option>", FRAMESIZE_SVGA, framesize_i == FRAMESIZE_SVGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1024x768</option>", FRAMESIZE_XGA, framesize_i == FRAMESIZE_XGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1280x720</option>", FRAMESIZE_HD, framesize_i == FRAMESIZE_HD ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1280x1024</option>", FRAMESIZE_SXGA, framesize_i == FRAMESIZE_SXGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1600x1200</option>", FRAMESIZE_UXGA, framesize_i == FRAMESIZE_UXGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1920x1080</option>", FRAMESIZE_FHD, framesize_i == FRAMESIZE_FHD ? "selected" : "");
  client.printf("<option value=\"%u\" %s>720x1280</option>", FRAMESIZE_P_HD, framesize_i == FRAMESIZE_P_HD ? "selected" : "");
  client.printf("<option value=\"%u\" %s>864x1536</option>", FRAMESIZE_P_3MP, framesize_i == FRAMESIZE_P_3MP ? "selected" : "");
  client.printf("<option value=\"%u\" %s>2048x1536</option>", FRAMESIZE_QXGA, framesize_i == FRAMESIZE_QXGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>2560x1440</option>", FRAMESIZE_QHD, framesize_i == FRAMESIZE_QHD ? "selected" : "");
  client.printf("<option value=\"%u\" %s>2560x1600</option>", FRAMESIZE_WQXGA, framesize_i == FRAMESIZE_WQXGA ? "selected" : "");
  client.printf("<option value=\"%u\" %s>1080x1920</option>", FRAMESIZE_P_FHD, framesize_i == FRAMESIZE_P_FHD ? "selected" : "");
  client.printf("<option value=\"%u\" %s>2560x1920</option>", FRAMESIZE_QSXGA, framesize_i == FRAMESIZE_QSXGA ? "selected" : "");

  client.println("</select><br>");

  HTML_PUT_LI_INPUT_WITH_COMMENT(contrast, "-2 - 2");
  HTML_PUT_LI_INPUT_WITH_COMMENT(brightness, "-2 - 2");
  HTML_PUT_LI_INPUT_WITH_COMMENT(saturation, "-2 - 2");
  HTML_PUT_LI_INPUT_WITH_COMMENT(sharpness, "-2 - 2");
  HTML_PUT_LI_INPUT(denoise);
  HTML_PUT_LI_INPUT_WITH_COMMENT(quality, "0 - 63");
  HTML_PUT_LI_INPUT(colorbar);
  HTML_PUT_LI_INPUT(whitebal);
  HTML_PUT_LI_INPUT_WITH_COMMENT(gain_ctrl, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(exposure_ctrl, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(hmirror, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(vflip, "0 | 1");

  HTML_PUT_LI_INPUT_WITH_COMMENT(aec2, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(awb_gain, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(agc_gain, "0 - 30");
  HTML_PUT_LI_INPUT_WITH_COMMENT(aec_value, "0 - 1200");

  HTML_PUT_LI_INPUT_WITH_COMMENT(special_effect, "0 - 6");
  HTML_PUT_LI_INPUT_WITH_COMMENT(wb_mode, "0 - 4");
  HTML_PUT_LI_INPUT_WITH_COMMENT(ae_level, "-2 - 2");

  /*
    HTML_PUT_LI_INPUT_WITH_COMMENT(dcw, "0 | 1");
    HTML_PUT_LI_INPUT_WITH_COMMENT(bpc, "0 | 1");
    HTML_PUT_LI_INPUT_WITH_COMMENT(wpc, "0 | 1");

    HTML_PUT_LI_INPUT_WITH_COMMENT(raw_gma, "0 | 1");
    HTML_PUT_LI_INPUT_WITH_COMMENT(lenc, "0 | 1");

    HTML_PUT_LI_INPUT(get_reg);
    HTML_PUT_LI_INPUT(set_reg);
    HTML_PUT_LI_INPUT(set_res_raw);
    HTML_PUT_LI_INPUT(set_pll);
    HTML_PUT_LI_INPUT(set_xclk);
  */

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

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

  HTTP_GET_PARAM_FROM_POST(pixformat);
  HTTP_GET_PARAM_FROM_POST(framesize);
  HTTP_GET_PARAM_FROM_POST(vflip);
  HTTP_GET_PARAM_FROM_POST(hmirror);
  HTTP_GET_PARAM_FROM_POST(gain_ctrl);
  HTTP_GET_PARAM_FROM_POST(exposure_ctrl);
  HTTP_GET_PARAM_FROM_POST(denoise);

  HTTP_GET_PARAM_FROM_POST(contrast);
  HTTP_GET_PARAM_FROM_POST(brightness);
  HTTP_GET_PARAM_FROM_POST(saturation);
  HTTP_GET_PARAM_FROM_POST(sharpness);
  HTTP_GET_PARAM_FROM_POST(gainceiling);
  HTTP_GET_PARAM_FROM_POST(quality);
  HTTP_GET_PARAM_FROM_POST(colorbar);
  HTTP_GET_PARAM_FROM_POST(whitebal);
  HTTP_GET_PARAM_FROM_POST(aec2);
  HTTP_GET_PARAM_FROM_POST(awb_gain);
  HTTP_GET_PARAM_FROM_POST(agc_gain);
  HTTP_GET_PARAM_FROM_POST(aec_value);
  HTTP_GET_PARAM_FROM_POST(special_effect);
  HTTP_GET_PARAM_FROM_POST(wb_mode);
  HTTP_GET_PARAM_FROM_POST(ae_level);

  /*
  HTTP_GET_PARAM_FROM_POST(dcw);
  HTTP_GET_PARAM_FROM_POST(bpc);
  HTTP_GET_PARAM_FROM_POST(wpc);
  HTTP_GET_PARAM_FROM_POST(raw_gma);
  HTTP_GET_PARAM_FROM_POST(lenc);
  HTTP_GET_PARAM_FROM_POST(get_reg);
  HTTP_GET_PARAM_FROM_POST(set_reg);
  HTTP_GET_PARAM_FROM_POST(set_res_raw);
  HTTP_GET_PARAM_FROM_POST(set_pll);
  HTTP_GET_PARAM_FROM_POST(set_xclk);
*/

  PutEEPROM();
  CameraSensorFullSetupFromStoreData();

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);
  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");
  client.println("SUCCESS PARAMETER UPDATE.<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());
  client.printf("<a href=\"http://%s/configCamera.html\">Return Config Camera Page</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_configChart(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/configChartSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  HTML_PUT_LI_INPUT(chartShowPointCount);
  HTML_PUT_LI_INPUT(chartUpdateInterval);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_POST_configChart(EthernetClient client)
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

  HTTP_GET_PARAM_FROM_POST(chartShowPointCount);
  HTTP_GET_PARAM_FROM_POST(chartUpdateInterval);

  PutEEPROM();

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);
  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");
  client.println("SUCCESS PARAMETER UPDATE.");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_configTime(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<ul id=\"unitTime\">");
  client.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  client.println("</ul>");

  client.println("<form action=\"/configTimeSuccess.html\" method=\"post\">");
  client.println("<ul>");

  String currentLine = "";
  String timeString = "";
  HTML_PUT_LI_WIDEINPUT(timeString);

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Save</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  client.println("<script>");

  client.println("function updateTimeString() {");
  client.println(" document.getElementById('timeString').value = new Date().toLocaleString();");
  client.println("}");
  client.println("setInterval(updateTimeString, 1000);");

  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/unitTimeNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("setInterval(fetchData, 1000);");

  client.println("fetchData();");
  client.println("updateTimeString();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
}

void HTTP_UI_PAGE_unitTime(EthernetClient client)
{
  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<ul id=\"unitTime\">");
  client.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  client.println("</ul>");

  client.println("<br>");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  client.println("<script>");
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/unitTimeNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("setInterval(fetchData, 1000);");
  client.println("fetchData();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
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

  client.println("<ul id=\"unitTime\">");
  client.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  client.println("</ul>");

  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  client.println("<script>");
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/unitTimeNow.json', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("setInterval(fetchData, 1000);");
  client.println("fetchData();");
  client.println("</script>");

  HTTP_UI_PART_HTMLFooter(client);
  return;
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

void TaskRestart(void *arg)
{
  delay(5000);
  ESP.restart();
}

PageHandler pageHandlers[] = {
    {HTTP_UI_MODE_GET, "sensorValueNow.json", HTTP_UI_JSON_sensorValueNow},
    {HTTP_UI_MODE_GET, "unitTimeNow.json", HTTP_UI_JSON_unitTimeNow},
    {HTTP_UI_MODE_GET, "cameraLineNow.json", HTTP_UI_JSON_cameraLineNow},
    {HTTP_UI_MODE_GET, "sensorImageNow.jpg", HTTP_UI_JPEG_sensorImageNow},
    {HTTP_UI_MODE_GET, "view.html", HTTP_UI_PAGE_view},
    {HTTP_UI_MODE_GET, "chart.js", HTTP_UI_JS_ChartJS},
    {HTTP_UI_MODE_GET, "chart.html", HTTP_UI_PAGE_chart},
    {HTTP_UI_MODE_GET, "cameraLineView.html", HTTP_UI_PAGE_cameraLineView},
    {HTTP_UI_MODE_GET, "capture.jpg", HTTP_UI_STREAM_JPEG},
    {HTTP_UI_MODE_GET, "configParam.html", HTTP_UI_PAGE_configParam},
    {HTTP_UI_MODE_GET, "configChart.html", HTTP_UI_PAGE_configChart},
    {HTTP_UI_MODE_GET, "configCamera.html", HTTP_UI_PAGE_configCamera},
    {HTTP_UI_MODE_GET, "configTime.html", HTTP_UI_PAGE_configTime},
    {HTTP_UI_MODE_GET, "unitTime.html", HTTP_UI_PAGE_unitTime},
    {HTTP_UI_MODE_GET, "top.html", HTTP_UI_PAGE_top},
    {HTTP_UI_MODE_POST, "configParamSuccess.html", HTTP_UI_POST_configParam},
    {HTTP_UI_MODE_POST, "configChartSuccess.html", HTTP_UI_POST_configChart},
    {HTTP_UI_MODE_POST, "configCameraSuccess.html", HTTP_UI_POST_configCamera},
    {HTTP_UI_MODE_POST, "configTimeSuccess.html", HTTP_UI_POST_configTime},
    {HTTP_UI_MODE_GET, "", HTTP_UI_PAGE_top} // default handler
};

void sendPage(EthernetClient client, String page)
{
  M5_LOGV("page = %s", page.c_str());

  size_t numPages = sizeof(pageHandlers) / sizeof(pageHandlers[0]);

  for (size_t i = 0; i < numPages; ++i)
  {
    if (page == pageHandlers[i].page)
    {
      pageHandlers[i].handler(client);
      return;
    }
  }
  HTTP_UI_PAGE_notFound(client);
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

    size_t numPages = sizeof(pageHandlers) / sizeof(pageHandlers[0]);

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
            sendPage(client, page);
          else
            HTTP_UI_PAGE_notFound(client);
          break;
        }
        if (c == '\n')
          currentLineIsBlank = true, currentLine = "";
        else if (c != '\r')
          currentLineIsBlank = false, currentLine += c;

        for (size_t i = 0; i < numPages; i++)
        {
          String pageName = String(pageHandlers[i].page);
          String CheckLine = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? String("GET /") : String("POST /")) + pageName;
          if (currentLine.endsWith(CheckLine.c_str()))
          {
            page = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? currentLine.substring(5) : currentLine.substring(6));
            M5_LOGI("currentLine = [%s] : CheckLine = [%s]: page = [%s]", currentLine.c_str(), CheckLine.c_str(), page.c_str());
            getRequest = true;
            break;
          }
        }
      }
    }
    delay(1);
    client.stop();
    M5_LOGV("client disconnected");
  }
}
