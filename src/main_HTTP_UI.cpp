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
  EdgeItem edgeItem;

  HTTP_UI_PART_ResponceHeader(client, "application/json");
  client.print("{");
  client.print("\"distance\":");
  if (xQueuePeek(xQueueEdge_Last, &edgeItem, 0))
  {
    client.print(String(edgeItem.edgeX));
  }
  else
  {
    client.print(String(-1));
  }
  // client.print(String(SensorValueString.toInt()));
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

// uint8_t *HTTP_UI_JPEG_cameraLineNow_JPEG_buf;
// int32_t HTTP_UI_JPEG_cameraLineNow_JPEG_len;

/*void HTTP_UI_JSON_cameraLineNow(EthernetClient client)
{
  M5_LOGD("");

  HTTP_UI_JPEG_STORE_TaskArgs taskArgs = {&HTTP_UI_JPEG_cameraLineNow_JPEG_buf, &HTTP_UI_JPEG_cameraLineNow_JPEG_len};
  unit_flash_set_brightness(storeData.flashIntensityMode);
  digitalWrite(FLASH_EN_PIN, HIGH);

  delay(30);
  xTaskCreatePinnedToCore(HTTP_UI_JPEG_STORE_Task, "HTTP_UI_JPEG_STORE_Task", 4096, &taskArgs, 0, NULL, 0);
  delay(storeData.flashLength);
  digitalWrite(FLASH_EN_PIN, LOW);

  if (storeData.flashLength < 30)
    delay(30 - storeData.flashLength);

  if (HTTP_UI_JPEG_cameraLineNow_JPEG_len > 0)
  {
    PoECAM.setLed(true);

    int32_t frame_len = *(taskArgs.HTTP_UI_JPEG_len);
    uint8_t *frame_buf = *(taskArgs.HTTP_UI_JPEG_buf);
    pixformat_t pixmode = taskArgs.pixmode;
    u_int32_t fb_width = (u_int32_t)(taskArgs.fb_width);
    u_int32_t fb_height = (u_int32_t)(taskArgs.fb_height);

    // M5_LOGD("w: %u ,h: %u ,m:%u", fb_width, fb_height, pixmode);

    uint8_t *bitmap_buf = (uint8_t *)ps_malloc(3 * taskArgs.fb_width * taskArgs.fb_height);
    fmt2rgb888(frame_buf, frame_len, pixmode, bitmap_buf);

    uint32_t pixLineStep = (u_int32_t)(storeData.pixLineStep);
    pixLineStep = pixLineStep > fb_width ? fb_width : pixLineStep;
    uint32_t pixLineRange = (u_int32_t)(storeData.pixLineRange);
    pixLineRange = pixLineRange > 100u ? 100u : pixLineRange;

    // M5_LOGD("Step: %u ,Range: %u", pixLineStep, pixLineRange);

    uint32_t RangePix = fb_width * pixLineRange / 100;
    uint32_t xStartPix = ((fb_width * (100u - pixLineRange)) / 200u);
    uint32_t xEndPix = xStartPix + ((fb_width * pixLineRange) / 100u);

    // M5_LOGD("xStartPix= %u xEndPix= %u, RangePix= %u", xStartPix, xEndPix, RangePix);
    // M5_LOGD("%u - %u", xStartPix, xEndPix);

    u_int32_t startOffset = (fb_width * fb_height / 2u + xStartPix) * 3u;
    uint8_t *bitmap_pix = bitmap_buf + startOffset;
    uint8_t *bitmap_pix_lineEnd = bitmap_buf + startOffset + (xEndPix - xStartPix) * 3u;

    bitmap_pix_lineEnd = bitmap_pix_lineEnd == bitmap_pix ? bitmap_pix_lineEnd + 3u : bitmap_pix_lineEnd;

    HTTP_UI_PART_ResponceHeader(client, "application/json");
    client.print("{");
    client.print("\"unitTime\":");
    client.printf("\"%s\"", NtpClient.convertTimeEpochToString().c_str());
    client.println(",");
    client.print("\"CameraLineValue\":[");

    u_int16_t br = 0;
    br += *(bitmap_pix);
    bitmap_pix++;
    br += *(bitmap_pix);
    bitmap_pix++;
    br += *(bitmap_pix);
    bitmap_pix++;
    client.printf("%u", br);

    while (bitmap_pix_lineEnd > bitmap_pix)
    {
      br = 0;
      br += *(bitmap_pix);
      bitmap_pix++;
      br += *(bitmap_pix);
      bitmap_pix++;
      br += *(bitmap_pix);
      bitmap_pix++;
      client.printf(",%u", br);

      bitmap_pix += pixLineStep * 3;
    }

    client.println("]");
    client.println(",");

    client.print("\"edgePoint\":");
    uint16_t edgePoint = HTTP_UI_JSON_cameraLineNow_EdgePosition(bitmap_buf, taskArgs);
    client.printf("%u", edgePoint);

    client.println("}");

    free(bitmap_buf);
  }
  // M5_LOGD("");
}
*/

