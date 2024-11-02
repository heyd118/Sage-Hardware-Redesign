#include <WiFi.h>

const char *ssid = "yourAP";
const char *password = "yourPassword";
WiFiServer server(80);

const int DATA_SIZE = 80; // 定义数据块大小
const uint8_t startMarker[2] = {0xFF, 0xFF}; // 帧开始
const uint8_t endMarker[2] = {0xFE, 0xFE}; // 帧结束
uint8_t data[DATA_SIZE]; // 数据包缓冲区
uint8_t contentFrame[4]; //序列号存储缓存
int byteIndex = 0; // 索引
bool isPacketStarted = false; //开始标志
volatile bool isConnected = false; //是否连接
uint32_t lastSequenceNumber = -1; // 前一个数据包的序列号
uint32_t missedPackets = 0; // 丢包数
uint32_t totalBytes = 0; //总字节数
uint32_t totalPackets = 0; //总包数
unsigned long lastPrintTime = 0;


void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  server.begin();
  Serial.println("Access Point started. IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    
    while (client.connected()) {
      if (client.available()) {
        uint8_t incomingByte = client.read();
        if (!isPacketStarted) 
        {
          // 检查开始标志
          if (byteIndex == 0 && incomingByte == startMarker[0]) 
          {
            data[byteIndex++] = incomingByte;
          } 
          else if (byteIndex == 1 && incomingByte == startMarker[1]) 
          {
            data[byteIndex++] = incomingByte;
            isPacketStarted = true;  // 正确的开始标志已检测到
          } 
          else 
          {
            byteIndex = 0;  // 不是有效的开始标志，重置索引
          }
        } 
        else 
        {
           // 包的中间部分
          data[byteIndex++] = incomingByte;

          if (byteIndex == DATA_SIZE) 
          {
            // 检查结束标志
            if (data[DATA_SIZE - 2] == endMarker[0] && data[DATA_SIZE - 1] == endMarker[1]) 
            {
              // 提取内容帧的前4个字节
              for (int i = 0; i < 4; i++) {
                contentFrame[i] = data[2 + i];
              }

              // 将提取的4个字节转换为uint32_t
              uint32_t receivedSequenceNumber = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];

              //输出转换后的uint32_t值
              //Serial.print("序列号: ");
              //Serial.println(receivedSequenceNumber, DEC);
          
              if (receivedSequenceNumber != lastSequenceNumber + 1 && lastSequenceNumber != -1) 
              {
                missedPackets += (receivedSequenceNumber - lastSequenceNumber - 1);
              }

              lastSequenceNumber = receivedSequenceNumber;
              totalPackets++;
            }
            // 重置状态以准备接收下一个数据包
            byteIndex = 0;
            isPacketStarted = false;
          }
        }
        // 打印丢包数

        
      }
      if (millis() - lastPrintTime >= 10000) {
        Serial.print("Lost packets in the last 10s: ");
        Serial.println(missedPackets);
        Serial.print("Total packets: ");
        Serial.println(totalPackets);
        missedPackets = 0; // 重置丢包计数
        totalPackets = 0;
        lastPrintTime = millis();
        }

    }
    client.stop();
    Serial.println("Client Disconnected.");  
    
  }


}
