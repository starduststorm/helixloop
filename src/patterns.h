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
  static constexpr int baselineFPS = 141; // pre-perf baseline
  BaselineStepper turnStepper;
  int turnRolls = 0;
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

      for (int k = turnRolls; k > 0; --k) {
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
      }
    };
  }

  void update() {
    turnRolls = turnStepper.steps(baselineFPS);
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
  // per-frame random chance below was tuned at this observed framerate (mean; it swung 58-142 with particle count)
  static constexpr int baselineFPS = 100;
  BaselineStepper switchStepper;
  int switchRolls = 0;
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
      for (int k = switchRolls; k > 0; --k) {
        if (random16() < 512) {
          bool choice = random8(2);
          p.directions.edgeTypes.second = EdgeType::counterclockwise | ((choice == 0) ? EdgeType::helix1 : EdgeType::helix2);
          p.directions.edgeTypes.third = EdgeType::counterclockwise | ((choice == 0) ? EdgeType::helix2 : EdgeType::helix1);
        }
      }
    };
  }

  unsigned long lastSpawn = 0;
  uint8_t spiralIndex = 0; //0-2

  void update() {
    switchRolls = switchStepper.steps(baselineFPS);
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
  static constexpr int baselineFPS = 141; // pre-perf baseline
  BaselineStepper accelStepper;
  int accelSteps = 0;
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
        p.speed = min(defaultSpeed, p.speed + accelSteps);
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
    accelSteps = accelStepper.steps(baselineFPS);
    particles.fadeDown = beatsin16(3, 3 << 8, 6 << 8);
    particles.update();
  }
  const char *description() {
    return "SpasticTriad";
  }
};

#if HARDWARE_VERSION >= 2

class TouchParticleStream : public Pattern, PaletteRotation<CRGBPalette256> {
  static constexpr unsigned spawnInterval = 120; // ms between particles per held TouchPoint
  static constexpr unsigned maxParticles = 150;
  static constexpr unsigned lingerDuration = 1500; // ms to keep running after the last particle so trails can fade out
  static constexpr int spiralSpeed = 60;
  unsigned long lastSpawn[TOUCH_POINT_COUNT] = {0};
  unsigned long lastActive;
public:
  Particles particles;
  TouchParticleStream() : particles(ledgraph, ctx, 0, spiralSpeed, 4000, {EdgeType::outbound}) {
    minBrightness = 20;
    particles.requireExactEdgeTypeMatch = true;
    particles.fadeDown = 5<<8;
    particles.setFadeUpDistance(2);
    particles.flowRule = Particles::priority;
    particles.spawnRule = Particles::manualSpawn;
    lastActive = millis();

    particles.handleUpdateParticle = [this](Particle &p, uint8_t index) {
      p.color = getShiftingPaletteColor(p.colorIndex, 3);
      if (p.age() > p.lifespan - (p.lifespan >> 2)) {
        p.brightness = 0xFF * (p.lifespan - p.age()) / (p.lifespan >> 2);
      }

      if (indexInSpiral(p.px) != -1) {
        return;
      }
      if (p.directions.edgeTypes.first == EdgeType::outbound) {
        // just left the spiral; keep heading whichever way around the wreath the spiral exit took us
        EdgeTypes heading = EdgeType::counterclockwise;
        for (Edge &edge : ledgraph.adjList[p.lastPx]) {
          if (edge.to == p.px && (edge.types & EdgeType::clockwise)) {
            heading = EdgeType::clockwise;
          }
        }
        // hop helixes at random at each intersection
        p.directions = MakeEdgeTypesQuad(heading);
      }
    };
  }

  void update() {
    unsigned long mils = millis();
    for (int i = 0; i < TOUCH_POINT_COUNT; ++i) {
      if (touchHeld[i] && mils - lastSpawn[i] > spawnInterval && particles.particles.size() < maxParticles) {
        Particle &p = particles.addParticle();
        p.px = HLSpiralCenters[i];
        bool ccwFirst = random8(2);
        p.directions = MakeEdgeTypesQuad(EdgeType::outbound,
                                         ccwFirst ? EdgeType::counterclockwise : EdgeType::clockwise,
                                         ccwFirst ? EdgeType::clockwise : EdgeType::counterclockwise);
        p.colorIndex = i * 0xFF / TOUCH_POINT_COUNT + random8(40);
        p.color = getShiftingPaletteColor(p.colorIndex, 3);
        p.lifespan += random16(1500);
        lastSpawn[i] = mils;
      }
    }
    if (!particles.particles.empty()) {
      lastActive = mils;
    }
    particles.update();
  }

  // nothing left to draw
  bool isIdle() {
    return millis() - lastActive > lingerDuration;
  }

  const char *description() {
    return "TouchParticleStream";
  }
};

#endif // HARDWARE_VERSION >= 2

class SoundBits : public SoundPattern, public PaletteRotation<CRGBPalette256> {
public:
  ParticleSim<LED_COUNT> particles;

  int bitLoudZoom = 70;

  static constexpr int baselineFPS = 60; // pre-perf baseline
  BaselineStepper frameStepper;
  int frameSteps = 0;

  // ambient amplitude level required for the pattern to be selected; tune to mic/environment
  static constexpr int ambientLevelThreshold = 1200;

  // registerPattern runCondition: only run when ambient sound is above threshold
  static uint8_t wantsToRun(PatternRunner &runner) {
    return (ambientSound && ambientSound->ambientLevel() > ambientLevelThreshold) ? 0xFF : 0;
  }

  SoundBits() : SoundPattern(fftProcessing), particles(ledgraph, ctx, 0, 0, 1200, {clockwise, counterclockwise}) {
    particles.preventReverseFlow = true;
    minBrightness = 20;
    particles.setFadeUpDistance(1);
    particles.handleUpdateParticle = [this](Particle &bit, uint8_t index) {
      if (bit.age() > bit.lifespan/2) {
        bit.brightness = min(0xFF, max(0, (int)(0xFF - 0xAF * (bit.age()-bit.lifespan/2) / (bit.lifespan-bit.lifespan/2))));
      }
      for (int k = frameSteps; k > 0; --k) {
        if (bit.speed > bitLoudZoom - bitLoudZoom * bit.age() / bit.lifespan) {
          bit.speed -= min<uint16_t>(2, bit.speed);
        }
      }
    };
  }
  
  void update() {
    unsigned long mils = millis();
    
    FFTFrame frame = spectrumFrame();
    frameSteps = frameStepper.steps(baselineFPS);
    for (unsigned b = 0; frameSteps > 0 && b < frame.size; ++b) {
      int32_t level = frame.spectrum[b] - fftLevelThreshold;
      
      if (level > fftLevelThreshold && particles.particles.size() < 255) {
        unsigned maxlifespan = 300;
        Particle &p = particles.addParticle();
        p.directions = ::all;
        p.px = random16(LED_COUNT);
        p.lifespan = max(1, min(maxlifespan, maxlifespan * level/30));
        uint8_t phase = b*15+millis()/100;
        uint8_t brightness = min(0xFF, level*10);
        p.color = getPaletteColor(phase, brightness);
        p.speed = min(bitLoudZoom, 3*level);
      }
    }
    autoGainUpdate();
    particles.update();
  }

  const char *description() {
    return "SoundBits";
  }
};

#endif
