#ifndef PATTERN_H
#define PATTERN_H

#include <FastLED.h>
#include <vector>
#include <functional>

#include "util.h"
#include "palettes.h"
#include "ledgraph.h"
#include "drawing.h"

typedef PaletteRotation<CRGBPalette32> ColorManager;

class Pattern {
private:  
  long startTime = -1;
  long stopTime = -1;
  long lastUpdateTime = -1;
public:
  ColorManager *colorManager;
  DrawingContext ctx;
  virtual ~Pattern() { }

  void start() {
    logf("Starting %s", description());
    startTime = millis();
    stopTime = -1;
    setup();
  }

  void loop() {
    update();
    lastUpdateTime = millis();
  }

  virtual bool wantsToIdleStop() {
    return true;
  }

  virtual bool wantsToRun() {
    // for idle patterns that require microphone input and may opt not to run if there is no sound
    return true;
  }

  virtual void setup() { }

  void stop() {
    logf("Stopping %s", description());
    startTime = -1;
  }

  virtual void update() { }
  
  virtual const char *description() = 0;

public:
  bool isRunning() {
    return startTime != -1;
  }

  unsigned long runTime() {
    return startTime == -1 ? 0 : millis() - startTime;
  }

  unsigned long frameTime() {
    return (lastUpdateTime == -1 ? 0 : millis() - lastUpdateTime);
  }
};

/* ------------------------------------------------------------------------------------------------------ */

// a lil patternlet that can be instantiated to run bits
class BitsFiller {
public:
  typedef enum : uint8_t { random, priority, split } FlowRule;
  typedef enum : uint8_t { maintainPopulation, manualSpawn } SpawnRule;
 
  struct Bit {
    friend BitsFiller;
  private:
    unsigned long birthmilli;
    bool firstFrame = true;
  public:
    uint8_t colorIndex; // storage only
    
    PixelIndex px;
    EdgeTypesQuad directions;
    
    unsigned long lifespan;

    CRGB color;
    uint8_t brightness = 0xFF;

    Bit(int px, EdgeTypesQuad directions, unsigned long lifespan) 
      : px(px), directions(directions), lifespan(lifespan) {
      reset();
    }

    void reset() {
      birthmilli = millis();
      color = CHSV(random8(), 0xFF, 0xFF);
    }

    unsigned long age() {
      return min(millis() - birthmilli, lifespan);
    }
protected:
    unsigned long exactAge() {
      return millis() - birthmilli;
    }
  };

private:

  DrawingContext &ctx;

  unsigned long lastTick = 0;
  unsigned long lastMove = 0;
  unsigned long lastBitSpawn = 0;

  uint8_t spawnLocation() {
    if (spawnPixels) {
      return spawnPixels->at(random8()%spawnPixels->size());
    }
    return random16()%LED_COUNT;
  }

  Bit &makeBit(Bit *fromBit=NULL) {
    // the bit directions at the BitsFiller level may contain multiple options, choose one at random for this bit
    EdgeTypesQuad directionsForBit = {0};

    for (int n = 0; n < 4; ++n) {
      uint8_t bit[EdgeTypesCount] = {0};
      uint8_t bitcount = 0;
      for (int i = 0; i < EdgeTypesCount; ++i) {
        uint8_t byte = bitDirections.quad >> (n * EdgeTypesCount);
        if (byte & 1 << i) {
          bit[bitcount++] = i;
        }
      }
      if (bitcount) {
        directionsForBit.quad |= 1 << (bit[random8()%bitcount] + n * EdgeTypesCount);
      }
    }

    if (fromBit) {
      bits.emplace_back(*fromBit);
    } else {
      bits.emplace_back(spawnLocation(), directionsForBit, lifespan);
    }
    return bits.back();
  }

  void killBit(uint8_t bitIndex) {
    handleKillBit(bits[bitIndex]);
    bits.erase(bits.begin() + bitIndex);
  }

