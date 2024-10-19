#include <bluefruit.h>

const uint8_t targetAddresses[2][6] = {
    {0x02, 0xDE, 0xF1, 0xB2, 0x46, 0xCF},
    {0xA7, 0x08, 0x0B, 0xE9, 0x61, 0xDC}
};

const int DATA_SIZE = 20;
const int moduleNum = 2;
const uint8_t startMarker[2] = {0xFF, 0xFF};
const uint8_t endMarker[2] = {0xFE, 0xFE};

struct PrphInfo {
    char name[17];
    uint16_t conn_handle;
    BLEClientUart bleuart;
    uint8_t data[DATA_SIZE];
    uint8_t byteIndex;
    bool isPacketStarted;
    uint32_t lastSequenceNumber;
    uint32_t missedPackets;
    uint32_t totalBytes;
    uint32_t totalPackets;
};

PrphInfo prphs[moduleNum];

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("Bluefruit52 Central Multi BLEUART Example");
    Bluefruit.begin(0, 4);
    Bluefruit.setName("Bluefruit52 Central");

    for (uint8_t idx = 0; idx < moduleNum; idx++) {
        prphs[idx].conn_handle = BLE_CONN_HANDLE_INVALID;
        prphs[idx].bleuart.begin();
        prphs[idx].bleuart.setRxCallback(bleuart_rx_callback);
    }

    Bluefruit.Central.setConnectCallback(connect_callback);
    Bluefruit.Central.setDisconnectCallback(disconnect_callback);
    Bluefruit.Scanner.setRxCallback(scan_callback);
    Bluefruit.Scanner.restartOnDisconnect(true);
    Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
    Bluefruit.Scanner.useActiveScan(false);
    Bluefruit.Scanner.start(0);
}

bool addressMatches(const ble_gap_evt_adv_report_t* report) {
    for (const auto& addr : targetAddresses) {
        if (memcmp(report->peer_addr.addr, addr, 6) == 0) return true;
    }
    return false;
}

void scan_callback(ble_gap_evt_adv_report_t* report) {
    if (addressMatches(report)) 
        Bluefruit.Central.connect(report);
    else 
        Bluefruit.Scanner.resume();
}

void connect_callback(uint16_t conn_handle) {
    int id = findConnHandle(BLE_CONN_HANDLE_INVALID);
    if (id < 0) return;

    auto& peer = prphs[id];
    peer.conn_handle = conn_handle;
    Bluefruit.Connection(conn_handle)->getPeerName(peer.name, sizeof(peer.name) - 1);
    Serial.print("Connected to ");
    Serial.println(peer.name);

    if (peer.bleuart.discover(conn_handle)) {
        peer.bleuart.enableTXD();
        if(id<1) Bluefruit.Scanner.start(0);
    } else {
        Bluefruit.disconnect(conn_handle);
    }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
    int id = findConnHandle(conn_handle);
    if (id < 0) return;

    prphs[id].conn_handle = BLE_CONN_HANDLE_INVALID;
    prphs[id].byteIndex = 0;
    prphs[id].isPacketStarted = false;
    prphs[id].lastSequenceNumber = 0xFFFFFFFF;
    Serial.print(prphs[id].name);
    Serial.println(" disconnected!");
}

void processPacket(PrphInfo& peer) {
    uint32_t receivedSequenceNumber = (peer.data[5] << 24) | (peer.data[4] << 16) | (peer.data[3] << 8) | (peer.data[2]);

    if (receivedSequenceNumber != peer.lastSequenceNumber + 1 && peer.lastSequenceNumber != 0xFFFFFFFF) {
        peer.missedPackets += (receivedSequenceNumber - peer.lastSequenceNumber - 1);
    }
    //Serial.println(receivedSequenceNumber);
    peer.lastSequenceNumber = receivedSequenceNumber;
    peer.totalPackets++;
}

void bleuart_rx_callback(BLEClientUart& uart_svc) {
    uint16_t conn_handle = uart_svc.connHandle();
    int id = findConnHandle(conn_handle);
    //Serial.println(id);
    if (id < 0) return;

    auto& peer = prphs[id];
    while(uart_svc.available()){
    uint8_t incomingByte = uart_svc.read();
    //Serial.println(incomingByte);
    peer.totalBytes++;

    if (!peer.isPacketStarted) {
        if (peer.byteIndex < 2 && incomingByte == startMarker[peer.byteIndex]) {
            peer.data[peer.byteIndex++] = incomingByte;
            if (peer.byteIndex == 2) {
                peer.isPacketStarted = true;
            }
        } else {
            peer.byteIndex = 0;
        }
    } else {
        peer.data[peer.byteIndex++] = incomingByte;

        if (peer.byteIndex == DATA_SIZE && peer.data[DATA_SIZE - 2] == endMarker[0] && peer.data[DATA_SIZE - 1] == endMarker[1]) {
            processPacket(peer);
            peer.byteIndex = 0;
            peer.isPacketStarted = false;
        }
    }
    }  
}

void loop() {
    static uint32_t startTime = 0;
    if (Bluefruit.Central.connected() && millis() - startTime >= 10000) {
        float elapsedTime = (millis() - startTime) / 1000.0;
        for (int i = 0; i < moduleNum; ++i) {
          if(prphs[i].conn_handle!=BLE_CONN_HANDLE_INVALID){
            
            Serial.printf("%d每分钟丢包数: %d\n", i + 1, prphs[i].missedPackets);
            Serial.printf("%d每分钟总包数: %d\n", i + 1, prphs[i].totalPackets);
            Serial.printf("%d传输速率: %.2f kbps\n", i + 1, (prphs[i].totalBytes * 8.0) / (elapsedTime * 1000.0));
            prphs[i].totalPackets = prphs[i].totalBytes = prphs[i].missedPackets = 0;
          }
        }
        startTime = millis();
    }
}

int findConnHandle(uint16_t conn_handle) {
    for (int id = 0; id < moduleNum; id++) {
        if (conn_handle == prphs[id].conn_handle) {
            return id;
        }
    }
    return -1;
}
