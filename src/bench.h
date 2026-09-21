#ifndef BENCH_H
#define BENCH_H

// Serial bench tools. Commands, one per line:
//   PATTERN <n>  crossfade to pattern index n and hold it (no auto-advance)
//   AUTO         resume random auto-advance
//   FAKEFFT <n>  drive the fft with a synthetic spectrum at level n, 0 to stop
//   PERF         toggle per-section frame timing logs
//   FPS <n>      clamp the main loop to n fps to check patterns for framerate invariance, 0 to unclamp
//   REBOOT

static int benchClampFPS = 0;
static bool benchPerfLogging = false;
static uint32_t benchPerfPatternUS = 0, benchPerfShowUS = 0, benchPerfFrames = 0;
static unsigned long benchPerfMark = 0;

#define BENCH_PERF_MARK() (benchPerfMark = micros())
#define BENCH_PERF_ACCUM(accum) ((accum) += micros() - benchPerfMark)

static inline void benchLoop(char *serialLine) {
  if (serialLine) {
    if (strncmp(serialLine, "PATTERN ", 8) == 0) {
      int patternIndex = atoi(serialLine + 8);
      logf("PATTERN %i requested", patternIndex);
      randomRunner->patternTimeout = 0;
      randomRunner->runPatternAtIndex(patternIndex);
    } else if (strcmp(serialLine, "AUTO") == 0) {
      logf("AUTO requested");
      randomRunner->patternTimeout = kPatternRunDuration;
#if HARDWARE_VERSION >= 2
    } else if (strncmp(serialLine, "FAKEFFT ", 8) == 0) {
      fftProcessing.benchTestLevel = atoi(serialLine + 8);
      logf("FAKEFFT %i requested", fftProcessing.benchTestLevel);
#endif
    } else if (strncmp(serialLine, "FPS ", 4) == 0) {
      benchClampFPS = atoi(serialLine + 4);
      logf("FPS clamp %i requested", benchClampFPS);
    } else if (strcmp(serialLine, "PERF") == 0) {
      benchPerfLogging = !benchPerfLogging;
      benchPerfPatternUS = benchPerfShowUS = benchPerfFrames = 0;
      logf("PERF logging %s", benchPerfLogging ? "on" : "off");
#if defined(ARDUINO_ARCH_RP2040)
    } else if (strcmp(serialLine, "REBOOT") == 0) {
      logf("REBOOT requested");
      Serial.flush();
      delay(50);
      rp2040.reboot();
#endif
    }
  }

  if (benchPerfLogging && ++benchPerfFrames >= 200) {
    logf("PERF avg over %u frames: patterns %uus, show %uus", benchPerfFrames, benchPerfPatternUS / benchPerfFrames, benchPerfShowUS / benchPerfFrames);
    benchPerfPatternUS = benchPerfShowUS = benchPerfFrames = 0;
  }
}

#endif
