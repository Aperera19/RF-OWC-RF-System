/*
  XIAO nRF52840 BLE to 32-Bit IR Gateway (Variable CRC & High-Res Temp)
  Core: Seeed nRF52 Boards (Non-mbed)
*/

#define IR_SEND_PIN D3

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

// --- EXPERIMENT CONFIGURATION ---
// Set to 'true' for strict CRC-8 Validation.
// Set to 'false' to use static 0xA5 header (allows false acceptances for IEEE testing).
const bool ENABLE_CRC = true; 

// --- Custom IR Protocol Timings ---
#define HDR_MARK    3000
#define HDR_SPACE   1500
#define BIT_MARK    500
#define ONE_SPACE   2000
#define ZERO_SPACE  800

void scan_callback(ble_gap_evt_adv_report_t* report);
void parseBTHome(ble_gap_evt_adv_report_t* report, uint8_t* payload, uint8_t len);
void transmitDataIR(uint32_t payload);
uint8_t calculateCRC8(uint8_t data[], uint8_t len);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(IR_SEND_PIN, OUTPUT);
  digitalWrite(IR_SEND_PIN, LOW);

  Serial.print("Starting BLE to IR Gateway... CRC Mode: ");
  Serial.println(ENABLE_CRC ? "ON" : "OFF");

  Bluefruit.begin();
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.filterRssi(-90);
  Bluefruit.Scanner.start(0); 
}

void loop() {
  delay(1000);
}

// ==========================================
// CRC-8 CALCULATION
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
// IR TRANSMISSION LOGIC
// ==========================================
void mark(long microseconds) {
  long endTime = micros() + microseconds;
  while (micros() < endTime) {
    digitalWrite(IR_SEND_PIN, HIGH); delayMicroseconds(11); 
    digitalWrite(IR_SEND_PIN, LOW); delayMicroseconds(11);
  }
}

void space(long microseconds) {
  digitalWrite(IR_SEND_PIN, LOW);
  delayMicroseconds(microseconds);
}

void transmitDataIR(uint32_t payload) {
  Serial.print(">>> TRANSMITTING 32-BIT PAYLOAD: 0x");
  Serial.println(payload, HEX);

  for (int repeat = 0; repeat < 2; repeat++) {
    mark(HDR_MARK); space(HDR_SPACE);
    
    for (int i = 31; i >= 0; i--) {
      mark(BIT_MARK);
      if ((payload >> i) & 1) space(ONE_SPACE);  
      else space(ZERO_SPACE); 
    }
    
    mark(BIT_MARK); space(0); 
    delay(40);
  }
}

// ==========================================
// BLE SCANNING & DECODING LOGIC
// ==========================================
void scan_callback(ble_gap_evt_adv_report_t* report) {
  uint8_t buffer[32];
  uint8_t len = Bluefruit.Scanner.parseReportByType(report, 0x16, buffer, sizeof(buffer));
  if (len >= 2 && buffer[0] == 0xD2 && buffer[1] == 0xFC) {
    parseBTHome(report, buffer + 2, len - 2);
  }
  Bluefruit.Scanner.resume();
}

void parseBTHome(ble_gap_evt_adv_report_t* report, uint8_t* payload, uint8_t len) {
  if (len < 1) return;
  uint8_t bthome_info = payload[0];
  if (bthome_info != 0x40 && bthome_info != 0x44) return;

  uint8_t current_packet_id = 0;   bool found_packet_id = false;
  int16_t temp_raw = 0;            bool found_temperature = false;
  uint8_t battery = 0;             bool found_battery = false;
  uint8_t humidity = 0;            bool found_humidity = false;

  int i = 1;
  while (i < len) {
    uint8_t obj_id = payload[i++];
    if (obj_id == 0x00) { 
      current_packet_id = payload[i++]; found_packet_id = true;
    } 
    else if (obj_id == 0x01) { 
      battery = payload[i++]; found_battery = true;
    } 
    else if (obj_id == 0x2E) { 
      humidity = payload[i++]; found_humidity = true;
    } 
    else if (obj_id == 0x3A) { i++; } 
    else if (obj_id == 0x45) { 
      temp_raw = payload[i] | (payload[i+1] << 8); 
      i += 2; found_temperature = true;
    } 
    else { break; }
  }

  static uint8_t last_packet_id = 255; 
  static bool first_packet = true;

  if (found_packet_id) {
    if (!first_packet && current_packet_id == last_packet_id) return; 
    last_packet_id = current_packet_id; first_packet = false;
  }

  if (found_temperature && found_humidity && found_battery) { 
    
    // 1. Map Battery (0-100) to 4 Levels (0-3) to save bits
    uint8_t batLevel = 0;
    if (battery >= 75) batLevel = 3;
    else if (battery >= 45) batLevel = 2;
    else if (battery >= 15) batLevel = 1;
    else batLevel = 0;

    // 2. Offset the Temperature to avoid negative numbers bit-clashing
    // temp_raw is in 0.1°C steps. Adding 4000 safely shifts the floor down to -400.0 °C.
    // Example: 23.4 °C -> 234 -> + 4000 = 4234. Fits perfectly in 15 bits.
    uint16_t safe_temp = temp_raw + 4000;

    // 3. Pack into 24 data bits: [2 bits Bat] [7 bits Hum] [15 bits Temp]
    uint32_t data24 = (((uint32_t)batLevel & 0x03) << 22) | 
                      (((uint32_t)humidity & 0x7F) << 15) | 
                      ((uint32_t)safe_temp & 0x7FFF);

    // 4. Apply CRC or Static Header based on flag
    uint8_t headerByte = 0xA5; // Static header for "No CRC" testing
    
    if (ENABLE_CRC) {
      uint8_t payloadBytes[3] = {
        (uint8_t)((data24 >> 16) & 0xFF),
        (uint8_t)((data24 >> 8) & 0xFF),
        (uint8_t)(data24 & 0xFF)
      };
      headerByte = calculateCRC8(payloadBytes, 3);
    }

    // 5. Final 32-Bit Assembly
    uint32_t packet = ((uint32_t)headerByte << 24) | data24;
    
    Serial.println("\n--- New High-Res Shelly Reading ---");
    Serial.print("Exact Temp: "); Serial.print(temp_raw * 0.1, 1); Serial.println(" °C"); 
    Serial.print("Humidity:   "); Serial.print(humidity); Serial.println(" %"); 
    Serial.print("Bat Level:  "); Serial.print(batLevel); Serial.println(" (0-3)"); 
    transmitDataIR(packet);
    Serial.println("-----------------------------------");
  }
}
