#ifndef PATTERN_H
#define PATTERN_H

#include <FastLED.h>
#include <vector>
#include <functional>

#include <paletting.h>
#include <patterning.h>

#include <util.h>
#include <drawing.h>
#include <audio.h>
#include <particles.h>
using Particles = ParticleSim<LED_COUNT>;

#include "ledgraph.h"

class SwarmPattern : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  Particles particles;
public:
  SwarmPattern() : particles(ledgraph, ctx, 16, 42, 8192, {}) {
    particles.requireExactEdgeTypeMatch = true;
    particles.fadeDown = 2<<8;
    particles.flowRule = Particles::priority;
    particles.spawnRule = Particles::maintainPopulation;
    particles.maxSpawnPerSecond = 2;

    particles.handleNewParticle = [this](Particle &p) {
      p.directions = MakeEdgeTypesQuad(EdgeType::helix2|EdgeType::counterclockwise, EdgeType::outbound);
      p.colorIndex = random8();
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      p.px = HLSpiralCenters[random8(ARRAY_SIZE(HLSpiralCenters))];
    };

    particles.handleUpdateParticle = [this](Particle &p, uint8_t index) {
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      if (p.age() > p.lifespan / 2) {
        p.directions.edgeTypes.first = EdgeType::helix1 | EdgeType::clockwise;
        p.directions.edgeTypes.second = EdgeType::clockwise;
      }
      if (p.age() > p.lifespan - (p.lifespan / 8)) {
        p.brightness = 0xFF * (p.lifespan - p.age()) / (p.lifespan / 8);
      }
    };
  }

  void update() {
    particles.update();
  }

  const char *description() {
    return "SwarmPattern";
  }
};

/* ------------------------------------------------------------------------------- */

class WanderingFew : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  Particles particles;
public:
  WanderingFew() : particles(ledgraph, ctx, 9, 42, 9000, {}) {
    particles.requireExactEdgeTypeMatch = true;
    particles.fadeDown = 3<<8;
    particles.setFadeUpDistance(3);
    particles.flowRule = Particles::priority;
    particles.spawnRule = Particles::maintainPopulation;
    particles.maxSpawnPerSecond = 1;

    particles.handleNewParticle = [this](Particle &p) {
      p.directions = MakeEdgeTypesQuad(EdgeType::outbound, EdgeType::helix2|EdgeType::counterclockwise, 0);
      p.colorIndex = random8();
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      p.px = HLSpiralCenters[random8(ARRAY_SIZE(HLSpiralCenters))];
    };

    particles.handleUpdateParticle = [this](Particle &p, uint8_t index) {
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      if (p.age() > p.lifespan - (p.lifespan >> 3)) {
        p.brightness = 0xFF * (p.lifespan - p.age()) / (p.lifespan >> 3);
      }

      // turn every 10 seconds or so?
      if (random16() < 64) {
        if (ledgraph.adjacencies(p.px, MakeEdgeTypesQuad(p.directions.edgeTypes.first, p.directions.edgeTypes.second)).size() > 0) { // only turn if on primary path
          p.directions.edgeTypes.third = p.directions.edgeTypes.second;
          p.directions.edgeTypes.second = (random8(2) == 0) ? EdgeType::clockwise : EdgeType::counterclockwise;
          p.directions.edgeTypes.second |= (random8(2) == 0) ? EdgeType::helix1 : EdgeType::helix2;
        }
      }
      if (random16() < 16) {
        if (p.directions.edgeTypes.first & EdgeType::inbound) {
          p.directions.edgeTypes.first = EdgeType::outbound;
        } else {
          p.directions.edgeTypes.first = EdgeType::inbound;
        }
      }
    };
  }

  void update() {
    particles.update();
  }

  const char *description() {
    return "WanderingFew";
  }
};

/* ------------------------------------------------------------------------------- */

