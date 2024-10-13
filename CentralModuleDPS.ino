#include <bluefruit.h>

uint8_t targetAddress[6] = {0x02, 0xDE, 0xF1, 0xB2, 0x46, 0xCF}; //固定的目标物理地址

BLEClientBas  clientBas;  // battery client
BLEClientDis  clientDis;  // device information client
BLEClientUart clientUart; // bleuart client

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
uint32_t startTime = 0;     // 开始时间

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("Initializing Bluefruit as Central...");
  Serial.println("--------------------------------\n");

  Bluefruit.begin(0, 1);
  Bluefruit.setName("CentralModule");

   // Configure Battery client
  clientBas.begin();  

  // Configure DIS client
  clientDis.begin();

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(receivedCallback);

  // Increase Blink rate to different from PrPh advertising mode
  Bluefruit.setConnLedInterval(250);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // // 0 = Don't stop scanning after n seconds
}

/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  /*
  Serial.print("Device Address: ");
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.println();
  */
  bool addressMatch = true;
  for (int i = 0; i < 6; i++) {
    if (report->peer_addr.addr[i] != targetAddress[i]) {
      addressMatch = false;
      break;
    }
  }

  if (addressMatch)
  {
    // Check if advertising contain BleUart service
    if ( Bluefruit.Scanner.checkReportForService(report, clientUart) )
    {
      Serial.print("BLE UART service detected. Connecting ... ");

      // Connect to device with bleuart service in advertising
      Bluefruit.Central.connect(report);
    }else
    {      
      // For Softdevice v6: after received a report, scanner will be paused
      // We need to call Scanner resume() to continue scanning
      Serial.println("Not Target");
    }
  }
  Bluefruit.Scanner.resume();
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected");

  Serial.print("Dicovering Device Information ... ");
  if ( clientDis.discover(conn_handle) )
  {
    Serial.println("Found it");
    char buffer[32+1];
    
    // read and print out Manufacturer
    memset(buffer, 0, sizeof(buffer));
    if ( clientDis.getManufacturer(buffer, sizeof(buffer)) )
    {
      Serial.print("Manufacturer: ");
      Serial.println(buffer);
    }

    // read and print out Model Number
    memset(buffer, 0, sizeof(buffer));
    if ( clientDis.getModel(buffer, sizeof(buffer)) )
    {
      Serial.print("Model: ");
      Serial.println(buffer);
    }

    Serial.println();
  }else
  {
    Serial.println("Found NONE");
  }

  Serial.print("Dicovering Battery ... ");
  if ( clientBas.discover(conn_handle) )
  {
    Serial.println("Found it");
    Serial.print("Battery level: ");
    Serial.print(clientBas.read());
    Serial.println("%");
  }else
  {
    Serial.println("Found NONE");
  }

  Serial.print("Discovering BLE Uart Service ... ");
  if ( clientUart.discover(conn_handle) )
  {
    Serial.println("Found it");

    Serial.println("Enable TXD's notify");
    clientUart.enableTXD();

    Serial.println("Ready to receive from peripheral");
    startTime = millis();
    missedPackets = 0;
    isConnected = true;
  }else
  {
    Serial.println("Found NONE");
    
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }  
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  byteIndex = 0; // 索引
  isPacketStarted = false; //开始标志
  lastSequenceNumber = -1; // 前一个数据包的序列号
  missedPackets = 0; // 丢包数
  totalBytes = 0; //总字节数
  totalPackets = 0; //总包数
  startTime = 0;     // 开始时间
  isConnected = false; //断开连接
}

void receivedCallback(BLEClientUart& uart) 
{ 
  while ( uart.available() )
  {
    uint8_t incomingByte = uart.read();
    //Serial.print(incomingByte);
    totalBytes++;
    
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
          uint32_t receivedSequenceNumber = (uint32_t)contentFrame[3] << 24 |
                            (uint32_t)contentFrame[2] << 16 |
                            (uint32_t)contentFrame[1] << 8  |
                            (uint32_t)contentFrame[0];

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
  }
}

void loop()
{
  if (isConnected){
    if (millis() - startTime >= 60000) 
    { 
      unsigned long currentTime = millis();
      float elapsedTime = (currentTime - startTime) / 1000.0;
      float kbps = (totalBytes * 8.0) / (elapsedTime * 1000.0);
      Serial.print("每分钟丢包数: ");
      Serial.println(missedPackets);
      Serial.print("每分钟总包数: ");
      Serial.println(totalPackets);
      Serial.print("传输速率: ");
      Serial.print(kbps);
      Serial.println(" kbps");
      totalPackets = 0;
      missedPackets = 0;  // 重置计数器
      startTime = millis();  // 重置计时器   
      totalBytes = 0;      
    }
  }
}