void HTTP_UI_JSON_cameraLineNow(EthernetClient client)
{
  ProfItem profItem;
  EdgeItem edgeItem;
  M5_LOGD("");
  if (xQueueProf_Last != NULL && xQueueReceive(xQueueProf_Last, &profItem, portMAX_DELAY) == pdPASS)
  {

    M5_LOGD("");
    xQueuePeek(xQueueEdge_Last, &edgeItem, portMAX_DELAY);

    HTTP_UI_PART_ResponceHeader(client, "application/json");
    client.print("{");
    client.print("\"unitTime\":");
    client.printf("\"%s\"", NtpClient.convertTimeEpochToString(profItem.epoc).c_str());
    client.println(",");
    client.print("\"CameraLineValue\":[");
    client.printf("%u", profItem.buf[0]);
    M5_LOGD("");
    for (size_t i = 1; i < profItem.len; i++)
    {
      client.printf(",%u", profItem.buf[i]);
    }

    client.println("]");
    client.println(",");

    client.print("\"edgePoint\":");
    client.printf("%u", edgeItem.edgeX);
    client.println("}");
    M5_LOGD("");
    free(profItem.buf);
  }
  M5_LOGD("");
}

uint16_t HTTP_UI_JSON_cameraLineNow_EdgePosition(uint8_t *bitmap_buf, HTTP_UI_JPEG_STORE_TaskArgs taskArgs)
{

  int32_t fb_width = (int32_t)((taskArgs.fb_width));
  int32_t fb_height = (int32_t)((taskArgs.fb_height));
  int32_t xStartRate = (int32_t)(storeData.pixLineEdgeSearchStart);
  int32_t xEndRate = (int32_t)(storeData.pixLineEdgeSearchEnd);
  M5_LOGI("xStartRate=%d , xEndRate=%d ", xStartRate, xEndRate);

  int32_t xStartPix = (int32_t)((fb_width * xStartRate) / 100);
  int32_t xEndPix = (int32_t)((fb_width * xEndRate) / 100);
  M5_LOGI("xStartPix=%d , xEndPix=%d ", xStartPix, xEndPix);

  int32_t xStep = xStartPix < xEndPix ? 1 : -1;

  int32_t startOffset = (fb_width * fb_height / 2) * 3;
  uint8_t *bitmap_pix = bitmap_buf + startOffset;
  int16_t br = 0;
  int32_t EdgeMode = storeData.pixLineEdgeUp == 1 ? 1 : -1;
  int32_t th = (int32_t)storeData.pixLineThrethold;

  int32_t x = xStartPix;

  for (; (xEndPix - x) * xStep > 0; x += xStep)
  {
    bitmap_pix = bitmap_buf + startOffset + x * 3;
    br = 0;
    br += *(bitmap_pix);
    br += *(bitmap_pix + 1);
    br += *(bitmap_pix + 2);
    M5_LOGI("%d : %d / %d", x, br, th);
    if ((th - br) * EdgeMode < 0)
    {
      return (uint16_t)x;
    }
  }

  return (uint16_t)x;
}
/*
void HTTP_UI_JPEG_cameraLineNow(EthernetClient client)
{
  M5_LOGI("");
  HTTP_UI_JPEG_STORE_TaskArgs taskArgs = {&HTTP_UI_JPEG_cameraLineNow_JPEG_buf, &HTTP_UI_JPEG_cameraLineNow_JPEG_len};
  if (*(taskArgs.HTTP_UI_JPEG_len) > 0)
  {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    int32_t to_sends = *(taskArgs.HTTP_UI_JPEG_len);
    uint8_t *out_buf = *(taskArgs.HTTP_UI_JPEG_buf);

    int32_t now_sends = 0;
    uint32_t packet_len = 1 * 1024;

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (client.write(out_buf, now_sends) == 0)
      {
        break;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
    }
    M5_LOGI("");
  }
  client.stop();
}
*/

