/*
  XIAO nRF52840: Optical-to-BLE Gateway (Peripheral)
  Core: Seeed nRF52 mbed-enabled
  Requires: IRremote, ArduinoBLE

  Function:
  1. Receive optical IR packets on D2
  2. Decode temperature, humidity, and battery
  3. Validate CRC8
  4. Print received values to Serial Monitor
  5. Forward validated data via BLE
*/

#define IR_RECEIVE_PIN D2

#include <Arduino.h>
#include <IRremote.hpp>
#include <ArduinoBLE.h>

const bool ENABLE_CRC = true;

// ==========================================
// BLE CONFIGURATION
// ==========================================

const char* SERVICE_UUID =
  "19B10000-E8F2-537E-4F6C-D104768A1214";

const char* CHAR_UUID =
  "19B10001-E8F2-537E-4F6C-D104768A1214";

BLEService gatewayService(SERVICE_UUID);

// Packed payload for BLE transmission
struct __attribute__((packed)) SensorPayload {
  float temperature;
  uint8_t humidity;
  uint8_t battery;
};

SensorPayload currentData;

// BLE Characteristic
BLECharacteristic sensorChar(
  CHAR_UUID,
  BLERead | BLENotify,
  sizeof(SensorPayload)
);

// ==========================================
// CRC8 CALCULATION
// ==========================================

uint8_t calculateCRC8(uint8_t data[], uint8_t len) {

  uint8_t crc = 0x00;

  for (uint8_t i = 0; i < len; i++) {

    crc ^= data[i];

    for (uint8_t j = 0; j < 8; j++) {

      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  while (!Serial) {
    delay(10);
  }

  Serial.println();
  Serial.println("==================================");
  Serial.println(" XIAO nRF52840 Optical-BLE Gateway");
  Serial.println("==================================");

  // Initialize IR Receiver
  IrReceiver.begin(
    IR_RECEIVE_PIN,
    ENABLE_LED_FEEDBACK
  );

  Serial.println("IR Receiver Initialized");

  // Initialize BLE
  if (!BLE.begin()) {

    Serial.println("ERROR: BLE initialization failed!");

    while (1) {
      delay(1000);
    }
  }

  // BLE Server Configuration
  BLE.setLocalName("OptoGateway");

  BLE.setAdvertisedService(gatewayService);

  gatewayService.addCharacteristic(sensorChar);

  BLE.addService(gatewayService);

  // Initialize empty sensor data
  memset(
    &currentData,
    0,
    sizeof(SensorPayload)
  );

  sensorChar.writeValue(
    (uint8_t*)&currentData,
    sizeof(SensorPayload)
  );

  // Start BLE Advertising
  BLE.advertise();

  Serial.println("BLE Initialized");
  Serial.println("Advertising: OptoGateway");
  Serial.println();
  Serial.println("Waiting for optical packets...");
  Serial.println();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

  // Maintain BLE connection
  BLE.poll();

  // Check for IR packets
  if (IrReceiver.decode()) {

    if (IrReceiver.irparams.rawlen >= 67) {

      long headerMark =
        IrReceiver.irparams.rawbuf[1] * MICROS_PER_TICK;

      long headerSpace =
        IrReceiver.irparams.rawbuf[2] * MICROS_PER_TICK;

      // Check valid optical packet header
      if (
        headerMark > 1500 &&
        headerMark < 4500 &&
        headerSpace > 800 &&
        headerSpace < 2200
      ) {

        uint32_t decodedData = 0;
        int bitIndex = 0;

        // Decode 32-bit optical packet
        for (int i = 3; i < 67; i += 2) {

          long spaceDuration =
            IrReceiver.irparams.rawbuf[i + 1]
            * MICROS_PER_TICK;

          if (spaceDuration > 1400) {

            decodedData |=
              (1UL << (31 - bitIndex));
          }

          bitIndex++;
        }

        // ==================================
        // EXTRACT SENSOR VALUES
        // ==================================

        uint8_t receivedHeader =
          (decodedData >> 24) & 0xFF;

        uint8_t batLevel =
          (decodedData >> 22) & 0x03;

        uint8_t humidity =
          (decodedData >> 15) & 0x7F;

        uint16_t safe_temp =
          decodedData & 0x7FFF;

        float exact_temp =
          ((int32_t)safe_temp - 4000) * 0.1f;

        bool packetValid = false;

        // ==================================
        // CRC VALIDATION
        // ==================================

        if (ENABLE_CRC) {

          uint32_t data24 =
            decodedData & 0xFFFFFF;

          uint8_t dataToCheck[3] = {

            (uint8_t)((data24 >> 16) & 0xFF),

            (uint8_t)((data24 >> 8) & 0xFF),

            (uint8_t)(data24 & 0xFF)

          };

          if (
            calculateCRC8(dataToCheck, 3)
            == receivedHeader
          ) {

            packetValid = true;
          }

        } else {

          if (receivedHeader == 0xA5) {

            packetValid = true;
          }
        }

        // ==================================
        // VALID PACKET RECEIVED
        // ==================================

        if (packetValid) {

          Serial.println();
          Serial.println("========== IR PACKET RECEIVED ==========");

          // Raw packet
          Serial.print("Raw Data    : 0x");
          Serial.println(decodedData, HEX);

          // Temperature
          Serial.print("Temperature : ");
          Serial.print(exact_temp, 1);
          Serial.println(" C");

          // Humidity
          Serial.print("Humidity    : ");
          Serial.print(humidity);
          Serial.println(" %");

          // Battery
          Serial.print("Battery     : ");
          Serial.println(batLevel);

          // CRC
          Serial.println("CRC Status  : VALID");

          Serial.println("----------------------------------------");

          // ==================================
          // PREPARE BLE PAYLOAD
          // ==================================

          currentData.temperature = exact_temp;
          currentData.humidity = humidity;
          currentData.battery = batLevel;

          Serial.println("Forwarding data via BLE...");

          // ==================================
          // SEND DATA VIA BLE
          // ==================================

          bool sent = sensorChar.writeValue(
            (uint8_t*)&currentData,
            sizeof(SensorPayload)
          );

          if (sent) {

            Serial.println("BLE characteristic updated successfully.");

          } else {

            Serial.println("BLE characteristic update failed.");
          }

          Serial.println("========================================");
          Serial.println();
        }

        else {

          Serial.println("IR Packet Received: CRC FAILED");
        }
      }
    }

    // Ready for next optical packet
    IrReceiver.resume();
  }
}
