#ifndef PATTERN_H
#define PATTERN_H

#include <FastLED.h>
#include <vector>
#include <functional>

#include <paletting.h>
#include <patterning.h>

#include <util.h>
#include <drawing.h>
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

#endif
