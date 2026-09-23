#ifndef HWTEST_H
#define HWTEST_H

// HWTEST serial command: a few-second hardware self-test for unit intake, driven by ./hwtest on the host.
// The firmware only measures; every line is "HWTEST <section> key=value ..." and the host applies the limits and keeps the
// log, so limits can move without reflashing and the raw numbers accumulate into a spread across units.
//
// Sequence, owning the pixels the whole time:
//   1. dark, then full red / green / blue / white, so a person can spot dead or stuck pixels (a break in the chain shows
//      as everything after it dark). The microphone is sampled throughout and the touch pads must all read untouched.
//   2. unless started as "HWTEST QUICK": each spiral lights in turn and the test waits for its center pad to be touched
//      (green once it is; lift to move on). A pad that never responds is reported as such.
//   3. one line per section, then END.
// Include after main.cpp's audioInput, touchPIO and ctx exist.

#if HARDWARE_VERSION >= 2

const char *kHWTestCommand = "HWTEST";

class HWTest {
  static const unsigned long kDarkMS = 400, kColorMS = 700;    // dark, then 4 colors: 3.2s of pixels
  static const unsigned long kTouchWaitMS = 30000;             // per pad, to touch it and again to let go: the first one comes right after the color sweep
  // whole-string fills are the heaviest thing this board draws; keep them modest on a USB-powered bench
  static const uint8_t kColorBrightness = 20, kWhiteBrightness = 10, kSpiralBrightness = 25;
  unsigned long startedAt = 0;
  bool running = false;
  bool quick = false;
  // microphone, over every sample that arrived during the test
  uint32_t micSeen = 0, micSamples = 0, micFrames = 0, micEmptyFrames = 0;
  int32_t micMin = 0, micMax = 0;
  double micSum = 0, micSumSq = 0;
  // touch: what the pads read while nobody should be touching them (the pixel phase)
  uint32_t idleTouchOr = 0, idleTouchFrames = 0;
  // touch: the pad-by-pad phase
  int touchPad = 0;
  bool touchWaitingForRelease = false;
  unsigned long touchPadStartedAt = 0;
  bool touchSeen[TOUCH_POINT_COUNT] = {0};
  unsigned long touchSeenMS[TOUCH_POINT_COUNT] = {0};
  uint32_t touchStrayOr = 0;  // pads other than the one being asked for that read touched while it was untouched

  void sampleMic() {
    audioInput.update();
    uint32_t seen = audioInput.samplesSeen();
    uint32_t fresh = min(seen - micSeen, (uint32_t)AudioProcessing::windowSize);
    micSeen = seen;
    micFrames++;
    if (fresh == 0) {
      micEmptyFrames++;
      return;
    }
    const int16_t *window = audioInput.samples() + AudioProcessing::windowSize - fresh;
    for (uint32_t i = 0; i < fresh; ++i) {
      if (micSamples == 0 || window[i] < micMin) micMin = window[i];
      if (micSamples == 0 || window[i] > micMax) micMax = window[i];
      micSum += window[i];
      micSumSq += (double)window[i] * window[i];
      micSamples++;
    }
  }

  void fillSpiral(int s, CRGB color) {
    for (int i = 0; i < SPIRAL_LED_COUNT; ++i) ctx.leds[HLSpiralCenters[s] + i] = color;
  }

  void show(CRGB color, uint8_t brightness) {
    ctx.leds.fill_solid(color);
    FastLED.setBrightness(brightness);
    FastLED.show();
  }

  // the pad-by-pad phase; true while it still has something to do
  bool touchPhase(unsigned long t) {
    if (quick || touchPad >= TOUCH_POINT_COUNT) return false;
    if (touchPadStartedAt == 0) touchPadStartedAt = t;
    uint32_t state = touchPIO.state();
    bool touched = state & (1u << touchPad);
    if (!touchWaitingForRelease) {
      touchStrayOr |= state & ~(1u << touchPad);
      if (touched) {
        touchSeen[touchPad] = true;
        touchSeenMS[touchPad] = t - touchPadStartedAt;
        touchWaitingForRelease = true;
      }
    }
    bool done = touchWaitingForRelease ? !touched : false;
    if (done || t - touchPadStartedAt >= (touchWaitingForRelease ? 2 : 1) * kTouchWaitMS) {
      touchPad++;
      touchWaitingForRelease = false;
      touchPadStartedAt = t;
      return touchPad < TOUCH_POINT_COUNT;
    }
    ctx.leds.fill_solid(CRGB::Black);
    fillSpiral(touchPad, touchWaitingForRelease ? CRGB::Green : CRGB(0, 80, 255));
    FastLED.setBrightness(kSpiralBrightness);
    FastLED.show();
    return true;
  }

