#define DEBUG 0
#define WAIT_FOR_SERIAL 0

// for memory logging
#ifdef __arm__
extern "C" char* sbrk(int incr);
#else
extern char *__brkval;
#endif

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

#if SAMD
#define UNCONNECTED_PIN_1 A4
#define UNCONNECTED_PIN_2 A5
#else
#define UNCONNECTED_PIN_1 A0
#define UNCONNECTED_PIN_2 A1
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

void startupWelcome() {
  int pixelIndices[LED_COUNT];
  for (int i = 0; i < LED_COUNT; ++i) {
    pixelIndices[i] = i;
  }
  shuffle<int, LED_COUNT>(pixelIndices);

  const uint8_t colorShift = random8(2, 0xFF); // exclude 0,1 to avoid low color distribution
  const int sprinkleDuration = 500;
  const int eachPxDuration = 800;

  ctx.leds.fill_solid(CRGB::Black);

  DrawModal(120, sprinkleDuration + eachPxDuration, [pixelIndices, colorShift](unsigned long elapsed) {
    int maxPx = min(LED_COUNT, LED_COUNT * (int)elapsed / sprinkleDuration);
    
    for (int i = 0; i < maxPx; ++i) {
      int px = pixelIndices[i];
      CRGB color = ColorFromPalette((CRGBPalette32)Trans_Flag_gp, i * colorShift);
      unsigned long start = i * sprinkleDuration / LED_COUNT;
      if (elapsed > start && elapsed < start + eachPxDuration) {
        // set segment brightness based on sin [0,pi]
        uint8_t brightness = sin16((elapsed - start) * 0x7FFF / eachPxDuration) >> 8;
        color.nscale8(dim8_raw(brightness));
      } else {
        color = CRGB::Black;
      }
      
      ctx.leds[px] = color;
    }
  });
  ctx.leds.fill_solid(CRGB::Black);
  FastLED.show();
}

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
  random16_add_entropy(lsb_noise(UNCONNECTED_PIN_2, 8 * sizeof(uint16_t)));

  // use the alternate sercoms each of these SPI ports (SERCOM3)
  // pinPeripheral(LEDS_MISO, PIO_SERCOM_ALT);
  // pinPeripheral(LEDS_SCK,  PIO_SERCOM_ALT);
  // pinPeripheral(LEDS_MOSI, PIO_SERCOM_ALT);

  // FastLED.addLeds<SK9822, BGR>(ctx.leds, ctx.leds.size());
#if SAMD
  FastLED.addLeds<SK9822HD, 9, 8, BGR, DATA_RATE_MHZ(16)>(ctx.leds, ctx.leds.size());
#else
  FastLED.addLeds<SK9822HD, 3, 2, BGR, DATA_RATE_MHZ(16)>(ctx.leds, ctx.leds.size());
#endif

  fc.loop();

  patternManager.setup();
  patternManager.registerPattern<SwarmPattern>();
  patternManager.registerPattern<SpiralSource>();
  patternManager.registerPattern<WanderingFew>();

  patternManager.setupRandomRunner(50*1000);
  
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

  // static bool firstLoop = true;
  // if (firstLoop) {
  //   startupWelcome();
  //   firstLoop = false;
  // }

  FastLED.setBrightness(50);

  patternManager.loop();
  
  FastLED.show();
#if SAMD
  motorloop();
#endif

  fc.loop();
  fc.clampToFramerate(120);
}
