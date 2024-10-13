#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "NRF52TimerInterrupt.h" 


// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery
NRF52Timer ITimer0(NRF_TIMER_1); //Timer

#define TIMER_FREQ_HZ  100  //定时器Hz

const int DATA_SIZE =80; // 定义数据块大小
uint8_t data[DATA_SIZE];  // 创建数据块
uint32_t sequenceNumber = 0; // 序列号

volatile bool sendDataFlag = false;  // 标志位，用于控制是否发送数据
uint32_t totalPackets = 0; //总包数


void TimerHandler0(void)
{
  sendDataFlag = true;
}

void setup()
{
  Serial.begin(115200);
  //while (!Serial);

#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif
  
  Serial.println("Initializing Bluefruit as Peripheral...");
  Serial.println("---------------------------\n");

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  // Maximize MTU size for higher throughput
  //Bluefruit.configPrphMTU(247);

  Bluefruit.begin();
  Bluefruit.setTxPower(8);    // Check bluefruit.h for supported values
  Bluefruit.setName("PeripheraModule"); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // 初始化数据块
  data[0] = 0xFF; data[1] = 0xFF;
  for (int i = 2; i < DATA_SIZE-2; i++) {
    data[i] = i;
  }
  data[DATA_SIZE-2] = 0xFE; data[DATA_SIZE-1] = 0xFE;

  // Frequency in float Hz
  ITimer0.attachInterrupt(TIMER_FREQ_HZ, TimerHandler0);
  ITimer0.disableTimer();
  sendDataFlag = false;
  

  /*
  if (ITimer.attachInterrupt(TIMER_FREQ_HZ, TimerHandler))
  {
    Serial.print("Starting ITimer OK, millis() = "); Serial.println(millis());
  }
  else
    Serial.println("Can't set ITimer. Select another freq. or timer");
  */

  // Set up and start advertising
  startAdv();


}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode

  // BLE connection interval optimization
  Bluefruit.Periph.setConnInterval(6, 9);  // New: Min = 7.5ms, Max = 11.25ms

  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{
  if (bleuart.notifyEnabled())
  {
    if (sendDataFlag)
    {
      sendDataFlag = false;  // 重置标志位    
      memcpy(data+2, &sequenceNumber, sizeof(sequenceNumber));
      sequenceNumber++;
      bleuart.write(data, DATA_SIZE);
      totalPackets++;
    }   
  } 
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);

  //设置 PHY 为 2M
    
  if (Bluefruit.Connection(conn_handle)->requestPHY(BLE_GAP_PHY_2MBPS)) {
    Serial.println("Successfully set PHY to 2M");
  } else {
    Serial.println("Failed to set PHY to 2M, using default");
  }

  delay(100);
  ITimer0.enableTimer(); 
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  ITimer0.disableTimer();
  //Serial.println();
  //Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);  
  sendDataFlag = false;
  sequenceNumber = 0;
  totalPackets = 0;
}