  void splitBit(Bit &bit, PixelIndex toIndex) {
    Bit &split = makeBit(&bit);
    split.px = toIndex;
  }

  bool isIndexAllowed(PixelIndex index) {
    if (allowedPixels) {
      return allowedPixels->end() != allowedPixels->find(index);
    }
    return true;
  }

  vector<Edge> edgeCandidates(PixelIndex index, EdgeTypesQuad bitDirections) {
    vector<Edge> nextEdges;
    switch (flowRule) {
      case priority: {
        auto adj = ledgraph.adjacencies(index, bitDirections);
        for (auto edge : adj) {
          // logf("edgeCandidates: index %i has %i adjacencies matching directions %i (%i,%i)", 
              // index, adj.size(), (int)bitDirections.pair, bitDirections.edgeTypes.first, bitDirections.edgeTypes.second);
          // loglf("checking if adj index %i is allowed for edge from %i types %i...", (int)edge.to, (int)edge.from, (int)edge.types);
          if (isIndexAllowed(edge.to)) {
            nextEdges.push_back(edge);
            break;
          }
        }
        break;
      }
      case random:
      case split: {
        auto adj = ledgraph.adjacencies(index, bitDirections);
        for (auto a : adj) {
          if (isIndexAllowed(a.to)) {
            nextEdges.push_back(a);
          }
        }
        if (flowRule == split) {
          if (nextEdges.size() == 1) {
            // flow normally if we're not actually splitting
            nextEdges.push_back(nextEdges.front());
          } else {
            // split along all allowed split directions, or none if none are allowed
            for (Edge nextEdge : nextEdges) {
              if (splitDirections & nextEdge.types) {
                nextEdges.push_back(nextEdge);
              }
            }
          }
        } else if (nextEdges.size() > 0) {
          // FIXME: EdgeType::random behavior doesn't work right with the way fadeUp is implemented
          nextEdges.push_back(nextEdges.at(random8()%nextEdges.size()));
        }
        break;
      }
    }
    // TODO: does not handle duplicates in the case of the same vertex being reachable via multiple edges
    assert(nextEdges.size() <= 4, "no pixel in this design has more than 4 adjacencies but index %i had %u", index, nextEdges.size());
    return nextEdges;
  }

  bool flowBit(uint8_t bitIndex) {
    vector<Edge> nextEdges = edgeCandidates(bits[bitIndex].px, bits[bitIndex].directions);
    if (nextEdges.size() == 0) {
      // leaf behavior
      logf("no path for bit");
      killBit(bitIndex);
      return false;
    } else {
      bits[bitIndex].px = nextEdges.front().to;
      for (unsigned i = 1; i < nextEdges.size(); ++i) {
        splitBit(bits[bitIndex], nextEdges[i].to);
      }
    }
    return true;
  }

public:
  void dumpBits() {
    logf("--------");
    logf("There are %i bits", bits.size());
    for (unsigned b = 0; b < bits.size(); ++b) {
      Bit &bit = bits[b];
      logf("Bit %i: px=%i, birthmilli=%lu, colorIndex=%u", b, bit.px, bit.birthmilli, bit.colorIndex);
      Serial.print("  Directions: 0b");
      for (int i = 2*EdgeTypesCount - 1; i >= 0; --i) {
        Serial.print(bit.directions.quad & (1 << i));
      }
      Serial.println();
    }
    logf("--------");
  }

  vector<Bit> bits;
  uint8_t maxSpawnBits;
  uint8_t maxBitsPerSecond = 0; // limit how fast new bits are spawned, 0 = no limit
  uint8_t speed; // in pixels/second
  EdgeTypesQuad bitDirections;

  unsigned long lifespan = 0; // in milliseconds, forever if 0

  FlowRule flowRule = random;
  SpawnRule spawnRule = maintainPopulation;
  uint8_t fadeUpDistance = 0; // fade up n pixels ahead of bit motion
  EdgeTypes splitDirections = EdgeType::all; // if flowRule is split, which directions are allowed to split
  
