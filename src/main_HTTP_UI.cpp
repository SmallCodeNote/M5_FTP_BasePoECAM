#include <M5Unified.h>
#include <M5_Ethernet.h>
#include "M5_Ethernet_NtpClient.h"

#include "main.h"
#include "main_HTTP_UI.h"
#include "main_HTTP_UI_ChartJS.h"
#include "main_EEPROM_handler.h"

EthernetServer HttpUIServer = EthernetServer(80);
String SensorValueString = "";

void HTTP_UI_PART_ResponceHeader(EthernetClient httpClient, String Content_Type)
{
  httpClient.println("HTTP/1.1 200 OK");
  httpClient.println("Content-Type: " + Content_Type);
  httpClient.println("Connection: close");
  httpClient.println();
}

void HTTP_UI_PART_HTMLHeader(EthernetClient httpClient, String title)
{
  httpClient.println("<!DOCTYPE HTML>");
  httpClient.println("<html>");
  httpClient.println("<head>");
  httpClient.println("<title>" + title + "</title>");
  httpClient.println("<style>");
  httpClient.println("h1 {padding: 0.0em 0.5em;color: #494949;background: #f4f4f4;border-left: solid 8px #2d8fdd;border-bottom: solid 3px #d7d7d7;}");
  httpClient.println("h2 {padding: 0.0em 0.5em;border-left: solid 8px #2d8fdd;}");
  httpClient.println("h3 {padding: 0.0em 0.5em;border-left: solid 8px #2d8fdd;}");
  httpClient.println("ul,ol {padding:0.0em 1.5em; position:relative;}");
  httpClient.println("ul li, ol li {display: flex;color: #2d8fdd;border-left:solid 6px #2d8fdd;background: #f1f8ff; margin-bottom: 3px;line-height: 1.5;padding: 0.0em 0.5em;list-style-type: none!important;}");
  httpClient.println("label {flex: 0 0 150px; margin-right: 20px;}");
  httpClient.println("input {flex: 0 0 50px; margin-right: 20px;text-align: right;}");
  httpClient.println("</style>");
  httpClient.println("</head>");
  httpClient.println("<body>");
}

void HTTP_UI_PART_HTMLFooter(EthernetClient httpClient)
{
  httpClient.println("</body>");
  httpClient.println("</html>");
}

void HTTP_UI_JSON_sensorValueNow(EthernetClient httpClient)
{
  EdgeItem edgeItem;

  HTTP_UI_PART_ResponceHeader(httpClient, "application/json");
  httpClient.print("{");
  httpClient.print("\"distance\":");
  if (xQueueEdge_Last != NULL && xQueuePeek(xQueueEdge_Last, &edgeItem, 0))
  {
    httpClient.print(String(edgeItem.edgeX));
  }
  else
  {
    httpClient.print(String(-1));
  }
  httpClient.println("}");
}

void HTTP_UI_JSON_unitTimeNow(EthernetClient httpClient)
{
  HTTP_UI_PART_ResponceHeader(httpClient, "application/json");
  httpClient.print("{");
  httpClient.print("\"unitTime\":");
  httpClient.printf("\"%s\"", NtpClient.convertTimeEpochToString().c_str());
  httpClient.println("}");
}

void HTTP_UI_JSON_cameraLineNow(EthernetClient httpClient)
{
  ProfItem profItem;
  EdgeItem edgeItem;
  int maxWait = storeData.imageBufferingEpochInterval * 1000;
  unsigned long startMillis = millis();

  bool sw1 = xQueueProf_Last != NULL && xQueueReceive(xQueueProf_Last, &profItem, maxWait) == pdPASS;
  maxWait = maxWait - (millis() - startMillis);
  maxWait = maxWait < 0 ? 0 : maxWait;
  bool sw2 = xQueueEdge_Last != NULL && xQueuePeek(xQueueEdge_Last, &edgeItem, maxWait) == pdPASS;

  if (sw1 && sw2)
  {

    HTTP_UI_PART_ResponceHeader(httpClient, "application/json");
    httpClient.print("{");
    httpClient.print("\"unitTime\":");
    httpClient.printf("\"%s\"", NtpClient.convertTimeEpochToString(profItem.epoc).c_str());
    httpClient.println(",");
    httpClient.print("\"CameraLineValue\":[");
    httpClient.printf("%u", profItem.buf[0]);

    for (size_t i = 1; i < profItem.len; i++)
    {
      httpClient.printf(",%u", profItem.buf[i]);
      delay(1);
    }

    httpClient.println("]");
    httpClient.println(",");

    httpClient.print("\"edgePoint\":");
    httpClient.printf("%u", edgeItem.edgeX);
    httpClient.println("}");

    free(profItem.buf);
  }
  else
  {
    HTTP_UI_PART_ResponceHeader(httpClient, "application/json");
    httpClient.print("{");
    httpClient.print("\"unitTime\":");
    httpClient.printf("\"%s\"", NtpClient.convertTimeEpochToString().c_str());
    httpClient.println(",");
    httpClient.print("\"CameraLineValue\":[0]");
    httpClient.println(",");

    httpClient.print("\"edgePoint\":");
    httpClient.print("0");
    httpClient.println("}");

    M5_LOGW("sw1 = %d, sw2 = %d, maxWait = %d", sw1, sw2, maxWait);
  }
}

