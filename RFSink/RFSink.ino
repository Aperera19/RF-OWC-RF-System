/*
  XIAO nRF52840: BLE Data Collector (Central)
  Core: Seeed nRF52 mbed-enabled
  Requires: ArduinoBLE
*/

#include <Arduino.h>
#include <ArduinoBLE.h>

// Must perfectly match Node 1
const char* TARGET_SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
const char* TARGET_CHAR_UUID    = "19B10001-E8F2-537E-4F6C-D104768A1214";

// Exact same memory structure to unpack the incoming binary payload
struct __attribute__((packed)) SensorPayload {
  float temperature;
  uint8_t humidity;
  uint8_t battery;
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("❌ starting Bluetooth® Low Energy module failed!");
    while (1);
  }

 // Serial.println("✅ Collector Node Active. Scanning for Optical Gateway...");
  
  // By scanning specifically for this UUID, we ignore all other BLE noise
  BLE.scanForUuid(TARGET_SERVICE_UUID);
}

void loop() {
  // Check if a peripheral was discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    Serial.print("Discovered Optical Gateway! MAC: ");
    Serial.println(peripheral.address());

    // Stop scanning since we found our target
    BLE.stopScan();

    // Take control of the gateway
    connectToGateway(peripheral);

    // If connection drops, start scanning again
    Serial.println("Connection lost. Resuming scan...");
    BLE.scanForUuid(TARGET_SERVICE_UUID);
  }
}

void connectToGateway(BLEDevice peripheral) {
  if (peripheral.connect()) {
    Serial.println("Optical-BLE Gateway Connected");
  } else {
    Serial.println("❌ Failed to connect!");
    return;
  }

  // Discover peripheral attributes
  if (peripheral.discoverAttributes()) {
    //Serial.println("Attributes discovered.");
  } else {
    Serial.println("❌ Attribute discovery failed!");
    peripheral.disconnect();
    return;
  }

  // Retrieve the specific characteristic
  BLECharacteristic sensorChar = peripheral.characteristic(TARGET_CHAR_UUID);

  if (!sensorChar) {
    Serial.println("❌ Target characteristic not found on peripheral!");
    peripheral.disconnect();
    return;
  }

  // Subscribe to receive automatic updates (Notifications)
  if (!sensorChar.canSubscribe()) {
    Serial.println("❌ Characteristic cannot be subscribed to!");
    peripheral.disconnect();
    return;
  }
  
  sensorChar.subscribe();
  Serial.println("Waiting for optical data...\n");

  // Keep the connection alive and listen for data updates
  while (peripheral.connected()) {
    
    // Check if the gateway updated the characteristic (a new IR packet arrived)
    if (sensorChar.valueUpdated()) {
      
      SensorPayload incomingData;
      
      // Read the raw bytes into our structured payload
      sensorChar.readValue((uint8_t*)&incomingData, sizeof(SensorPayload));

      // Print the final result to the Serial Monitor
      Serial.println("========================================");
      Serial.println("NEW OPTICAL PACKET RECEIVED VIA BLE");
      Serial.print("Temperature: "); Serial.print(incomingData.temperature, 1); Serial.println(" °C");
      Serial.print("Humidity:    "); Serial.print(incomingData.humidity); Serial.println(" %");
      Serial.print("Battery Lvl: "); Serial.print(incomingData.battery); Serial.println(" (0-3)");
      Serial.println("========================================");
    }
  }
}
