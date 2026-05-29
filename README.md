# FS25 ESP32 Beacon

An ESP32-S3 firmware that emulates the Giants Software USB Beacon light for Farming Simulator 25. Plug it in and your NeoPixel ring mirrors the in-game beacon state automatically — no mods or scripts required.

## How it works

FS25 has native support for a physical USB beacon accessory (VID `0x340D` / PID `0x1710`). When a vehicle beacon is toggled, the game sends HID output reports to the device. This firmware spoofs that device on an ESP32-S3 and drives a WS2812B NeoPixel ring accordingly.

The game sends a keepalive report every ~3 seconds while the beacon is active, and auto-offs the device after ~10 seconds without one.

## Hardware

| Part | Details |
|------|---------|
| Microcontroller | ESP32-S3 Super Mini |
| LED ring | 12x WS2812B NeoPixel, GPIO4 |
| Colour | Amber `#FF3700` |

Any ESP32-S3 board with TinyUSB support should work. WS2812B ring size is configurable via `LED_COUNT`.

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
5. Set **USB CDC On Boot** to `Disabled`

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