void HTTP_UI_JPEG_sensorImageNow(EthernetClient httpClient)
{
  M5_LOGI("");
  JpegItem jpegItem;
  if (xQueueJpeg_Last != NULL && xQueueReceive(xQueueJpeg_Last, &jpegItem, 0) == pdPASS)
  {
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-Type: image/jpeg");
    httpClient.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
    httpClient.println("Access-Control-Allow-Origin: *");
    httpClient.println();

    int32_t to_sends = jpegItem.len;
    uint8_t *out_buf = jpegItem.buf;

    int32_t now_sends = 0;
    uint32_t packet_len = 2 * 1024;

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (httpClient.write(out_buf, now_sends) == 0)
      {
        break;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
      delay(5);
    }
    free(jpegItem.buf);
    M5_LOGI("");
  }
  else
  {
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-Type: image/jpeg");
    httpClient.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
    httpClient.println("Access-Control-Allow-Origin: *");
    httpClient.println();
  }
  // httpClient.stop();
}

void HTTP_UI_STREAM_JPEG(EthernetClient httpClient)
{
  M5_LOGI("Image stream start");

  httpClient.println("HTTP/1.1 200 OK");
  httpClient.printf("Content-Type: %s\r\n", _STREAM_CONTENT_TYPE);
  httpClient.println("Content-Disposition: inline; filename=capture.jpg");
  httpClient.println("Access-Control-Allow-Origin: *");
  httpClient.println();
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
      httpClient.print(_STREAM_BOUNDARY);
      httpClient.printf(_STREAM_PART, PoECAM.Camera.fb);
      int32_t to_sends = PoECAM.Camera.fb->len;
      int32_t now_sends = 0;
      uint8_t *out_buf = PoECAM.Camera.fb->buf;
      uint32_t packet_len = 1 * 1024;
      while (to_sends > 0)
      {
        now_sends = to_sends > packet_len ? packet_len : to_sends;
        if (httpClient.write(out_buf, now_sends) == 0)
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
  // httpClient.stop();
  M5_LOGI("Image stream end\r\n");
}

void HTTP_UI_PAGE_top(EthernetClient httpClient)
{
  String pageTitle = "top";
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "TOP");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<a href=\"/capture.jpg\">Stream</a><br>");
  httpClient.println("<a href=\"/view.html\">View Page</a><br>");
  httpClient.println("<a href=\"/chart.html\">Chart Page</a><br>");
  httpClient.println("<a href=\"/cameraLineView.html\">CameraLineView Page</a><br>");
  httpClient.println("<a href=\"/unitTime.html\">Unit Time</a><br>");

  httpClient.println("<br>");
  httpClient.println("<hr>");
  httpClient.println("<br>");
  httpClient.println("<a href=\"/configNetworkParam.html\">Config Network Parameter</a><br>");
  httpClient.println("<a href=\"/configIntervalParam.html\">Config Interval Parameter</a><br>");
  httpClient.println("<a href=\"/configCameraParam.html\">Config Camera Parameter</a><br>");
  httpClient.println("<a href=\"/configTime.html\">Config Time</a><br>");
  httpClient.println("<a href=\"/configEdgeSearch.html\">Config EdgeSearch Parameter</a><br>");

  httpClient.println("<a href=\"/flashSwitch.html\">flashSwitch Parameter</a><br>");

  httpClient.println("<br>");
  httpClient.println("<hr>");
  httpClient.println("<br>");

  httpClient.println("<a href=\"/sensorValueNow.json\">sensorValueNow.json</a><br>");
  httpClient.println("<a href=\"/unitTimeNow.json\">unitTimeNow.json</a><br>");
  httpClient.println("<a href=\"/cameraLineNow.json\">cameraLineNow.json</a><br>");
  httpClient.println("<a href=\"/sensorImageNow.jpg\">sensorImageNow.jpg</a><br>");

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_PAGE_view(EthernetClient httpClient)
{
  String pageTitle = "view";
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "VIEW");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  httpClient.println("<ul id=\"sensorData\">");
  httpClient.println("<li>Distance: <span id=\"distance\"></span> mm</li>");
  httpClient.println("</ul>");

  httpClient.println("<img id=\"sensorImage\" src=\"/sensorImageNow.jpg\" alt=\"Sensor Image\" />");

  httpClient.println("<script>");

  httpClient.println("function refreshImage() {");
  httpClient.println("  var img = document.getElementById('sensorImage');");
  httpClient.println("  img.src = '/sensorImageNow.jpg?' + new Date().getTime();"); // add timestamp
  httpClient.println("}");

  httpClient.println("function fetchData() {");
  httpClient.println("  var xhr = new XMLHttpRequest();");
  httpClient.println("  xhr.onreadystatechange = function() {");
  httpClient.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  httpClient.println("      var data = JSON.parse(xhr.responseText);");
  httpClient.println("      document.getElementById('distance').innerText = data.distance;");
  httpClient.println("    }");
  httpClient.println("  };");
  httpClient.println("  xhr.open('GET', '/sensorValueNow.json', true);");
  httpClient.println("  xhr.send();");
  httpClient.println("}");

  httpClient.printf("setInterval(refreshImage, %u);", storeData.chartUpdateInterval);
  httpClient.printf("setInterval(fetchData, %u);", storeData.chartUpdateInterval);
  httpClient.println("refreshImage();");
  httpClient.println("fetchData();");
  httpClient.println("</script>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_PAGE_cameraLineView(EthernetClient httpClient)
{
  String pageTitle = "cameraLineView";
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "cameraLineView");

  httpClient.println("<h1>Camera Line View</h1>");

  httpClient.println("<ul id=\"valueLabel\">");
  httpClient.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  httpClient.println("<li>edgePoint: <span id=\"edgePoint\"></span></li>");
  httpClient.println("</ul>");

  httpClient.println("<canvas id=\"cameraLineChart\" width=\"400\" height=\"100\"></canvas>");

  httpClient.println("<canvas id=\"cameraImage\" width=\"400\"></canvas>");

  httpClient.println("<script src=\"/chart.js\"></script>");

  httpClient.println("<script>");
  httpClient.println("var chart = null;");
  httpClient.println("var edgePointBuff = 0;");

  httpClient.println("function fetchCameraLineData() {");
  httpClient.println("  var xhr = new XMLHttpRequest();");
  httpClient.println("  xhr.onreadystatechange = function() {");
  httpClient.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  httpClient.println("      var data = JSON.parse(xhr.responseText);");
  httpClient.println("      updateChart(data.CameraLineValue);");
  httpClient.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  httpClient.println("      document.getElementById('edgePoint').innerText = data.edgePoint;");
  httpClient.println("      edgePointBuff = data.edgePoint;");
  httpClient.println("    }");
  httpClient.println("  };");
  httpClient.println("  xhr.open('GET', '/cameraLineNow.json', true);");
  httpClient.println("  xhr.send();");
  httpClient.println("}");

  httpClient.println("function updateChart(data) {");
  httpClient.println("  var ctx = document.getElementById('cameraLineChart').getContext('2d');");

  httpClient.println("  if (chart) {");
  httpClient.println("    chart.destroy();");
  httpClient.println("  }");

  httpClient.println("  chart = new Chart(ctx, {");
  httpClient.println("    type: 'line',");
  httpClient.println("    data: {");
  httpClient.println("      labels: data.map((_, index) => index),");
  httpClient.println("      datasets: [{");
  httpClient.println("        label: 'Camera Line Data',");
  httpClient.println("        data: data,");
  httpClient.println("        borderColor: 'rgba(75, 192, 192, 1)',");
  httpClient.println("        borderWidth: 1,");
  httpClient.println("        fill: false");
  httpClient.println("      }]");
  httpClient.println("    },");
  httpClient.println("    options: {");
  httpClient.println("      animation: false,");
  httpClient.println("      scales: {");
  httpClient.println("        x: {");
  httpClient.println("          type: 'linear',");
  httpClient.println("          position: 'bottom'");
  httpClient.println("        },");
  httpClient.println("        y: {");
  httpClient.println("          type: 'linear',");
  httpClient.println("          position: 'right'");
  //  httpClient.println("          display: false");
  httpClient.println("        }");
  httpClient.println("      },");
  httpClient.println("      plugins: {");
  httpClient.println("        legend: {");
  httpClient.println("          display: false");
  httpClient.println("        }");
  httpClient.println("      }");
  httpClient.println("    }");
  httpClient.println("  });");
  httpClient.println("}");

  uint32_t iWidth = (uint32_t)CameraSensorFrameWidth(storeData.framesize);
  uint32_t iHeight = (uint32_t)CameraSensorFrameHeight(storeData.framesize);

  uint32_t centerX = iWidth / 2, centerY = iHeight / 2;

  uint32_t x1 = (uint32_t)((iWidth * (100 - storeData.pixLineRange)) / 200);
  uint32_t xw = (uint32_t)((iWidth * storeData.pixLineRange) / 100);
  uint32_t y1 = (uint32_t)((iHeight) / 2) - 1;
  uint32_t px = 0;
  u_int32_t py = 0;

  httpClient.println("function refreshImage() {");
  httpClient.println("  var ctx = document.getElementById('cameraImage').getContext('2d');");
  httpClient.println("  var img = new Image();");
  httpClient.println("  img.onload = function() {");
  httpClient.println("    var canvas = document.getElementById('cameraImage');");
  httpClient.println("    canvas.height = img.height * (canvas.width / img.width);");
  httpClient.println("    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);");
  httpClient.println("    ctx.strokeStyle = 'red';");
  httpClient.println("    ctx.lineWidth = 1;");
  httpClient.printf("    var angle = %u * Math.PI / 180;", storeData.pixLineAngle); // Convert angle to radians
  httpClient.println("    var centerX = canvas.width / 2;");
  httpClient.println("    var centerY = canvas.height / 2;");
  httpClient.printf("    var halfWidth = %u * (canvas.width / img.width);", (xw / 2));
  httpClient.printf("    var halfHeight =  %u * (canvas.height / img.height);", (2 / 2));

  httpClient.println("    function rotatePoint(x, y, centerX, centerY, angle) {");
  httpClient.println("      var cos = Math.cos(angle);");
  httpClient.println("      var sin = Math.sin(angle);");
  httpClient.println("      var nx = cos * (x - centerX) - sin * (y - centerY) + centerX;");
  httpClient.println("      var ny = sin * (x - centerX) + cos * (y - centerY) + centerY;");
  httpClient.println("      return {x: nx, y: ny};");
  httpClient.println("    }");

  httpClient.println("    var points = [");
  httpClient.println("      rotatePoint(centerX - halfWidth, centerY - halfHeight, centerX, centerY, angle),");
  httpClient.println("      rotatePoint(centerX + halfWidth, centerY - halfHeight, centerX, centerY, angle),");
  httpClient.println("      rotatePoint(centerX + halfWidth, centerY + halfHeight, centerX, centerY, angle),");
  httpClient.println("      rotatePoint(centerX - halfWidth, centerY + halfHeight, centerX, centerY, angle)");
  httpClient.println("    ];");

  httpClient.println("    ctx.beginPath();");
  httpClient.println("    ctx.moveTo(points[0].x, points[0].y);");
  httpClient.println("    for (var i = 1; i < points.length; i++) {");
  httpClient.println("      ctx.lineTo(points[i].x, points[i].y);");
  httpClient.println("    }");
  httpClient.println("    ctx.closePath();");
  httpClient.println("    ctx.stroke();");

  httpClient.println("    ctx.beginPath();");
  httpClient.printf("    ctx.arc(%u * (canvas.width / img.width), edgePointBuff * (canvas.height / img.height), 3, 0, 2 * Math.PI);", centerX);
  httpClient.println("    ctx.fillStyle = 'red';");
  httpClient.println("    ctx.fill();");
  httpClient.println("    ctx.stroke();");

  httpClient.println("  };");
  httpClient.println("  img.src = '/sensorImageNow.jpg?' + new Date().getTime();"); // add timestamp
  httpClient.println("}");

  httpClient.println("function update() {");
  httpClient.println("  refreshImage();");
  httpClient.println("  fetchCameraLineData();");
  httpClient.println("}");

  httpClient.printf("setInterval(update, %u);", storeData.chartUpdateInterval);
  httpClient.println("update();");
  httpClient.println("</script>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_PAGE_chart(EthernetClient httpClient)
{
  String pageTitle = "chart";
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "chart");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  httpClient.println("<ul id=\"sensorData\">");
  httpClient.println("<li>Distance: <span id=\"distance\"></span> mm</li>");
  httpClient.println("</ul>");

  httpClient.println("<canvas id=\"distanceChart\" width=\"400\" height=\"200\"></canvas>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  httpClient.println("<script src=\"/chart.js\"></script>");
  httpClient.println("<script>");
  httpClient.printf("var distanceData = Array(%u).fill(0);\n", storeData.chartShowPointCount);
  httpClient.println("var myChart = null;");
  httpClient.println("function fetchData() {");
  httpClient.println("  var xhr = new XMLHttpRequest();");
  httpClient.println("  xhr.onreadystatechange = function() {");
  httpClient.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  httpClient.println("      var data = JSON.parse(xhr.responseText);");
  httpClient.println("      document.getElementById('distance').innerText = data.distance;");
  httpClient.println("      distanceData.push(data.distance);");
  httpClient.printf("      if (distanceData.length > %u) { distanceData.shift(); }", storeData.chartShowPointCount);
  httpClient.println("      updateChart();");
  httpClient.println("    }");
  httpClient.println("  };");
  httpClient.println("  xhr.open('GET', '/sensorValueNow.json', true);");
  httpClient.println("  xhr.send();");
  httpClient.println("}");

  httpClient.println("function updateChart() {");
  httpClient.println("  var ctx = document.getElementById('distanceChart').getContext('2d');");

  httpClient.println("  if (myChart) {");
  httpClient.println("    myChart.destroy();");
  httpClient.println("  }");

  httpClient.println("  myChart = new Chart(ctx, {");
  httpClient.println("    type: 'line',");
  httpClient.println("    data: {");
  httpClient.println("      labels: Array.from({length: distanceData.length}, (_, i) => i + 1),");
  httpClient.println("      datasets: [{");
  httpClient.println("        label: 'Distance',");
  httpClient.println("        data: distanceData,");
  httpClient.println("        borderColor: 'rgba(75, 192, 192, 1)',");
  httpClient.println("        borderWidth: 1");
  httpClient.println("      }]");
  httpClient.println("    },");
  httpClient.println("    options: {");
  httpClient.println("      animation: false,");
  httpClient.println("      scales: {");
  httpClient.println("        x: { beginAtZero: true },");
  httpClient.println("        y: { beginAtZero: true }");
  httpClient.println("      }");
  httpClient.println("    }");
  httpClient.println("  });");
  httpClient.println("}");

  httpClient.printf("setInterval(fetchData, %u);", storeData.chartUpdateInterval);
  httpClient.println("fetchData();");

  httpClient.println("</script>");

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_PAGE_notFound(EthernetClient httpClient)
{
  String pageTitle = "notFound";
  httpClient.println("HTTP/1.1 404 Not Found");
  httpClient.println("Content-Type: text/html");
  httpClient.println("Connection: close");
  httpClient.println();

  HTTP_UI_PART_HTMLHeader(httpClient, "404 not found");

  httpClient.println("<h1>404 Not Found</h1>");

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_POSTPAGE_configIntervalParam(EthernetClient httpClient)
{
  String pageTitle = "Interval Parameters.";

  // POST LOAD ==============
  String currentLine = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());
  if (currentLineLength > 1)
  {
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(imageBufferingEpochInterval);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftpImageSaveInterval);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftpEdgeSaveInterval);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftpProfileSaveInterval);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(chartShowPointCount);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(chartUpdateInterval);

    PutEEPROM();
  }

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "config param");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  if (currentLineLength > 1)
  {
    httpClient.printf("Last Update <br /> %s", NtpClient.convertTimeEpochToString().c_str());
    httpClient.println("Rebooting...");
  }

  httpClient.println("<form action=\"/configIntervalParam.html\" method=\"post\">");
  httpClient.println("<ul>");

  HTML_PUT_LI_INPUT_WITH_COMMENT(imageBufferingEpochInterval, "1- (sec.)");

  HTML_PUT_LI_INPUT_WITH_COMMENT(ftpImageSaveInterval, "1- (sec.)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(ftpEdgeSaveInterval, "1- (sec.)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(ftpProfileSaveInterval, "1- (sec.)");

  HTML_PUT_LI_INPUT(chartShowPointCount);
  HTML_PUT_LI_INPUT_WITH_COMMENT(chartUpdateInterval, "(msec.)");

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_POSTPAGE_configNetworkParam(EthernetClient httpClient)
{
  String pageTitle = "config Network Param";

  // POST LOAD ==============
  String currentLine = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());
  if (currentLineLength > 1)
  {
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(deviceName);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(deviceIP_String);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ntpSrvIP_String);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftpSrvIP_String);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftp_user);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ftp_pass);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(timeZoneOffset);

    PutEEPROM();
  }

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "config param");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  if (currentLineLength > 1)
  {
    httpClient.printf("Last Update <br /> %s", NtpClient.convertTimeEpochToString().c_str());
    httpClient.println("Rebooting...");
  }

  httpClient.println("<form action=\"/configNetworkParam.html\" method=\"post\">");
  httpClient.println("<ul>");

  HTML_PUT_LI_WIDEINPUT(deviceName);
  HTML_PUT_LI_WIDEINPUT(deviceIP_String);
  HTML_PUT_LI_WIDEINPUT(ntpSrvIP_String);
  HTML_PUT_LI_INPUT(timeZoneOffset);

  HTML_PUT_LI_WIDEINPUT(ftpSrvIP_String);
  HTML_PUT_LI_WIDEINPUT(ftp_user);
  HTML_PUT_LI_WIDEINPUT(ftp_pass);

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save and reboot</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);

  if (currentLineLength > 1)
  {
    xTaskCreatePinnedToCore(TaskRestart, "TaskRestart", 4096, NULL, 0, NULL, 1);
  }
}

