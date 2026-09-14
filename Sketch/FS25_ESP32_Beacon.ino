/*
 * FS25 ESP32 Beacon
 * Farming Simulator 25 - USB Beacon Light Emulator
 *
 * Emulates the Giants Software USB Beacon (VID: 0x340D, PID: 0x1710)
 * using a Waveshare ESP32-S3-Zero + 12x WS2812B NeoPixel ring.
 *
 * FS25 automatically sends HID output reports to control the beacon state.
 * The NeoPixel ring mirrors the in-game beacon:
 *   Mode 0x00  OFF
 *   Mode 0x01  ROUND (amber chasing sweep)
 *   Mode 0x07  BLINK (amber flash: two quick flashes then a long pause, repeating)
 *
 * Hardware:
 *   Board:     Waveshare ESP32-S3-Zero
 *   NeoPixels: 12x WS2812B ring on GPIO4
 *
 * Arduino IDE settings:
 *   Board:           ESP32S3 Dev Module
 *   USB Mode:        USB-OTG (TinyUSB)
 *   USB CDC On Boot: Enabled or Disabled both work - CDC is a separate
 *                    USB interface and doesn't affect the HID descriptor
 *                    FS25 checks against.
 *
 * To flash: hold BOOT while plugging in USB, then upload normally.
 *
 * Libraries: Adafruit NeoPixel, Arduino ESP32 core >= 3.x
 */

#include "USB.h"
#include "USBHID.h"
#include <Adafruit_NeoPixel.h>
extern USBHID HID;

// ── Hardware ──────────────────────────────────────────────────────────────────
#define LED_PIN     4
#define LED_COUNT   12
#define BRIGHTNESS  255  // 0-255, tuned for visibility through the housing

// ── Beacon colours ────────────────────────────────────────────────────────────
#define BEACON_AMBER  0xFF3700   // amber/orange, tuned through housing (R255 G55 B0)
#define BEACON_OFF    0x000000

// ── NeoPixel ─────────────────────────────────────────────────────────────────
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ── USB HID descriptor ────────────────────────────────────────────────────────
// Minimal HID descriptor: vendor-defined, 10-byte output report
static const uint8_t giants_hid_descriptor[] = {
  0x06, 0x00, 0xFF,   // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,         // Usage (Vendor Usage 1)
  0xA1, 0x01,         // Collection (Application)

  // No Report ID — framework only calls _onOutput when report_id == 0
  0x09, 0x01,         //   Usage (Vendor Usage 1)
  0x15, 0x00,         //   Logical Minimum (0)
  0x26, 0xFF, 0x00,   //   Logical Maximum (255)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x40,         //   Report Count (64) ← 64 input bytes
  0x81, 0x02,         //   Input (Data, Variable, Absolute)

  0x09, 0x01,         //   Usage (Vendor Usage 1)
  0x15, 0x00,         //   Logical Minimum (0)
  0x26, 0xFF, 0x00,   //   Logical Maximum (255)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x40,         //   Report Count (64) ← 64 output bytes → OutputReportByteLength=65
  0x91, 0x02,         //   Output (Data, Variable, Absolute)

  0xC0                // End Collection
};

// ── HID device class ─────────────────────────────────────────────────────────
class GiantsBeaconHID : public USBHIDDevice {
public:
  GiantsBeaconHID() {}

  void begin() {
    // Register with TinyUSB stack
    HID.addDevice(this, sizeof(giants_hid_descriptor));
  }

  uint16_t _onGetDescriptor(uint8_t* dst) override {
    memcpy(dst, giants_hid_descriptor, sizeof(giants_hid_descriptor));
    return sizeof(giants_hid_descriptor);
  }

  // Called by TinyUSB when the host sends an output report (i.e. the game)
  void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
    // No LED calls here — USB callbacks must not block
    // Real payload: [0xFF, mode, ...] — byte[0]=0xFF always, byte[1]=mode
    uint8_t mode = (len >= 2) ? buffer[1] : buffer[0];
    BeaconState newState = beaconState;
    if      (mode == 0x00) newState = STATE_OFF;
    else if (mode == 0x01) newState = STATE_ROUND;
    else if (mode == 0x07) newState = STATE_BLINK;

