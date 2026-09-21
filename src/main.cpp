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

// TouchPoints sit at the spiral centers on contiguous gpios; pad i is at HLSpiralCenters[i]
#define TOUCH_PIN_0 0
#define TOUCH_POINT_COUNT 3

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
#if defined(ARDUINO_ARCH_RP2040)
#include <apa102pio.h>
#endif
#include "ledgraph.h"
#if SAMD
#include "motor.h"
#endif

FrameCounter fc;

#define PDM_DATA 18 
#define PDM_CLK 19

#if HARDWARE_VERSION >= 2
#include <audio.h>
AudioInputPDM audioInput(PDM_DATA, PDM_CLK, true);
// TODO: fft numBins should be pattern-determined. how to rationalize this with a shared fft?
FFTProcessing fftProcessing(audioInput, 10, 128);

#include <controls.h>
TouchPIO touchPIO;
HardwareControls controls;
bool touchHeld[TOUCH_POINT_COUNT] = {0};
#endif

#include "patterns.h"

#include <functional>

DrawingContext ctx;

PatternManager patternManager(ctx);
CrossfadingPatternRunner *randomRunner = NULL;
static const unsigned long kPatternRunDuration = 50*1000;

#include "bench.h"

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

#if defined(ARDUINO_ARCH_RP2040)
  // FastLED bitbangs spi chipsets on rp2040; use dustlib's PIO+DMA transport at 16MHz instead
  static APA102PIOController<BGR> ledController(LED_DATA_PIN, LED_CLK_PIN, 16000000);
  FastLED.addLeds(&ledController, &ctx.leds[0], ctx.leds.size());
#else
  FastLED.addLeds<SK9822HD, LED_DATA_PIN, LED_CLK_PIN, BGR, DATA_RATE_MHZ(16)>(ctx.leds, ctx.leds.size());
#endif

  fc.loop();

  patternManager.setup();
  patternManager.registerPattern<SwarmPattern>();
  patternManager.registerPattern<SpiralSource>();
  patternManager.registerPattern<WanderingFew>();
#if HARDWARE_VERSION >= 2
  patternManager.registerPattern<SoundBits>(0, &SoundBits::wantsToRun);
#endif
  // patternManager.setTestRunner<SpasticTriad>();

  randomRunner = patternManager.setupRandomRunner(kPatternRunDuration, 2000);

#if HARDWARE_VERSION >= 2
  if (touchPIO.begin(TOUCH_PIN_0, TOUCH_POINT_COUNT)) {
    for (int i = 0; i < TOUCH_POINT_COUNT; ++i) {
      TouchButton *button = new TouchButton(touchPIO, i);
      button->onButtonDown([i]() { touchHeld[i] = true; });
      button->onButtonUp([i]() { touchHeld[i] = false; });
      controls.addControl(button);
    }
  }
  // overlay on top of the random runner for as long as a TouchPoint is held or its particles are still around
  patternManager.setupConditionalRunner<TouchParticleStream>([](PatternRunner &runner) -> uint8_t {
    for (bool held : touchHeld) {
      if (held) {
        return 0xFF;
      }
    }
    return (runner.pattern && !static_cast<TouchParticleStream *>(runner.pattern)->isIdle()) ? 0xFF : 0;
  }, 1);
#endif

  initLEDGraph();
  assert(ledgraph.adjList.size() == LED_COUNT, "adjlist size should match LED_COUNT");

  audioInput.subscribe();

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

#if HARDWARE_VERSION >= 2
  controls.update();
#endif

  BENCH_PERF_MARK();
  patternManager.loop();
  BENCH_PERF_ACCUM(benchPerfPatternUS);
  
  BENCH_PERF_MARK();
  FastLED.show();
  BENCH_PERF_ACCUM(benchPerfShowUS);
#if SAMD
  motorloop();
#endif

  fftProcessing.frameReset();

  benchLoop(readSerialLine());

  fc.loop();
  fc.clampToFramerate(benchClampFPS);
}