void HTTP_UI_POSTPAGE_configCameraParam(EthernetClient httpClient)
{
  String pageTitle = "config Camera Param";

  // POST LOAD ==============
  String currentLine = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());
  if (currentLineLength > 1)
  {
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(pixformat);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(framesize);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(contrast);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(brightness);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(saturation);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(sharpness);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(denoise);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(quality);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(colorbar);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(whitebal);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(gain_ctrl);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(exposure_ctrl);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(hmirror);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(vflip);

    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(gainceiling);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(aec2);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(awb_gain);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(agc_gain);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(aec_value);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(special_effect);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(wb_mode);
    HTTP_GET_PARAM_FROM_POST_OR_STORESTRING(ae_level);

    PutEEPROM();
    CameraSensorFullSetupFromStoreData();
  }

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "config camera");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  httpClient.println("<form action=\"/configCameraParam.html\" method=\"post\">");
  httpClient.println("<ul>");

  String optionString = " selected";
  int pixformat_i = pixformat.toInt();

  httpClient.println("<li><label for=\"pixformat\">Pixformat:</label>");
  httpClient.println("<select id=\"pixformat\" name=\"pixformat\">");

  httpClient.printf("<option value=\"%u\" %s>RGB565</option>", PIXFORMAT_RGB565, pixformat_i == PIXFORMAT_RGB565 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>YUV422</option>", PIXFORMAT_YUV422, pixformat_i == PIXFORMAT_YUV422 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>YUV420</option>", PIXFORMAT_YUV420, pixformat_i == PIXFORMAT_YUV420 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>Grayscale</option>", PIXFORMAT_GRAYSCALE, pixformat_i == PIXFORMAT_GRAYSCALE ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>JPEG</option>", PIXFORMAT_JPEG, pixformat_i == PIXFORMAT_JPEG ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>RGB888</option>", PIXFORMAT_RGB888, pixformat_i == PIXFORMAT_RGB888 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>RAW</option>", PIXFORMAT_RAW, pixformat_i == PIXFORMAT_RAW ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>RGB444</option>", PIXFORMAT_RGB444, pixformat_i == PIXFORMAT_RGB444 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>RGB555</option>", PIXFORMAT_RGB555, pixformat_i == PIXFORMAT_RGB555 ? "selected" : "");

  httpClient.println("</select></li>");

  int framesize_i = framesize.toInt();

  httpClient.println("<li><label for=\"framesize\">Framesize:</label>");
  httpClient.println("<select id=\"framesize\" name=\"framesize\">");

  httpClient.printf("<option value=\"%u\" %s>96x96</option>", FRAMESIZE_96X96, framesize_i == FRAMESIZE_96X96 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>160x120</option>", FRAMESIZE_QQVGA, framesize_i == FRAMESIZE_QQVGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>176x144</option>", FRAMESIZE_QCIF, framesize_i == FRAMESIZE_QCIF ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>240x176</option>", FRAMESIZE_HQVGA, framesize_i == FRAMESIZE_HQVGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>240x240</option>", FRAMESIZE_240X240, framesize_i == FRAMESIZE_240X240 ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>320x240</option>", FRAMESIZE_QVGA, framesize_i == FRAMESIZE_QVGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>400x296</option>", FRAMESIZE_CIF, framesize_i == FRAMESIZE_CIF ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>480x320</option>", FRAMESIZE_HVGA, framesize_i == FRAMESIZE_HVGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>640x480</option>", FRAMESIZE_VGA, framesize_i == FRAMESIZE_VGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>800x600</option>", FRAMESIZE_SVGA, framesize_i == FRAMESIZE_SVGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1024x768</option>", FRAMESIZE_XGA, framesize_i == FRAMESIZE_XGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1280x720</option>", FRAMESIZE_HD, framesize_i == FRAMESIZE_HD ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1280x1024</option>", FRAMESIZE_SXGA, framesize_i == FRAMESIZE_SXGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1600x1200</option>", FRAMESIZE_UXGA, framesize_i == FRAMESIZE_UXGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1920x1080</option>", FRAMESIZE_FHD, framesize_i == FRAMESIZE_FHD ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>720x1280</option>", FRAMESIZE_P_HD, framesize_i == FRAMESIZE_P_HD ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>864x1536</option>", FRAMESIZE_P_3MP, framesize_i == FRAMESIZE_P_3MP ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>2048x1536</option>", FRAMESIZE_QXGA, framesize_i == FRAMESIZE_QXGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>2560x1440</option>", FRAMESIZE_QHD, framesize_i == FRAMESIZE_QHD ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>2560x1600</option>", FRAMESIZE_WQXGA, framesize_i == FRAMESIZE_WQXGA ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>1080x1920</option>", FRAMESIZE_P_FHD, framesize_i == FRAMESIZE_P_FHD ? "selected" : "");
  httpClient.printf("<option value=\"%u\" %s>2560x1920</option>", FRAMESIZE_QSXGA, framesize_i == FRAMESIZE_QSXGA ? "selected" : "");

  httpClient.println("</select></li>");

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

  HTML_PUT_LI_INPUT_WITH_COMMENT(gainceiling, "0 - 6");

  HTML_PUT_LI_INPUT_WITH_COMMENT(aec2, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(awb_gain, "0 | 1");
  HTML_PUT_LI_INPUT_WITH_COMMENT(agc_gain, "0 - 30");
  HTML_PUT_LI_INPUT_WITH_COMMENT(aec_value, "0 - 1200");

  HTML_PUT_LI_INPUT_WITH_COMMENT(special_effect, "0 - 6");
  HTML_PUT_LI_INPUT_WITH_COMMENT(wb_mode, "0 - 4");
  HTML_PUT_LI_INPUT_WITH_COMMENT(ae_level, "-2 - 2");

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_POSTPAGE_configEdgeSearch(EthernetClient httpClient)
{
  String pageTitle = "config Edge Search";
  // POST LOAD ==============
  String currentLine = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());
  if (currentLineLength > 1)
  {
    //HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(chartShowPointCount);
    //HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(chartUpdateInterval);


  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineStep);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineRange);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineAngle);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineShiftUp);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineSideWidth);

  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineEdgeSearchStart);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineEdgeSearchEnd);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineEdgeUp);
  HTTP_GET_PARAM_FROM_POST_OR_STOREDATA(pixLineThrethold);

    PutEEPROM();
  }

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, pageTitle);

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  if (currentLineLength > 1)
  {
    httpClient.printf("Last Update <br /> %s", NtpClient.convertTimeEpochToString().c_str());
  }

  httpClient.println("<form action=\"/configEdgeSearch.html\" method=\"post\">");
  httpClient.println("<ul>");

  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineStep, "0-255 (px)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineRange, "0-100 (%)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineAngle, "0-90 (deg) 0:horizontal 90:vertical");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineShiftUp, "0-255 (px)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineSideWidth, "0-255 (px) 0:LineWidth->1,1->3,2->5");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeSearchStart, "0-100 (%)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeSearchEnd, "0-100 (%)");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeUp, "1: dark -> light / 2: light -> dark");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineThrethold, "0-765");

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_POSTPAGE_configTime(EthernetClient httpClient)
{
  String pageTitle = "config Time";

  // POST LOAD ==============
  String currentLine = "";
  String timeString = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());
  if (currentLineLength > 1)
  {
    HTTP_GET_PARAM_FROM_POST(timeString);

    timeString = urlDecode(timeString);
    M5_LOGI("posted timeString = %s", timeString.c_str());
    NtpClient.updateTimeFromString(timeString);
  }

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "config time");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<br />");

  httpClient.println("<ul id=\"unitTime\">");
  httpClient.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  httpClient.println("</ul>");

  httpClient.println("<form action=\"/configTime.html\" method=\"post\">");
  httpClient.println("<ul>");

  HTML_PUT_LI_WIDEINPUT(timeString);

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  httpClient.println("<script>");

  httpClient.println("function updateTimeString() {");
  httpClient.println(" document.getElementById('timeString').value = new Date().toLocaleString();");
  httpClient.println("}");
  httpClient.println("setInterval(updateTimeString, 1000);");

  httpClient.println("function fetchData() {");
  httpClient.println("  var xhr = new XMLHttpRequest();");
  httpClient.println("  xhr.onreadystatechange = function() {");
  httpClient.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  httpClient.println("      var data = JSON.parse(xhr.responseText);");
  httpClient.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  httpClient.println("    }");
  httpClient.println("  };");
  httpClient.println("  xhr.open('GET', '/unitTimeNow.json', true);");
  httpClient.println("  xhr.send();");
  httpClient.println("}");
  httpClient.println("setInterval(fetchData, 1000);");

  httpClient.println("fetchData();");
  httpClient.println("updateTimeString();");
  httpClient.println("</script>");

  HTTP_UI_PART_HTMLFooter(httpClient);
}