void HTTP_UI_JPEG_cameraLineNow(EthernetClient client)
{
  M5_LOGI("");
  JpegItem jpegItem;
  if (xQueueJpeg_Last != NULL && xQueueReceive(xQueueJpeg_Last, &jpegItem, portMAX_DELAY) == pdPASS)
  {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    int32_t to_sends = jpegItem.len;
    uint8_t *out_buf = jpegItem.buf;

    int32_t now_sends = 0;
    uint32_t packet_len = 1 * 1024;

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (client.write(out_buf, now_sends) == 0)
      {
        break;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
    }
    free(jpegItem.buf);
    M5_LOGI("");
  }
  // client.stop();
}

void HTTP_UI_JPEG_STORE_Task(void *arg)
{
  M5_LOGD("");

  HTTP_UI_JPEG_STORE_TaskArgs *taskArgs = (HTTP_UI_JPEG_STORE_TaskArgs *)arg;
  *(taskArgs->HTTP_UI_JPEG_len) = 0;

  if (PoECAM.Camera.get())
  {
    int32_t frame_len = PoECAM.Camera.fb->len;
    uint8_t *frame_buf = PoECAM.Camera.fb->buf;
    pixformat_t pixmode = PoECAM.Camera.fb->format;
    u_int32_t fb_width = (u_int32_t)(PoECAM.Camera.fb->width);
    u_int32_t fb_height = (u_int32_t)(PoECAM.Camera.fb->height);

    taskArgs->pixmode = pixmode;
    taskArgs->fb_width = fb_width;
    taskArgs->fb_height = fb_height;

    if (*(taskArgs->HTTP_UI_JPEG_buf) != nullptr)
    {
      free(*(taskArgs->HTTP_UI_JPEG_buf));
    }

    *(taskArgs->HTTP_UI_JPEG_buf) = (uint8_t *)ps_malloc(frame_len);
    memcpy(*(taskArgs->HTTP_UI_JPEG_buf), frame_buf, frame_len);

    PoECAM.Camera.free();
    *(taskArgs->HTTP_UI_JPEG_len) = frame_len;
  }
  vTaskDelete(NULL);
  M5_LOGI("");
}
/*
uint8_t *HTTP_UI_JPEG_sensorImageNow_JPEG_buf;
int32_t HTTP_UI_JPEG_sensorImageNow_JPEG_len;
void HTTP_UI_JPEG_sensorImageNow(EthernetClient client)
{
  M5_LOGI("");

  HTTP_UI_JPEG_STORE_TaskArgs taskArgs = {&HTTP_UI_JPEG_sensorImageNow_JPEG_buf, &HTTP_UI_JPEG_sensorImageNow_JPEG_len};
  unit_flash_set_brightness(storeData.flashIntensityMode);
  digitalWrite(FLASH_EN_PIN, HIGH);

  delay(30);
  xTaskCreatePinnedToCore(HTTP_UI_JPEG_STORE_Task, "HTTP_UI_JPEG_STORE_Task", 4096, &taskArgs, 0, NULL, 0);
  delay(storeData.flashLength);
  digitalWrite(FLASH_EN_PIN, LOW);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
  client.println("Access-Control-Allow-Origin: *");
  client.println();

  if (*(taskArgs.HTTP_UI_JPEG_len) > 0)
  {
    int32_t to_sends = *(taskArgs.HTTP_UI_JPEG_len);
    uint8_t *out_buf = *(taskArgs.HTTP_UI_JPEG_buf);

    int32_t now_sends = 0;
    uint32_t packet_len = 1 * 1024;

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (client.write(out_buf, now_sends) == 0)
      {
        break;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
    }

    M5_LOGI("");
  }
  client.stop();
}
*/