class SpiralSource : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  Particles particles;
public:
  SpiralSource() : particles(ledgraph, ctx, 0, 42, 2400, {}) {
    particles.requireExactEdgeTypeMatch = true;
    particles.setFadeUpDistance(2);
    particles.flowRule = Particles::priority;
    particles.maxSpawnPerSecond = 1;

    particles.handleUpdateParticle = [this](Particle &p, uint8_t index) {
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      if (p.age() > p.lifespan - (p.lifespan / 2)) {
        p.brightness = 0xFF * (p.lifespan - p.age()) / (p.lifespan / 2);
        p.brightness = dim8_raw(p.brightness);
      } else if (p.age() < 100) {
        p.brightness = max(20u, 0xFF * p.age() / 100);
        p.brightness = dim8_raw(p.brightness);
      }

      // switch loops sometimes
      if (random16() < 512) {
          bool choice = random8(2);
          p.directions.edgeTypes.second = EdgeType::counterclockwise | ((choice == 0) ? EdgeType::helix1 : EdgeType::helix2);
          p.directions.edgeTypes.third = EdgeType::counterclockwise | ((choice == 0) ? EdgeType::helix2 : EdgeType::helix1);
      }
    };
  }

  unsigned long lastSpawn = 0;
  uint8_t spiralIndex = 0; //0-2

  void update() {
    particles.update();
    
    particles.fadeDown = beatsin16(1, 3<<8, 8<<8);

    if (millis() - lastSpawn > beatsin88((uint16_t)(1.8*256), 40, 120)) {
      Particle &p = particles.addParticle();
      p.directions = MakeEdgeTypesQuad(EdgeType::outbound, EdgeType::helix2|EdgeType::counterclockwise, 0);
      p.colorIndex = 0xFF*millis()/1000/6;
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      p.px = HLSpiralCenters[spiralIndex];
      p.lifespan = beatsin88((uint16_t)(3.14*256), 1000, 3000) + random8();
      spiralIndex = (spiralIndex + 1) % 3;
      p.speed = beatsin88((uint16_t)(2.2*256), 36, 48);

      lastSpawn = millis();
    }
  }

  const char *description() {
    return "SpiralSource";
  }
};