void HTTP_UI_PAGE_unitTime(EthernetClient httpClient)
{
  String pageTitle = "unitTime";
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, "unit time");

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<br />");

  httpClient.println("<ul id=\"unitTime\">");
  httpClient.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  httpClient.println("</ul>");

  httpClient.println("<br>");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  httpClient.println("<script>");
  httpClient.println("function fetchData() {");
  httpClient.println("  var xhr = new XMLHttpRequest();");
  httpClient.println("  xhr.onreadystatechange = function() {");
  httpClient.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  httpClient.println("      var data = JSON.parse(xhr.responseText);");
  httpClient.println("      document.getElementById('unitTime').innerText = data.unitTime;");
  httpClient.println("    }");
  httpClient.println("  };");
  httpClient.println("  xhr.open('GET', '/unitTimeNow.json', true);");
  httpClient.println("  xhr.send();");
  httpClient.println("}");
  httpClient.println("setInterval(fetchData, 1000);");
  httpClient.println("fetchData();");
  httpClient.println("</script>");

  HTTP_UI_PART_HTMLFooter(httpClient);
}

void HTTP_UI_POSTPAGE_flashSwitch(EthernetClient httpClient)
{
  String pageTitle = "flashSwitch";
  // POST LOAD ==============
  String currentLine = "";
  httpClient.setTimeout(5u);
  while (httpClient.available())
  {
    currentLine = httpClient.readStringUntil('\n');
  }
  unsigned int currentLineLength = currentLine.length();
  M5_LOGV("POST %u :: [%s]", currentLineLength, currentLine.c_str());

  String flashBrightnessStatus = flashIntensityMode;
  String flashTestLength = flashLength;

  HTTP_GET_PARAM_FROM_POST(flashBrightnessStatus);
  HTTP_GET_PARAM_FROM_POST(flashTestLength);

  if (flashBrightnessStatus.length() < 1)
  {
    flashBrightnessStatus = String(storeData.flashIntensityMode);
  }

  if (flashTestLength.length() < 1)
  {
    flashTestLength = String(storeData.flashLength);
  }

  uint8_t brightness_u = flashBrightnessStatus.toInt();
  uint16_t flashTestLength_u = flashTestLength.toInt();

  M5_LOGD("%s, %u", flashTestLength, flashTestLength_u);

  storeData.flashIntensityMode = brightness_u;
  storeData.flashLength = flashTestLength_u;
  delay(storeData.imageBufferingEpochInterval);

  // GET RETURN ==============
  HTTP_UI_PART_ResponceHeader(httpClient, "text/html");
  HTTP_UI_PART_HTMLHeader(httpClient, pageTitle);

  httpClient.println("<h1>" + deviceName + "</h1>");
  httpClient.println("<h2>" + pageTitle + "</h2>");

  httpClient.println("<form action=\"/flashSwitch.html\" method=\"post\">");
  httpClient.println("<ul>");

  currentLine = "";

  HTML_PUT_LI_INPUT(flashTestLength);

  String optionString = " selected";

  httpClient.println("<li><label for=\"flashBrightnessStatus\">Brightness:</label>");
  httpClient.println("<select id=\"flashBrightnessStatus\" name=\"flashBrightnessStatus\">");

  httpClient.printf("<option value=\"%d\" %s>Flashlight off</option>", 0, brightness_u == 0 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>100%% brightness + 220ms</option>", 1, brightness_u == 1 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>90%% brightness + 220ms</option>", 2, brightness_u == 2 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>80%% brightness + 220ms</option>", 3, brightness_u == 3 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>70%% brightness + 220ms</option>", 4, brightness_u == 4 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>60%% brightness + 220ms</option>", 5, brightness_u == 5 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>50%% brightness + 220ms</option>", 6, brightness_u == 6 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>40%% brightness + 220ms</option>", 7, brightness_u == 7 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>30%% brightness + 220ms</option>", 8, brightness_u == 8 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>100%% brightness + 1.3s</option>", 9, brightness_u == 9 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>90%% brightness + 1.3s</option>", 10, brightness_u == 10 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>80%% brightness + 1.3s</option>", 11, brightness_u == 11 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>70%% brightness + 1.3s</option>", 12, brightness_u == 12 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>60%% brightness + 1.3s</option>", 13, brightness_u == 13 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>50%% brightness + 1.3s</option>", 14, brightness_u == 14 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>40%% brightness + 1.3s</option>", 15, brightness_u == 15 ? "selected" : "");
  httpClient.printf("<option value=\"%d\" %s>30%% brightness + 1.3s</option>", 16, brightness_u == 16 ? "selected" : "");

  httpClient.println("</select></li>");

  httpClient.println("<li class=\"button\">");
  httpClient.println("<button type=\"submit\">Save and Flash</button>");
  httpClient.println("</li>");
  httpClient.println("</ul>");
  httpClient.println("</form>");

  httpClient.println("<br />");
  httpClient.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  httpClient.printf("<img src=\"/sensorImageNow.jpg?%s\">", NtpClient.convertTimeEpochToString().c_str());

  HTTP_UI_PART_HTMLFooter(httpClient);
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
    {HTTP_UI_MODE_GET, "unitTime.html", HTTP_UI_PAGE_unitTime},

    {HTTP_UI_MODE_GET, "configNetworkParam.html", HTTP_UI_POSTPAGE_configNetworkParam},
    {HTTP_UI_MODE_POST, "configNetworkParam.html", HTTP_UI_POSTPAGE_configNetworkParam},

    {HTTP_UI_MODE_GET, "configIntervalParam.html", HTTP_UI_POSTPAGE_configIntervalParam},
    {HTTP_UI_MODE_POST, "configIntervalParam.html", HTTP_UI_POSTPAGE_configIntervalParam},

    {HTTP_UI_MODE_GET, "configCameraParam.html", HTTP_UI_POSTPAGE_configCameraParam},
    {HTTP_UI_MODE_POST, "configCameraParam.html", HTTP_UI_POSTPAGE_configCameraParam},

    {HTTP_UI_MODE_GET, "flashSwitch.html", HTTP_UI_POSTPAGE_flashSwitch},
    {HTTP_UI_MODE_POST, "flashSwitch.html", HTTP_UI_POSTPAGE_flashSwitch},

    {HTTP_UI_MODE_GET, "configEdgeSearch.html", HTTP_UI_POSTPAGE_configEdgeSearch},
    {HTTP_UI_MODE_POST, "configEdgeSearch.html", HTTP_UI_POSTPAGE_configEdgeSearch},

    {HTTP_UI_MODE_GET, "configTime.html", HTTP_UI_POSTPAGE_configTime},
    {HTTP_UI_MODE_POST, "configTime.html", HTTP_UI_POSTPAGE_configTime},

    {HTTP_UI_MODE_GET, "top.html", HTTP_UI_PAGE_top},
    {HTTP_UI_MODE_GET, " ", HTTP_UI_PAGE_top} // default handler
};

void sendPage(EthernetClient httpClient, String page)
{
  M5_LOGI("page = %s", page.c_str());

  size_t numPages = sizeof(pageHandlers) / sizeof(pageHandlers[0]);

  for (size_t i = 0; i < numPages; ++i)
  {
    if (page == pageHandlers[i].page)
    {
      M5_LOGD("Eth mutex take");
      mutex_Eth_Take_FunctionName = String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      if (xSemaphoreTake(mutex_Eth, (TickType_t)MUX_ETH_BLOCK_TIM) == pdTRUE)
      {
        pageHandlers[i].handler(httpClient);
        xSemaphoreGive(mutex_Eth);
        M5_LOGI("Eth mutex give");
        mutex_Eth_Take_FunctionName = String("nan ") + String(__FUNCTION__) + ": " + String(__LINE__) + " [" + String(millis()) + "]";
      }
      else
      {
        M5_LOGW("eth mutex can not take. : take function = %s", mutex_Eth_Take_FunctionName.c_str());
      }
      return;
    }
  }
  HTTP_UI_PAGE_notFound(httpClient);
}

void HTTP_UI()
{
  if (xSemaphoreTake(mutex_Eth_SocketOpen, portMAX_DELAY) != pdTRUE)
  {
    return;
  }
  EthernetClient httpClient = HttpUIServer.available();
  xSemaphoreGive(mutex_Eth_SocketOpen);

  if (!httpClient)
    return;

  M5_LOGI("new httpClient");

  unsigned long readTimeout = 5u;
  httpClient.setTimeout(readTimeout);

  String currentLine = "";
  currentLine.reserve(256u);
  String requestedPageName = "";
  requestedPageName.reserve(64u);
  size_t numPages = sizeof(pageHandlers) / sizeof(pageHandlers[0]);
  bool getRequest = false;

  while (httpClient.connected())
  {
    if (httpClient.available())
    {
      currentLine = httpClient.readStringUntil('\n');
      currentLine.replace("\r", "");
      unsigned int currentLineLength = currentLine.length();

      if (currentLineLength == 0) // Final Line Reach
      {
        if (getRequest)
        {
          sendPage(httpClient, requestedPageName);
        }
        else
        {
          HTTP_UI_PAGE_notFound(httpClient);
        }
        M5_LOGD("break from request line end.");
        break;
      }
      else // Line have contents
      {
        M5_LOGV("%s", currentLine.c_str());
        if (currentLineLength > 6 && (currentLine.startsWith("GET /") || currentLine.startsWith("POST /")))
        {
          for (size_t i = 0; i < numPages; i++)
          {
            String pageName = String(pageHandlers[i].page);
            String CheckLine = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? String("GET /") : String("POST /")) + pageName;

            if (currentLine.startsWith(CheckLine.c_str()))
            {
              requestedPageName = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? CheckLine.substring(5) : CheckLine.substring(6));
              M5_LOGI("currentLine = [%s] : CheckLine = [%s]: page = [%s]", currentLine.c_str(), CheckLine.c_str(), requestedPageName.c_str());
              getRequest = true;
              break;
            }
          }
        }
      }

      delay(1);
    }
  }
  httpClient.stop();
}