void HTTP_UI_JPEG_sensorImageNow(EthernetClient client)
{
  M5_LOGI("");
  JpegItem jpegItem;
  if (xQueueJpeg_Last != NULL && xQueueReceive(xQueueJpeg_Last, &jpegItem, portMAX_DELAY) == pdPASS)
  {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Disposition: inline; filename=sensorImageNow.jpg");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    int32_t to_sends = jpegItem.len;
    uint8_t *out_buf = jpegItem.buf;

    int32_t now_sends = 0;
    uint32_t packet_len = 1 * 1024;

    while (to_sends > 0)
    {
      now_sends = to_sends > packet_len ? packet_len : to_sends;
      if (client.write(out_buf, now_sends) == 0)
      {
        break;
      }
      out_buf += now_sends;
      to_sends -= now_sends;
    }
    free(jpegItem.buf);
    M5_LOGI("");
  }
  // client.stop();
}

uint8_t *HTTP_UI_JPEG_flashTestJPEG;
int32_t HTTP_UI_JPEG_flashTestJPEG_len;
void HTTP_UI_JPEG_flashTestImage(EthernetClient client)
{
  M5_LOGI("");

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Content-Disposition: inline; filename=flashTestImage.jpg");
  client.println("Access-Control-Allow-Origin: *");
  client.println();

  uint32_t packet_len = 1 * 1024;
  int32_t now_sends = 0;
  int32_t to_sends = HTTP_UI_JPEG_flashTestJPEG_len;
  uint8_t *out_buf = HTTP_UI_JPEG_flashTestJPEG;

  while (to_sends > 0)
  {
    now_sends = to_sends > packet_len ? packet_len : to_sends;
    if (client.write(out_buf, now_sends) == 0)
    {
      break;
    }
    out_buf += now_sends;
    to_sends -= now_sends;
  }

  // client.stop();
  M5_LOGI("");
}