  const vector<PixelIndex> *spawnPixels = NULL; // list of pixels to automatically spawn bits on
  const set<PixelIndex> *allowedPixels = NULL; // set of pixels that bits are allowed to travel to

  function<void(Bit &)> handleNewBit = [](Bit &bit){};
  function<void(Bit &)> handleUpdateBit = [](Bit &bit){};
  function<void(Bit &)> handleKillBit = [](Bit &bit){};

  BitsFiller(DrawingContext &ctx, uint8_t maxSpawnBits, uint8_t speed, unsigned long lifespan, vector<EdgeTypes> bitDirections)
    : ctx(ctx), maxSpawnBits(maxSpawnBits), speed(speed), lifespan(lifespan) {
      this->bitDirections = MakeEdgeTypesQuad(bitDirections);
    bits.reserve(maxSpawnBits);
  };

  void fadeUpForBit(Bit &bit, PixelIndex px, int distanceRemaining, unsigned long lastMove) {
    vector<Edge> nextEdges = edgeCandidates(px, bit.directions);

    unsigned long mils = millis();
    unsigned long fadeUpDuration = 1000 * fadeUpDistance / speed;
    for (Edge edge : nextEdges) {
      unsigned long fadeTimeSoFar = mils - lastMove + distanceRemaining * 1000/speed;
      uint8_t progress = 0xFF * fadeTimeSoFar / fadeUpDuration;

      CRGB existing = ctx.leds[edge.to];
      CRGB blended = blend(existing, bit.color, dim8_raw(progress));
      blended.nscale8(bit.brightness);
      ctx.leds[edge.to] = blended;
      
      if (distanceRemaining > 0) {
        fadeUpForBit(bit, edge.to, distanceRemaining-1, lastMove);
      }
    }
  }

  int fadeDown = 4; // fadeToBlackBy units per millisecond
  void update() {
    unsigned long mils = millis();

    ctx.leds.fadeToBlackBy(fadeDown * (mils - lastTick));
    
    if (spawnRule == maintainPopulation) {
      for (unsigned b = bits.size(); b < maxSpawnBits; ++b) {
        if (maxBitsPerSecond != 0 && mils - lastBitSpawn < 1000 / maxBitsPerSecond) {
          continue;
        }
        addBit();
        lastBitSpawn = mils;
      }
    }

    if (mils - lastMove > 1000/speed) {
      for (int i = bits.size() - 1; i >= 0; --i) {
        if (bits[i].firstFrame) {
          // don't flow bits on the first frame. this allows pattern code to make their own bits that are displayed before being flowed
          continue;
        }
        bool bitAlive = flowBit(i);
        if (bitAlive && bits[i].lifespan != 0 && bits[i].exactAge() > bits[i].lifespan) {
          killBit(i);
        }
      }
      if (mils - lastMove > 2000/speed) {
        lastMove = mils;
      } else {
        // This helps avoid time drift, which for some reason can make one device run consistently faster than another
        lastMove += 1000/speed;
      }
    }
    for (Bit &bit : bits) {
      handleUpdateBit(bit);
    }

    for (Bit &bit : bits) {
      CRGB color = bit.color;
      color.nscale8(bit.brightness);
      ctx.leds[bit.px] = color;
    }
    
    if (fadeUpDistance > 0) {
      for (Bit &bit : bits) {
        if (bit.firstFrame) continue;
        // don't show full fade-up distance right when bit is created
        int bitFadeUpDistance = min((unsigned long)fadeUpDistance, speed * bit.age() / 1000);
        if (bitFadeUpDistance > 0) {
          // TODO: can fade-up take into account color advancement?
          fadeUpForBit(bit, bit.px, bitFadeUpDistance - 1, lastMove);
        }
      }
    }

    lastTick = mils;

    for (Bit &bit : bits) {
      bit.firstFrame = false;
    }
  };