    if (newState == STATE_BLINK && beaconState != STATE_BLINK) {
      blinkResetRequested = true;
    }
    beaconState = newState;
  }

  volatile bool blinkResetRequested = false;

  enum BeaconState { STATE_OFF, STATE_ROUND, STATE_BLINK };
  volatile BeaconState beaconState = STATE_OFF;
};

USBHID HID;
GiantsBeaconHID beaconHID;


// ── Animation state ───────────────────────────────────────────────────────────
uint8_t  chasePos    = 0;
uint32_t lastStep    = 0;

#define CHASE_INTERVAL_MS   60    // speed of rotating sweep
#define CHASE_TAIL          4     // number of lit pixels in the sweep

// Blink sequence measured from real FS25 footage (60fps frame analysis):
// two quick flashes then a long pause, 883ms total cycle.
const uint32_t blinkSequence[]   = { 67, 67, 67, 682 };
const bool     blinkSequenceOn[] = { true, false, true, false };
#define BLINK_SEQUENCE_LEN  (sizeof(blinkSequence) / sizeof(blinkSequence[0]))

uint8_t  blinkStep     = 0;
uint32_t blinkStepStart = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Dim a colour by a 0-255 factor
uint32_t dimColour(uint32_t colour, uint8_t brightness) {
  uint8_t r = (uint8_t)(colour >> 16);
  uint8_t g = (uint8_t)(colour >>  8);
  uint8_t b = (uint8_t)(colour      );
  return ring.Color(
    (r * brightness) / 255,
    (g * brightness) / 255,
    (b * brightness) / 255
  );
}

void animateRound() {
  uint32_t now = millis();
  if (now - lastStep < CHASE_INTERVAL_MS) return;
  lastStep = now;

  ring.clear();
  for (int i = 0; i < CHASE_TAIL; i++) {
    int pixel = (chasePos - i + LED_COUNT) % LED_COUNT;
    // Fade the tail: brightest at head, dimmer toward tail
    uint8_t fade = 255 - (i * (255 / CHASE_TAIL));
    ring.setPixelColor(pixel, dimColour(BEACON_AMBER, fade));
  }
  ring.show();
  chasePos = (chasePos + 1) % LED_COUNT;
}

void animateBlink() {
  uint32_t now = millis();
  if (beaconHID.blinkResetRequested) {
    beaconHID.blinkResetRequested = false;
    blinkStep = 0;
    blinkStepStart = now;
    for (int i = 0; i < LED_COUNT; i++) ring.setPixelColor(i, BEACON_AMBER);
    ring.show();
    return;
  }
  if (now - blinkStepStart >= blinkSequence[blinkStep]) {
    blinkStep = (blinkStep + 1) % BLINK_SEQUENCE_LEN;
    blinkStepStart = now;

    if (blinkSequenceOn[blinkStep]) {
      for (int i = 0; i < LED_COUNT; i++) ring.setPixelColor(i, BEACON_AMBER);
    } else {
      ring.clear();
    }
    ring.show();
  }
}

void allOff() {
  ring.clear();
  ring.show();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  // Set USB VID/PID to match Giants beacon hardware
  USB.VID(0x340D);
  USB.PID(0x1710);
  USB.manufacturerName("GIANTS Software");
  USB.productName("Beacon");

  HID.begin();
  beaconHID.begin();
  USB.begin();

  ring.begin();
  ring.setBrightness(BRIGHTNESS);

  // Startup test: sweep amber around the ring to confirm hardware works
  for (int i = 0; i < LED_COUNT; i++) {
    ring.clear();
    ring.setPixelColor(i, BEACON_AMBER);
    ring.show();
    delay(60);
  }
  ring.clear();
  ring.show();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  switch (beaconHID.beaconState) {
    case GiantsBeaconHID::STATE_ROUND:
      animateRound();
      break;
    case GiantsBeaconHID::STATE_BLINK:
      animateBlink();
      break;
    case GiantsBeaconHID::STATE_OFF:
    default:
      allOff();
      break;
  }
}
