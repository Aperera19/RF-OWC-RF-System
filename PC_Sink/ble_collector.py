
import asyncio
import struct
from datetime import datetime
from bleak import BleakScanner, BleakClient

# ==========================================
# BLE CONFIGURATION
# ==========================================

DEVICE_NAME = "OptoGateway"

CHAR_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214"

packet_count = 0

# ==========================================
# RECEIVE AND DECODE DATA
# ==========================================

def on_notification(sender, data):
    global packet_count

    if len(data) != 6:
        print("Invalid packet length:", len(data))
        return

    # Decode binary data from XIAO nRF52840
    temperature, humidity, battery = struct.unpack(
        "<fBB", data
    )

    packet_count += 1

    timestamp = datetime.now().strftime("%H:%M:%S")

    print("\n========================================")
    print("       OPTICAL SENSOR DATA")
    print("========================================")

    print(f"Packet No.  : {packet_count}")
    print(f"Time        : {timestamp}")
    print(f"Temperature : {temperature:.1f} C")
    print(f"Humidity    : {humidity} %")
    print(f"Battery     : {battery} (0-3)")

    print("========================================")


# ==========================================
# MAIN BLE COLLECTOR
# ==========================================

async def main():

    print("========================================")
    print("  Optical-to-BLE Python Collector")
    print("========================================")

    print("\nScanning for OptoGateway...")

    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME,
        timeout=20.0
    )

    if device is None:
        print("ERROR: OptoGateway not found!")
        print("Check Bluetooth and gateway power.")
        return

    print("Gateway Found!")
    print("Address:", device.address)

    try:
        async with BleakClient(device) as client:

            print("\nConnected to OptoGateway!")
            print("Subscribing to sensor notifications...")

            await client.start_notify(
                CHAR_UUID,
                on_notification
            )

            print("\nWaiting for optical packets...")
            print("Press Ctrl+C to stop.\n")

            while client.is_connected:
                await asyncio.sleep(1)

            print("Gateway disconnected.")

    except Exception as e:
        print("BLE error:", e)


# ==========================================
# START PROGRAM
# ==========================================

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nCollector stopped.")
