#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


const char *ssid = "yourAP";
const char *password = "yourPassword";

const char *serverIP = "192.168.4.1";  // 服务器的IP地址
WiFiClient client;

const int DATA_SIZE =80; // 定义数据块大小
uint8_t packet[DATA_SIZE]; // Total 80 bytes
uint32_t sequenceNumber = 0; // 序列号
hw_timer_t * timer = NULL;

void onTimer(TimerHandle_t xTimer) {
  if (client.connected()) {

  // Add the sequence number
  packet[2] = (sequenceNumber >> 24) & 0xFF; // Most significant byte
  packet[3] = (sequenceNumber >> 16) & 0xFF;
  packet[4] = (sequenceNumber >> 8) & 0xFF;
  packet[5] = sequenceNumber & 0xFF; // Least significant byte

  // Fill the middle part with some data
  for (int i = 6; i < 78; i++) {
    packet[i] = (uint8_t)(i - 6); // Just an example, fill with incremental values
  }

  packet[78] = 0xFE; // Footer byte 1
  packet[79] = 0xFE; // Footer byte 2

  // Send the packet
  client.write(packet, sizeof(packet));
  Serial.println("Packet sent");

  sequenceNumber++; // Increment the sequence number
  } else {
    Serial.println("Connection lost. Reconnecting...");
    if (client.connect(serverIP, 80)) {
      Serial.println("Reconnected to server.");
    } else {
      Serial.println("Reconnection failed.");
    }
  }
}

void setup() {
  Serial.begin(115200);




  // 连接到WiFi接入点
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected to WiFi");

  // 连接到服务器
  if (client.connect(serverIP, 80)) {
    Serial.println("Connected to server.");
  } else {
    Serial.println("Connection failed.");
  }

  // 设置定时器
  TimerHandle_t timer = xTimerCreate("DataTimer", pdMS_TO_TICKS(10), pdTRUE, (void *)0, onTimer);
  xTimerStart(timer, 0); // 启动定时器
}

void loop() {
  // 主循环可以保持空白，所有工作都在定时器中进行
}
