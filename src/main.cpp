#define DEBUG 0
#define WAIT_FOR_SERIAL 0

#include <Arduino.h>

#if SAMD

// Arduino on samd21 defines min and max differently than the STL so we need to undefine them
#undef min
#undef max
// since we're using the native port of the "arduino zero", not the programming port
#define Serial SerialUSB

#endif // SAMD

#include <SPI.h>
#include "wiring_private.h" // pinPeripheral() function
// SPIClass ledsSPI (&sercom3, LEDS_MISO, LEDS_SCK, LEDS_MOSI, SPI_PAD_2_SCK_3, SERCOM_RX_PAD_1);

// #include <I2S.h>
// I2SClass micI2S (0, MIC_CLK_GEN, MIC_SD, MIC_SCK, MIC_FS);

// static const uint8_t brightnessDialPort = PORTA;
// static const uint8_t brightnessDialPin = 9;

/* --------------------------------- */

#if HARDWARE_VERSION >= 2

// these pins are unconnected but exported in a pin header
#define UNCONNECTED_PIN_1 26
#define UNCONNECTED_PIN_2 27

#define LED_DATA_PIN 11
#define LED_CLK_PIN 10

#define TOUCH_PIN_0 0

#define PIN_PDM_DIN 18
#define PIN_PDM_CLK 19

#else // HARDWARE_VERSION

#if SAMD
#define LED_DATA_PIN 9
#define LED_CLK_PIN 8
#else
#define LED_DATA_PIN 3
#define LED_CLK_PIN 2
#endif // HARDWARE_VERSION

#if SAMD
#define UNCONNECTED_PIN_1 A4
#define UNCONNECTED_PIN_2 A5
#else
#define UNCONNECTED_PIN_1 A0
#define UNCONNECTED_PIN_2 A1
#endif
#endif


#define FASTLED_USE_PROGMEM 1
#define FASTLED_USE_GLOBAL_BRIGHTNESS 1
// #define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

#define LED_COUNT 369

#include <util.h>
#include <drawing.h>
#include <patterning.h>
#include "ledgraph.h"
#if SAMD
#include "motor.h"
#endif

#include "patterns.h"

#include <functional>

DrawingContext ctx;

FrameCounter fc;
PatternManager patternManager(ctx);

static bool serialTimeout = false;
static unsigned long setupDoneTime;

void setup() {
  Serial.begin(57600);
  
#if WAIT_FOR_SERIAL
  long setupStart = millis();
  while (!Serial) {
    if (millis() - setupStart > 10000) {
      serialTimeout = true;
      break;
    }
  }
  logf("begin - waited %ims for Serial", millis() - setupStart);
#elif DEBUG
  delay(2000);
  Serial.println("Done waiting at boot.");
#endif
  randomSeed(lsb_noise(UNCONNECTED_PIN_1, 8 * sizeof(uint32_t)));
  random16_add_entropy(lsb_noise(UNCONNECTED_PIN_2, 8 * sizeof(uint16_t)));

  FastLED.addLeds<SK9822HD, LED_DATA_PIN, LED_CLK_PIN, BGR, DATA_RATE_MHZ(16)>(ctx.leds, ctx.leds.size());

  fc.loop();

  patternManager.setup();
  patternManager.registerPattern<SwarmPattern>();
  patternManager.registerPattern<SpiralSource>();
  patternManager.registerPattern<WanderingFew>();

  patternManager.setupRandomRunner(50*1000, 2000);
  
  initLEDGraph();
  assert(ledgraph.adjList.size() == LED_COUNT, "adjlist size should match LED_COUNT");

  setupDoneTime = millis();
#if SAMD
  motorsetup();
#endif
}

void serialTimeoutIndicator() {
  FastLED.setBrightness(20);
  ctx.leds.fill_solid(CRGB::Black);
  if ((millis() - setupDoneTime) % 250 < 100) {
    ctx.leds.fill_solid(CRGB::Red);
  }
  FastLED.show();
  delay(20);
}

void loop() {
  if (serialTimeout && millis() - setupDoneTime < 1000) {
    serialTimeoutIndicator();
    return;
  }

  FastLED.setBrightness(25);

  patternManager.loop();
  
  FastLED.show();
#if SAMD
  motorloop();
#endif

  fc.loop();
  fc.clampToFramerate(120);
}