  Bit &addBit() {
    Bit &newbit = makeBit();
    handleNewBit(newbit);
    return newbit;
  }

  void removeAllBits() {
    bits.clear();
  }

  void resetBitColors(ColorManager *colorManager) {
    for (Bit &bit : bits) {
      bit.color = colorManager->getPaletteColor(bit.colorIndex, bit.color.getAverageLight());
    }
  }
};

/* ------------------------------------------------------------------------------- */

class SwarmPattern : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  BitsFiller *bitsFiller;
public:
  SwarmPattern() {
    vector<EdgeTypes> directions = {EdgeType::none};
    bitsFiller = new BitsFiller(ctx, 16, 42, 8192, directions);
    bitsFiller->fadeDown = 2;
    bitsFiller->flowRule = BitsFiller::priority;
    bitsFiller->spawnRule = BitsFiller::maintainPopulation;
    bitsFiller->maxBitsPerSecond = 2;

    bitsFiller->handleKillBit = [](BitsFiller::Bit &bit) {
    };

    bitsFiller->handleNewBit = [this](BitsFiller::Bit &bit) {
      bit.directions = MakeEdgeTypesQuad(Edge::loop2|Edge::counterclockwise, Edge::outbound);
      bit.colorIndex = random8();
      bit.color = getPaletteColor(bit.colorIndex);
      bit.px = HLSpiralCenters[random8(ARRAY_SIZE(HLSpiralCenters))];
    };

    bitsFiller->handleUpdateBit = [this](BitsFiller::Bit &bit) {
      bit.color = this->colorManager->getPaletteColor(bit.colorIndex);
      if (bit.age() > bit.lifespan >> 1) {
        bit.directions.edgeTypes.first = Edge::loop1 | Edge::clockwise;
        bit.directions.edgeTypes.second = Edge::clockwise;
      }
      if (bit.age() > bit.lifespan - (bit.lifespan >> 3)) {
        bit.brightness = 0xFF * (bit.lifespan - bit.age()) / (bit.lifespan >> 3);
      }
    };
  }

  ~SwarmPattern() {
    delete bitsFiller;
  }

  void update() {
    bitsFiller->update();

    for (int i = 0; i < colorManager->trackedColorsCount(); ++i) {
      bitsFiller->bits[i].color = colorManager->getTrackedColor(i);
    }
    colorManager->paletteRotationTick();
  }

  const char *description() {
    return "SwarmPattern";
  }
};



class WanderingFew : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  BitsFiller *bitsFiller;
public:
  WanderingFew() {
    vector<EdgeTypes> directions = {EdgeType::none};
    bitsFiller = new BitsFiller(ctx, 9, 42, 9000, directions);
    bitsFiller->fadeDown = 3;
    bitsFiller->fadeUpDistance = 3;
    bitsFiller->flowRule = BitsFiller::priority;
    bitsFiller->spawnRule = BitsFiller::maintainPopulation;
    bitsFiller->maxBitsPerSecond = 1;

    bitsFiller->handleKillBit = [](BitsFiller::Bit &bit) {
    };

    bitsFiller->handleNewBit = [this](BitsFiller::Bit &bit) {
      bit.directions = MakeEdgeTypesQuad(Edge::outbound, Edge::loop2|Edge::counterclockwise, 0);
      bit.colorIndex = random8();
      bit.color = getPaletteColor(bit.colorIndex);
      bit.px = HLSpiralCenters[random8(ARRAY_SIZE(HLSpiralCenters))];
    };

    bitsFiller->handleUpdateBit = [this](BitsFiller::Bit &bit) {
      bit.color = this->colorManager->getPaletteColor(bit.colorIndex);
      if (bit.age() > bit.lifespan - (bit.lifespan >> 3)) {
        bit.brightness = 0xFF * (bit.lifespan - bit.age()) / (bit.lifespan >> 3);
      }

      // turn every 10 seconds or so?
      if (random16() < 64) {
        if (ledgraph.adjacencies(bit.px, MakeEdgeTypesQuad(bit.directions.edgeTypes.first, bit.directions.edgeTypes.second)).size() > 0) { // only turn if on primary path
          bit.directions.edgeTypes.third = bit.directions.edgeTypes.second;
          bit.directions.edgeTypes.second = (random8(2) == 0) ? Edge::clockwise : Edge::counterclockwise;
          bit.directions.edgeTypes.second |= (random8(2) == 0) ? Edge::loop1 : Edge::loop2;
        }
      }
      if (random16() < 16) {
        if (bit.directions.edgeTypes.first & Edge::inbound) {
          bit.directions.edgeTypes.first = Edge::outbound;
        } else {
          bit.directions.edgeTypes.first = Edge::inbound;
        }
      }
    };
  }

  ~WanderingFew() {
    delete bitsFiller;
  }

  void update() {
    bitsFiller->update();

    for (int i = 0; i < colorManager->trackedColorsCount(); ++i) {
      bitsFiller->bits[i].color = colorManager->getTrackedColor(i);
    }
    colorManager->paletteRotationTick();
  }

  const char *description() {
    return "WanderingFew";
  }
};