  void report() {
    double mean = micSamples ? micSum / micSamples : 0;
    logf("HWTEST mic streaming=%i samples=%lu frames=%lu empty_frames=%lu min=%li max=%li mean=%.1f rms=%.1f peak=%i", audioInput.isStreaming(),
         micSamples, micFrames, micEmptyFrames, micMin, micMax, mean, micSamples ? sqrt(max(0.0, micSumSq / micSamples - mean * mean)) : 0.0,
         audioInput.peakAmplitude());
    char pads[96] = "";
    for (int p = 0, n = 0; p < TOUCH_POINT_COUNT; ++p) {
      n += snprintf(pads + n, sizeof(pads) - n, " seen%i=%i ms%i=%lu", p, touchSeen[p], p, touchSeenMS[p]);
    }
    logf("HWTEST touch pio=%i sm=%i idle_state=0x%lx idle_frames=%lu pads=%i tested=%i stray=0x%lx%s", touchPIO.pio() ? (int)pio_get_index(touchPIO.pio()) : -1,
         touchPIO.sm(), idleTouchOr, idleTouchFrames, TOUCH_POINT_COUNT, !quick, touchStrayOr, pads);
    logf("HWTEST END ms=%lu", millis() - startedAt);
  }

public:
  bool active() { return running; }

  void begin(bool quickTest) {
    char serialNumber[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(serialNumber, sizeof(serialNumber));
    *this = HWTest();
    running = true;
    quick = quickTest;
    startedAt = millis();
    micSeen = audioInput.samplesSeen();
    // the UF2 bootloader, the 1200-baud touch and rp2040.reboot() all reboot through the watchdog, so watchdog_caused_reboot()
    // is true on every first boot after a flash; only a reboot from an enabled watchdog timing out is worth reporting
    logf("HWTEST BEGIN sn=%s hw=%i leds=%i uptime_ms=%lu watchdog_reboot=%i quick=%i", serialNumber, HARDWARE_VERSION, LED_COUNT, millis(),
         watchdog_enable_caused_reboot(), quick);
#if LOG_BOOT_CAPTURE_BYTES
    // what was logged before anyone was listening (failed mic or touch init lands here), a line at a time
    char line[200];
    size_t n = 0;
    for (const char *c = bootLog(); ; ++c) {
      if (*c == '\n' || *c == '\0' || n == sizeof(line) - 1) {
        line[n] = '\0';
        if (n && strncmp(line, "HWTEST", 6) != 0) logf("HWTEST BOOT %s", line);
        n = 0;
        if (*c == '\0') break;
      } else if (*c != '\r') {
        line[n++] = *c;
      }
    }
#endif
  }

  // core0, every frame while active, in place of the pattern pipeline
  void loop() {
    unsigned long t = millis() - startedAt;
    const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    const unsigned long pixelsMS = kDarkMS + ARRAY_SIZE(colors) * kColorMS;
    sampleMic();
    if (t < pixelsMS) {
      idleTouchOr |= touchPIO.state();
      idleTouchFrames++;
      CRGB color = t < kDarkMS ? CRGB::Black : colors[(t - kDarkMS) / kColorMS];
      show(color, color == CRGB(CRGB::White) ? kWhiteBrightness : kColorBrightness);
      return;
    }
    if (touchPhase(t - pixelsMS)) return;
    show(CRGB::Black, kColorBrightness);
    report();
    running = false;
  }
};
HWTest hwTest;

#endif // HARDWARE_VERSION >= 2
#endif