void HTTP_UI_STREAM_JPEG(EthernetClient client)
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
  // client.stop();
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

  client.println("<a href=\"/flashSwitch.html\">flashSwitch Page</a><br>");

  client.println("<br>");
  client.println("<hr>");
  client.println("<br>");

  client.println("<a href=\"/sensorValueNow.json\">sensorValueNow.json</a><br>");
  client.println("<a href=\"/unitTimeNow.json\">unitTimeNow.json</a><br>");
  client.println("<a href=\"/cameraLineNow.json\">cameraLineNow.json</a><br>");
  client.println("<a href=\"/sensorImageNow.jpg\">sensorImageNow.jpg</a><br>");

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

  client.println("<ul id=\"valueLabel\">");
  client.println("<li>unitTime: <span id=\"unitTime\"></span></li>");
  client.println("<li>edgePoint: <span id=\"edgePoint\"></span></li>");
  client.println("</ul>");

  client.println("<canvas id=\"cameraLineChart\" width=\"400\" height=\"100\"></canvas>");
  client.println("<canvas id=\"cameraImage\" width=\"400\"></canvas>");

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
  client.println("      document.getElementById('edgePoint').innerText = data.edgePoint;");
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
  client.println("        },");
  client.println("        y: {");
  client.println("          type: 'linear',");
  client.println("          position: 'right'");
  //  client.println("          display: false");
  client.println("        }");
  client.println("      },");
  client.println("      plugins: {");
  client.println("        legend: {");
  client.println("          display: false");
  client.println("        }");
  client.println("      }");
  client.println("    }");
  client.println("  });");
  client.println("}");

  uint32_t iWidth = (uint32_t)CameraSensorFrameWidth(storeData.framesize);
  uint32_t iHeight = (uint32_t)CameraSensorFrameHeight(storeData.framesize);
  uint32_t x1 = (uint32_t)((iWidth * (100 - storeData.pixLineRange)) / 200);
  uint32_t xw = (uint32_t)((iWidth * storeData.pixLineRange) / 100);
  uint32_t y1 = (uint32_t)((iHeight) / 2) - 1;

  client.println("function refreshImage() {");
  client.println("  var ctx = document.getElementById('cameraImage').getContext('2d');");
  client.println("  var img = new Image();");
  client.println("  img.onload = function() {");
  client.println("    var canvas = document.getElementById('cameraImage');");
  client.println("    canvas.height = img.height * (canvas.width / img.width);");
  client.println("    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);");
  client.println("    ctx.strokeStyle = 'red';");
  client.println("    ctx.lineWidth = 1;");
  client.printf("    ctx.strokeRect(%u * (canvas.width / img.width), %u * (canvas.height / img.height), %u * (canvas.width / img.width), 2 * (canvas.height / img.height));", x1, y1, xw);
  client.println("");
  client.println("  };");
  client.println("  img.src = '/cameraLineNow.jpg?' + new Date().getTime();"); // add timestamp
  client.println("}");

  client.println("function update() {");
  client.println("  fetchCameraLineData();");
  client.println("  refreshImage();");
  client.println("}");

  client.printf("setInterval(update, %u);", storeData.chartUpdateInterval);
  client.println("update();");
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
  client.printf("var distanceData = Array(%u).fill(0);\n", storeData.chartShowPointCount);
  client.println("var myChart = null;");
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var data = JSON.parse(xhr.responseText);");
  client.println("      document.getElementById('distance').innerText = data.distance;");
  client.println("      distanceData.push(data.distance);");
  client.printf("      if (distanceData.length > %u) { distanceData.shift(); }", storeData.chartShowPointCount);
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
  client.printf("setInterval(fetchData, %u);", storeData.chartUpdateInterval > 3000 ? storeData.chartUpdateInterval - 3000 : 1);
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

  HTML_PUT_LI_INPUT(imageBufferingInterval);

  HTML_PUT_LI_INPUT(ftpImageSaveInterval);
  HTML_PUT_LI_INPUT(ftpEdgeSaveInterval);
  HTML_PUT_LI_INPUT(ftpProfileSaveInterval);

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

  HTTP_GET_PARAM_FROM_POST(imageBufferingInterval);

  HTTP_GET_PARAM_FROM_POST(ftpImageSaveInterval);
  HTTP_GET_PARAM_FROM_POST(ftpEdgeSaveInterval);
  HTTP_GET_PARAM_FROM_POST(ftpProfileSaveInterval);

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
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineStep, "px [0-]");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineRange, "%");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeSearchStart, "0-100%");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeSearchEnd, "0-100%");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineEdgeUp, "1: dark -> light / 2: light -> dark");
  HTML_PUT_LI_INPUT_WITH_COMMENT(pixLineThrethold, "0-765");

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

  HTTP_GET_PARAM_FROM_POST(pixLineStep);
  HTTP_GET_PARAM_FROM_POST(pixLineRange);

  HTTP_GET_PARAM_FROM_POST(pixLineEdgeSearchStart);
  HTTP_GET_PARAM_FROM_POST(pixLineEdgeSearchEnd);
  HTTP_GET_PARAM_FROM_POST(pixLineEdgeUp);
  HTTP_GET_PARAM_FROM_POST(pixLineThrethold);

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