class SpiralSource : public Pattern, PaletteRotation<CRGBPalette256> {
protected:
  BitsFiller *bitsFiller;
public:
  SpiralSource() {
    vector<EdgeTypes> directions = {EdgeType::none};
    bitsFiller = new BitsFiller(ctx, 0, 42, 2400, directions);
    bitsFiller->fadeDown = 8;
    bitsFiller->fadeUpDistance = 2;
    bitsFiller->flowRule = BitsFiller::priority;
    bitsFiller->maxBitsPerSecond = 1;

    bitsFiller->handleUpdateBit = [this](BitsFiller::Bit &bit) {
      bit.color = this->colorManager->getPaletteColor(bit.colorIndex);
      if (bit.age() > bit.lifespan - (bit.lifespan / 2)) {
        bit.brightness = 0xFF * (bit.lifespan - bit.age()) / (bit.lifespan / 2);
        bit.brightness = dim8_raw(bit.brightness);
      }

      // switch loops sometimes
      if (random16() < 512) {
          bool choice = random8(2);
          bit.directions.edgeTypes.second = Edge::counterclockwise | ((choice == 0) ? Edge::loop1 : Edge::loop2);
          bit.directions.edgeTypes.third = Edge::counterclockwise | ((choice == 0) ? Edge::loop2 : Edge::loop1);
      }
    };
  }

  ~SpiralSource() {
    delete bitsFiller;
  }

  unsigned long lastSpawn = 0;
  uint8_t spiralIndex = 0; //0-2

  void update() {
    bitsFiller->update();

    for (int i = 0; i < colorManager->trackedColorsCount(); ++i) {
      bitsFiller->bits[i].color = colorManager->getTrackedColor(i);
    }
    colorManager->paletteRotationTick();

    if (millis() - lastSpawn > beatsin88((uint16_t)(1.8*256), 40, 120)) {
      BitsFiller::Bit &bit = bitsFiller->addBit();
      bit.directions = MakeEdgeTypesQuad(Edge::outbound, Edge::loop2|Edge::counterclockwise, 0);
      bit.colorIndex = 0xFF*millis()/1000/4;
      bit.color = getPaletteColor(bit.colorIndex);
      bit.px = HLSpiralCenters[spiralIndex];
      bit.lifespan = beatsin88((uint16_t)(3.14*256), 1000, 3000) + random8();
      spiralIndex = (spiralIndex + 1) % 3;
      bitsFiller->speed = beatsin88((uint16_t)(2.2*256), 32, 48);

      lastSpawn = millis();
    }
  }

  const char *description() {
    return "SpiralSource";
  }
};

#endif
