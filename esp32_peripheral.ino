#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
uint32_t sequenceNumber = 0; // 序列号
const int DATA_SIZE =80; // 定义数据块大小
uint8_t data[DATA_SIZE];


void sendData() {
  // 创建要发送的数据

  // Add the sequence number
  data[2] = (sequenceNumber >> 24) & 0xFF; // Most significant byte
  data[3] = (sequenceNumber >> 16) & 0xFF;
  data[4] = (sequenceNumber >> 8) & 0xFF;
  data[5] = sequenceNumber & 0xFF; // Least significant byte

  // Fill the middle part with some data
  for (int i = 6; i < 78; i++) {
    data[i] = (uint8_t)(i - 6); // Just an example, fill with incremental values
  }

  data[78] = 0xFE; // Footer byte 1
  data[79] = 0xFE; // Footer byte 2
    
  // 发送数据
  pCharacteristic->setValue(data, sizeof(data));
  pCharacteristic->notify(); // 通知客户端数据已更新
  sequenceNumber++; // 增加序列号
}

// 定时器回调函数
void onTimer(TimerHandle_t xTimer) {
    sendData();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  // Create the start of data packet 
  data[0] = 0xFF; // Header byte 1
  data[1] = 0xFF; // Header byte 2

  BLEDevice::init("Device1");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
    
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );

  //pCharacteristic->setValue("Hello World");

  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE Server started, waiting for clients...");

  // 创建定时器
  TimerHandle_t timer = xTimerCreate("DataTimer", pdMS_TO_TICKS(10), pdTRUE, (void *)0, onTimer);
  xTimerStart(timer, 0); // 启动定时器
}

void loop() {

}