void HTTP_UI_PAGE_flashSwitch_Task(void *arg)
{
  M5_LOGD("");
  HTTP_UI_JPEG_flashTestJPEG_len = 0;
  if (PoECAM.Camera.get())
  {
    int32_t frame_len = PoECAM.Camera.fb->len;
    uint8_t *frame_buf = PoECAM.Camera.fb->buf;
    pixformat_t pixmode = PoECAM.Camera.fb->format;
    u_int32_t fb_width = (u_int32_t)(PoECAM.Camera.fb->width);
    u_int32_t fb_height = (u_int32_t)(PoECAM.Camera.fb->height);

    if (!HTTP_UI_JPEG_flashTestJPEG)
      free(HTTP_UI_JPEG_flashTestJPEG);

    HTTP_UI_JPEG_flashTestJPEG = (uint8_t *)ps_malloc(frame_len);
    memcpy(HTTP_UI_JPEG_flashTestJPEG, frame_buf, frame_len);

    PoECAM.Camera.free();
    HTTP_UI_JPEG_flashTestJPEG_len = frame_len;
  }
  vTaskDelete(NULL);
}

void HTTP_UI_PAGE_flashSwitch(EthernetClient client)
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

  String flashSwitchStatus = "OFF";
  String flashBrightnessStatus = "0";
  String flashTestLength = "10";

  HTTP_GET_PARAM_FROM_POST(flashSwitchStatus);
  HTTP_GET_PARAM_FROM_POST(flashBrightnessStatus);
  HTTP_GET_PARAM_FROM_POST(flashTestLength);
  uint8_t brightness_u = flashBrightnessStatus.toInt();
  uint32_t flashTestLength_u = flashTestLength.toInt();

  if (flashSwitchStatus == "ON")
  {
    M5_LOGW("FlashON: %u", brightness_u);
    unit_flash_set_brightness(brightness_u);
    digitalWrite(FLASH_EN_PIN, HIGH);

    delay(30);
    xTaskCreatePinnedToCore(HTTP_UI_PAGE_flashSwitch_Task, "HTTP_UI_PAGE_flashSwitch_Task", 4096, NULL, 0, NULL, 0);
    delay(flashTestLength_u);

    digitalWrite(FLASH_EN_PIN, LOW);
  }
  else
  {
    M5_LOGW("FlashOFF");
  }

  HTTP_UI_PART_ResponceHeader(client, "text/html");
  HTTP_UI_PART_HTMLHeader(client);

  client.println("<h1>" + deviceName + "</h1>");
  client.println("<br />");

  client.println("<form action=\"/flashSwitch.html\" method=\"post\">");
  client.println("<ul>");

  currentLine = "";
  flashSwitchStatus = "ON";
  HTML_PUT_LI_INPUT(flashSwitchStatus);
  HTML_PUT_LI_INPUT(flashTestLength);

  String optionString = " selected";

  client.println("<label for=\"flashBrightnessStatus\">Brightness:</label>");
  client.println("<select id=\"flashBrightnessStatus\" name=\"flashBrightnessStatus\">");

  client.printf("<option value=\"%d\" %s>Flashlight off</option>", 0, brightness_u == 0 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>100%% brightness + 220ms</option>", 1, brightness_u == 1 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>90%% brightness + 220ms</option>", 2, brightness_u == 2 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>80%% brightness + 220ms</option>", 3, brightness_u == 3 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>70%% brightness + 220ms</option>", 4, brightness_u == 4 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>60%% brightness + 220ms</option>", 5, brightness_u == 5 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>50%% brightness + 220ms</option>", 6, brightness_u == 6 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>40%% brightness + 220ms</option>", 7, brightness_u == 7 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>30%% brightness + 220ms</option>", 8, brightness_u == 8 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>100%% brightness + 1.3s</option>", 9, brightness_u == 9 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>90%% brightness + 1.3s</option>", 10, brightness_u == 10 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>80%% brightness + 1.3s</option>", 11, brightness_u == 11 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>70%% brightness + 1.3s</option>", 12, brightness_u == 12 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>60%% brightness + 1.3s</option>", 13, brightness_u == 13 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>50%% brightness + 1.3s</option>", 14, brightness_u == 14 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>40%% brightness + 1.3s</option>", 15, brightness_u == 15 ? "selected" : "");
  client.printf("<option value=\"%d\" %s>30%% brightness + 1.3s</option>", 16, brightness_u == 16 ? "selected" : "");

  client.println("</select><br>");

  client.println("<li class=\"button\">");
  client.println("<button type=\"submit\">Flash</button>");
  client.println("</li>");
  client.println("</ul>");
  client.println("</form>");

  client.println("<br />");
  client.printf("<a href=\"http://%s/top.html\">Return Top</a><br>", deviceIP_String.c_str());

  if (HTTP_UI_JPEG_flashTestJPEG_len > 0)
  {
    client.printf("<img src=\"/flashTestImage.jpg?%s\">", NtpClient.convertTimeEpochToString().c_str());
  }

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

void TaskRestart(void *arg)
{
  delay(5000);
  ESP.restart();
}

PageHandler pageHandlers[] = {
    {HTTP_UI_MODE_GET, "sensorValueNow.json", HTTP_UI_JSON_sensorValueNow},
    {HTTP_UI_MODE_GET, "unitTimeNow.json", HTTP_UI_JSON_unitTimeNow},
    {HTTP_UI_MODE_GET, "cameraLineNow.json", HTTP_UI_JSON_cameraLineNow},
    {HTTP_UI_MODE_GET, "cameraLineNow.jpg", HTTP_UI_JPEG_cameraLineNow},
    {HTTP_UI_MODE_GET, "sensorImageNow.jpg", HTTP_UI_JPEG_sensorImageNow},
    {HTTP_UI_MODE_GET, "flashTestImage.jpg", HTTP_UI_JPEG_flashTestImage},
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
    {HTTP_UI_MODE_GET, "flashSwitch.html", HTTP_UI_PAGE_flashSwitch},
    {HTTP_UI_MODE_POST, "flashSwitch.html", HTTP_UI_PAGE_flashSwitch},
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
    unsigned long millis0 = millis();
    unsigned long millis1 = millis0;

    unsigned long bmillis1 = millis0;
    unsigned long bmillis2 = millis0;
    unsigned long bmillis3 = millis0;
    unsigned long bmillis4 = millis0;

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

        millis0 = millis();

        if (c == '\n')
        {
          currentLineIsBlank = true, currentLine = "";
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false, currentLine += c;
        }

        bmillis1 = millis() - millis0;

        if (c == '\n' && currentLineIsBlank)
        {
          M5_LOGV("%s", page.c_str());
          if (getRequest)
            sendPage(client, page);
          else
            HTTP_UI_PAGE_notFound(client);
          break;
        }

        bmillis2 = millis() - millis0;

        if (currentLine.length() > 5 && (currentLine.startsWith("GET /") || currentLine.startsWith("POST /")))
        {
          for (size_t i = 0; i < numPages; i++)
          {
            String pageName = String(pageHandlers[i].page);
            String CheckLine = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? String("GET /") : String("POST /")) + pageName;
            if (currentLine.endsWith(CheckLine.c_str()))
            {
              //page = (pageHandlers[i].mode == HTTP_UI_MODE_GET ? currentLine.substring(5) : currentLine.substring(6));
              page =(pageHandlers[i].mode == HTTP_UI_MODE_GET ? CheckLine.substring(5) : CheckLine.substring(6));
              M5_LOGI("currentLine = [%s] : CheckLine = [%s]: page = [%s]", currentLine.c_str(), CheckLine.c_str(), page.c_str());
              getRequest = true;
              break;
            }
          }
        }
        bmillis3 = millis() - millis0;

        if (millis0 - millis1 >= 500)
        {
          M5_LOGE("%s : %u - %u, %u, %u", currentLine.c_str(), millis0 - millis1, bmillis1, bmillis2, bmillis3);
        }
        millis1 = millis0;
      }

      delay(1);
    }
    client.stop();
    M5_LOGV("client disconnected");
  }
}