class SpasticTriad : public Pattern, PaletteRotation<CRGBPalette256> {
public:
  int defaultSpeed = 333;
  Particles particles;
  unsigned long nextEvent[3] = {0};
  uint8_t nextColorIndex = 0;
  SpasticTriad() : particles(ledgraph, ctx, 3, 0, 0, {}) {
    minBrightness = 20;
    maxColorJump = 15;
    particles.setFadeUpDistance(3);
    particles.requireExactEdgeTypeMatch = false;
    particles.preventReverseFlow = true;
    particles.flowRule = Particles::priority;

    particles.handleNewParticle = [this](Particle &p) {
      int num = particles.particles.size()-1;
      p.speed = defaultSpeed;
      p.px = HLSpiralCenters[num];
      p.directions = MakeEdgeTypesQuad(EdgeType::outbound, EdgeType::counterclockwise);
      p.colorIndex = nextColorIndex;
      nextColorIndex = num * 0xFF / 3;
    };

    particles.handleKillParticle = [this](Particle &p) {
      // happens rarely, if particles end up with no where to go. reuse the color index if so.
      nextColorIndex = p.colorIndex;
      assert(false, "particle died: px:, directions quad: %i, %i, %i, %i, speed = %i", p.px,
             p.directions.edgeTypes.first,
             p.directions.edgeTypes.second,
             p.directions.edgeTypes.third,
             p.directions.edgeTypes.fourth, 
             p.speed);
    };

    particles.handleUpdateParticle = [this](Particle &p, uint8_t index) {
      p.color = getShiftingPaletteColor(p.colorIndex, 30);

      for (int i = 0; i < ARRAY_SIZE(HLSpiralCenters); ++i) {
        if (p.px == HLSpiralCenters[i] && (p.lastPx != HLSpiralCenters[0] && p.lastPx != HLSpiralCenters[1] && p.lastPx != HLSpiralCenters[2])) {
        p.moveTo(HLSpiralCenters[random8(ARRAY_SIZE(HLSpiralCenters))]);
          // we actually want first priority to be counterclockwise, but if we do that then an event might leave the particle with no path. 
          // this will sometimes take an extra pixel when moving out of the spiral but that's fine especially at high speed.
          p.directions.edgeTypes.first = EdgeType::outbound;
          p.directions.edgeTypes.second = EdgeType::counterclockwise;
          break;
        }
      }

      int spiralPos = indexInSpiral(p.px);
      if (spiralPos != -1) {
        p.speed = max(3, defaultSpeed * (spiralPos+1)/SPIRAL_LED_COUNT);
      } else if (p.speed < defaultSpeed) {
        p.speed += 1;
      }

      if (millis() > nextEvent[index]) {
        int event = random8(4);
        switch (event) {
          case 0:
            p.directions.edgeTypes.first = (p.directions.edgeTypes.first & EdgeType::outbound
                                            ? EdgeType::inbound
                                            : EdgeType::outbound);
            break;
          case 1:
            p.directions.edgeTypes.second = (p.directions.edgeTypes.second & EdgeType::clockwise
                                             ? EdgeType::counterclockwise
                                             : EdgeType::clockwise);
            break;
          case 2:
            p.directions.edgeTypes.second |= (p.directions.edgeTypes.second & EdgeType::helix1
                                             ? EdgeType::helix2
                                             : EdgeType::helix1);
            p.directions.edgeTypes.third = (p.directions.edgeTypes.second & EdgeType::helix1
                                             ? EdgeType::helix2
                                             : EdgeType::helix1);
            break;
          case 3:
            p.directions.edgeTypes.first = EdgeType::inbound;
            p.directions.edgeTypes.second = EdgeType::clockwise;
            p.directions.edgeTypes.third = EdgeType::helix1;
            break;
          case 4:
            p.directions.edgeTypes.first = EdgeType::all;
            break;
        }
        p.lastPx = p.px; // override preventReverseFlow
        nextEvent[index] = millis() + random16(500, 5000);
      }
    };
  }
  void update() {
    defaultSpeed = beatsin16(1, 133, 333);
    particles.fadeDown = beatsin16(3, 3 << 8, 6 << 8);
    particles.update();
  }
  const char *description() {
    return "SpasticTriad";
  }
};

class SoundPattern : public Pattern, public FFTReceiver {
public:
  unsigned long lastLevelThreshChange{0};
  int minFFTLevelThreshold{3};
  int fftLevelThreshold{minFFTLevelThreshold};
  int autoGainAdjustmentInterval{600};

