# RF–OWC–RF Wireless Sensor Communication System

This repository contains the firmware and Python scripts for an experimental **RF–Optical Wireless Communication–RF (RF–OWC–RF) interface** for low-power sensor data transmission.

The system receives sensor measurements over Bluetooth Low Energy (BLE), forwards them through an optical wireless link, and retransmits the recovered information via BLE to a data sink.

## 1. System Architecture

```text
  BLE Sensor (BTHome / Shelly)
             |
             | RF (BLE)
             v
     XIAO nRF52840
      (NodeR2O_TX)
             |
             | OWC (Infrared)
             v
     XIAO nRF52840
      (GatewayO2R)
             |
             | RF (BLE)
             v
         Data Sink
             |
             +--- Option A: XIAO nRF52840
             |              (RFSink)
             |
             +--- Option B: BLE-enabled PC
                            (ble_collector.py)
```

The RF–OWC–RF interface uses BLE for the first and final communication segments and a custom infrared optical protocol for the intermediate OWC link.

## 2. Firmware and Board Configuration

| File | Hardware | Arduino Board Core | Libraries |
|---|---|---|---|
| `NodeR2O_TX.ino` | XIAO nRF52840 | **Seeed nRF52 Boards (Non-mbed)** | Bluefruit, Adafruit TinyUSB |
| `GatewayO2R.ino` | XIAO nRF52840 | **Seeed nRF52 mbed-enabled Boards** | IRremote, ArduinoBLE |
| `RFSink.ino` | XIAO nRF52840 | **Seeed nRF52 mbed-enabled Boards** | ArduinoBLE |
| `ble_collector.py` | BLE-enabled PC | Not applicable | Python, Bleak |

**Note:** `RFSink.ino` is optional. A BLE-enabled PC can receive the measurements directly using `ble_collector.py` instead of a dedicated XIAO nRF52840 data sink.

## 3. Arduino IDE Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Open **File → Preferences**.
3. Add the following Boards Manager URL:

   ```
   https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
   ```

4. Open **Tools → Board → Boards Manager**.
5. Install both:
   - **Seeed nRF52 Boards (Non-mbed)**
   - **Seeed nRF52 mbed-enabled Boards**
6. Install the required libraries.
7. Select the corresponding board core from Section 2 before uploading each firmware.

## 4. Firmware Description

### NodeR2O_TX.ino — RF-to-OWC Interface

- Receives supported BTHome sensor advertisements over BLE.
- Extracts temperature, humidity, and battery information.
- Encodes sensor data into a custom 32-bit optical packet.
- Transmits the encoded data through an infrared LED.
- Supports CRC-8 error detection.

**Board core:** Seeed nRF52 Boards (Non-mbed).

### GatewayO2R.ino — OWC-to-RF Interface

- Receives optical packets through an IR receiver connected to D2.
- Decodes temperature, humidity, and battery information.
- Validates received packets using CRC-8.
- Forwards valid measurements via BLE notifications.
- Advertises as `OptoGateway`.

**Board core:** Seeed nRF52 mbed-enabled Boards.

### RFSink.ino — Optional RF Data Sink

- Connects to the OWC-to-RF gateway via BLE.
- Subscribes to sensor data notifications.
- Displays received measurements through the Arduino Serial Monitor at 115200 baud.

**Board core:** Seeed nRF52 mbed-enabled Boards.

### ble_collector.py — PC-Based RF Data Sink

An alternative to `RFSink.ino` that enables direct BLE reception on a PC without requiring an additional XIAO nRF52840.

## 5. Running the Python RF Data Collector

**Requirements:** Python 3 and a PC supporting Bluetooth Low Energy.

**Step 1 — Install Python**

Download and install Python from https://www.python.org/downloads/.

**Step 2 — Install Bleak**

Open Windows Command Prompt and execute:

```bash
py -m pip install bleak
```

**Step 3 — Run the collector**

Open Command Prompt in the folder containing `ble_collector.py` and execute:

```bash
py ble_collector.py
```

The script automatically discovers `OptoGateway`, establishes a BLE connection, and displays received measurements.

Example output:

```text
========================================
       OPTICAL SENSOR DATA
========================================
Packet No.  : 1
Time        : 12:30:15
Temperature : 25.6 C
Humidity    : 55 %
Battery     : 2 (0-3)
========================================
```

Press `Ctrl+C` to stop the collector.

## 6. Important Notes

- Use the **Non-mbed** board core for `NodeR2O_TX.ino`.
- Use the **mbed-enabled** board core for `GatewayO2R.ino` and `RFSink.ino`.
- Ensure matching CRC configurations between the optical transmitter and receiver.
- `RFSink.ino` and `ble_collector.py` are alternative implementations of the final RF data sink.
- The OWC segment uses a custom 32-bit packet format with CRC-8 error detection.
- The final BLE segment transfers a 6-byte binary payload containing temperature, humidity, and battery level.
- Only one BLE data collector should be connected to the gateway at a time.
- Firmware containing `while (!Serial)` requires an active USB Serial connection during startup unless modified for standalone operation.

## 7. References

- [Seeed Studio XIAO nRF52840 Documentation](https://wiki.seeedstudio.com/XIAO_BLE/)
- [Arduino IDE](https://www.arduino.cc/en/software)
- [Bleak Python BLE Library](https://bleak.readthedocs.io/)
