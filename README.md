# FS25 ESP32 Beacon

An ESP32-S3 firmware that emulates the Giants Software USB Beacon light for Farming Simulator 25. Plug it in and your NeoPixel ring mirrors the in-game beacon state automatically — no mods or scripts required.

![Assembled beacon](Images/Assy%20Image.png)

## How it works

FS25 has native support for a physical USB beacon accessory (VID `0x340D` / PID `0x1710`). When a vehicle beacon is toggled, the game sends HID output reports to the device. This firmware spoofs that device on an ESP32-S3 and drives a WS2812B NeoPixel ring accordingly.

The game sends a keepalive report every ~3 seconds while the beacon is active, and auto-offs the device after ~10 seconds without one.

## Hardware

> **Affiliate disclosure:** as an Amazon Associate I earn from qualifying purchases.

| Title | Description | Qty | Affiliate Link |
|-------|-------------|-----|----------------|
| Waveshare ESP32-S3-Zero | ESP32-S3FH4R2 MCU — 4MB flash, 2MB PSRAM, WiFi + BLE 5 | 1 | https://amzn.to/4wRNUeB Waveshare 2pc <br> https://amzn.to/4wIOBa7 Clone 1pc <br> https://amzn.to/45HKDnj Clone 3pc |
| NeoPixel ring | 12x WS2812B addressable LED ring | 1 | https://amzn.to/4A7DNFr |
| M2x6 SHCS | Socket head cap screw (for NeoPixel ring to base) | 4 | TBD |
| M3x12 SHCS | Socket head cap screw (for tube clamps) | 2 | TBD |
| M2x5 countersunk screw | Countersunk machine screw (for ESP cover to base) | 2 | TBD |
| 3M Command small strip | Mounting strip | 2 | https://amzn.to/46WvWgx |
| 3D printed enclosure | Custom design, see [`CAD/`](CAD/) | 1 | — |

Any ESP32-S3 board with TinyUSB support should work. WS2812B ring size is configurable via `LED_COUNT`.

## Assembly

![Exploded view](Images/Exploded%20Image.png)

1. Solder three wires to the ESP32-S3-Zero: data to **GPIO4**, plus **5V** and **GND**.
2. Feed the wires through the base, slot the ESP32 into the base, and secure it with the ESP cover (2x M2x5 countersunk).
3. Trim the wires to length and solder them to the NeoPixel ring.
4. Secure the NeoPixel ring to the base with 4x M2x6 SHCS.
5. Fit the lens and rotate clockwise to secure.
6. Press-fit the tube into the base.
7. Secure the tube clamps to the tube with 2x M3x12 SHCS.
8. Fit the 3M Command strips to the tube clamps.
9. Clean the mounting area (e.g. the back of a monitor) with IPA.
10. Stick the beacon in place.

## Beacon modes

| Game state | Mode byte | Animation |
|-----------|-----------|-----------|
| Beacon off | `0x00` | All off |
| Rotating beacon | `0x01` | Amber chase sweep with fading tail |
| Strobe beacon | `0x07` | Amber flash |

## Arduino IDE setup

1. Install **Arduino ESP32 core >= 3.x** via Boards Manager
2. Install **Adafruit NeoPixel** via Library Manager
3. Select board: `ESP32S3 Dev Module`
4. Set **USB Mode** to `USB-OTG (TinyUSB)`

The Arduino IDE requires a sketch's parent folder to have the exact same name as its `.ino` file. Copy or symlink `Sketch\FS25_ESP32_Beacon.ino` into a folder named `FS25_ESP32_Beacon` before opening it in the IDE.

To flash: hold the BOOT button while plugging in USB, then upload normally.

## game.xml

Ensure your FS25 `game.xml` (in `Documents\My Games\FarmingSimulator2025\`) has:

```xml
<usbBeaconLight enable="true"/>
```

## Notes

- The game scans for the device at startup and re-acquires it automatically if the device re-enumerates
- The HID report descriptor must advertise `OutputReportByteLength = 65` (64-byte output report) — this is what FS25 checks in its device validation code
- The `_onOutput` callback in the ESP32 Arduino TinyUSB framework is only triggered when `report_id == 0` (no Report ID in descriptor); do not add a Report ID