  SoundPattern() : FFTReceiver(fftProcessing) {
    // stop main loop from lowering framerate when we have nothing to draw, since that results in visibly-delayed response to sounds
    fc.takeFPSAssertion(); 
  }
  ~SoundPattern() {
    fc.releaseFPSAssertion();
  }
  void autoGainUpdate() {
    FFTFrame frame = fftProcessing.getDataFrame();
    unsigned long mils = millis();

    int maxFrameValue = 0;
    int32_t sumFrameValue = 0;
    for (int i = 0 ; i < frame.size; ++i) {
      if (frame.smoothSpectrum[i] > maxFrameValue) {
        maxFrameValue = frame.smoothSpectrum[i];
      }
      sumFrameValue += frame.smoothSpectrum[i];
    }
    int avgFrameValue = sumFrameValue/frame.size;

    int litCount{0};
    for (int i = 0 ; i < LED_COUNT; ++i) {
      litCount += ctx.leds[i] ? 1 : 0;
    }
    /* latch-ditch auto gain:
     * slowly adjust threshold for drawing if to approach the average levels
     * quickly move threshold for drawing if we're over- or under-drawing
     * temporarily adjust thresholds at a fast interval at the start of pattern running to find a baseline
    */
   bool overDrawing = litCount > 95*LED_COUNT/100;
   bool underDrawing = litCount < 2*LED_COUNT/10;
   int adjustmentInterval = (runTime() > 3000 ? autoGainAdjustmentInterval : autoGainAdjustmentInterval/6);
   if ((overDrawing || fftLevelThreshold < avgFrameValue) && mils - lastLevelThreshChange > adjustmentInterval) {
      fftLevelThreshold++;
      if (overDrawing) {
        fftLevelThreshold += max(0, (maxFrameValue - fftLevelThreshold) / 20);
      }
      // logf("SoundPattern litCount = %i, frame value avg=%i,max=%i, fftLevelThreshold up to %i", litCount, avgFrameValue, maxFrameValue, fftLevelThreshold);
      lastLevelThreshChange = mils;
    } else if (fftLevelThreshold > minFFTLevelThreshold && (underDrawing || fftLevelThreshold > avgFrameValue) && mils - lastLevelThreshChange > adjustmentInterval) {
      fftLevelThreshold--;
      if (underDrawing) {
        fftLevelThreshold = max(minFFTLevelThreshold, fftLevelThreshold + min(0, (maxFrameValue - fftLevelThreshold) / 10));
      }
      // logf("SoundPattern litCount = %i, frame value avg=%i,max=%i, fftLevelThreshold down to %i", litCount, avgFrameValue, maxFrameValue, fftLevelThreshold);
      lastLevelThreshChange = mils;
    }
  }
};

class SoundBits : public SoundPattern, public PaletteRotation<CRGBPalette256> {
public:
  ParticleSim<LED_COUNT> particles;

  int bitLoudZoom = 70;

  // avg spectrum level required for the pattern to be selected; tune to mic/environment
  static constexpr int ambientLevelThreshold = 4;

  // registerPattern runCondition: only run when ambient sound is above threshold
  static uint8_t wantsToRun(PatternRunner &runner) {
    if (!audioInput.isStreaming()) {
      return 0;
    }
    FFTFrame frame = fftProcessing.getDataFrame();
    if (frame.size == 0 || !frame.spectrum) {
      return 0;
    }
    int32_t sum = 0;
    for (unsigned int i = 0; i < frame.size; ++i) {
      sum += frame.spectrum[i];
    }
    return (sum / (int32_t)frame.size > ambientLevelThreshold) ? 0xFF : 0;
  }


  
  SoundBits() : particles(ledgraph, ctx, 0, 0, 1200, {clockwise, counterclockwise}) {
    particles.preventReverseFlow = true;
    minBrightness = 20;
    particles.setFadeUpDistance(1);
    particles.handleUpdateParticle = [this](Particle &bit, uint8_t index) {
      if (bit.age() > bit.lifespan/2) {
        bit.brightness = min(0xFF, max(0, (int)(0xFF - 0xAF * (bit.age()-bit.lifespan/2) / (bit.lifespan-bit.lifespan/2))));
      }
      if (bit.speed > bitLoudZoom - bitLoudZoom * bit.age() / bit.lifespan) {
        bit.speed-=2;
      }
    };
  }
  
  void update() {
    unsigned long mils = millis();
    
    FFTFrame frame = spectrumFrame();
    for (unsigned freqBucket = 0; freqBucket < frame.size; ++freqBucket) {
      int32_t level = frame.spectrum[freqBucket] - fftLevelThreshold;
      
      if (level > fftLevelThreshold && particles.particles.size() < 255) {
        unsigned maxlifespan = 300;
        Particle &p = particles.addParticle();
        p.directions = ::all;
        p.px = random16(LED_COUNT);
        p.lifespan = max(1, min(maxlifespan, maxlifespan * level/30));
        uint8_t phase = freqBucket*15+millis()/100;
        uint8_t brightness = min(0xFF, level*10);
        p.color = getPaletteColor(phase, brightness);
        p.speed = min(bitLoudZoom, 3*level);
      }
      autoGainUpdate();
    }
    particles.update();
  }

  const char *description() {
    return "SoundBits";
  }
};

#endif
