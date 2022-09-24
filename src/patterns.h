#ifndef PATTERN_H
#define PATTERN_H

#include <FastLED.h>
#include <vector>
#include <functional>

#include "util.h"
#include "palettes.h"
#include "ledgraph.h"
#include "drawing.h"

typedef PaletteRotation<PaletteType> * ColorManager;

class Pattern {
private:  
  long startTime = -1;
  long stopTime = -1;
  long lastUpdateTime = -1;
public:
  ColorManager colorManager;
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

  virtual void colorModeChanged() { }
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
    
    uint8_t px;
    EdgeTypesPair directions;
    
    unsigned long lifespan;

    CRGB color;
    uint8_t brightness = 0xFF;

    Bit(int px, EdgeTypesPair directions, unsigned long lifespan) 
      : px(px), directions(directions), lifespan(lifespan) {
      reset();
    }

    void reset() {
      birthmilli = millis();
      color = CHSV(random8(), 0xFF, 0xFF);
    }

    unsigned long age() {
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
    EdgeTypesPair directionsForBit = {0};

    for (int n = 0; n < 2; ++n) {
      uint8_t bit[EdgeTypesCount] = {0};
      uint8_t bitcount = 0;
      for (int i = 0; i < EdgeTypesCount; ++i) {
        uint8_t nybble = bitDirections.pair >> (n * EdgeTypesCount);
        if (nybble & 1 << i) {
          bit[bitcount++] = i;
        }
      }
      if (bitcount) {
        directionsForBit.pair |= 1 << (bit[random8()%bitcount] + n * EdgeTypesCount);
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
    bits.erase(bits.begin() + bitIndex);
  }

  void splitBit(Bit &bit, uint8_t toIndex) {
    Bit &split = makeBit(&bit);
    split.px = toIndex;
  }

  bool isIndexAllowed(uint8_t index) {
    if (allowedPixels) {
      return allowedPixels->end() != allowedPixels->find(index);
    }
    return true;
  }

  vector<uint8_t> nextIndexes(uint8_t index, EdgeTypesPair bitDirections) {
    vector<uint8_t> next;
    switch (flowRule) {
      case priority: {
        auto adj = ledgraph.adjacencies(index, bitDirections);
        for (auto edge : adj) {
          if (isIndexAllowed(edge.to)) {
            next.push_back(edge.to);
            break;
          }
        }
        break;
      }
      case random:
      case split: {
        vector<Edge> nextEdges;
        auto adj = ledgraph.adjacencies(index, bitDirections);
        for (auto a : adj) {
          if (isIndexAllowed(a.to)) {
            nextEdges.push_back(a);
          }
        }
        if (flowRule == split) {
          if (nextEdges.size() == 1) {
            // flow normally if we're not actually splitting
            next.push_back(nextEdges.front().to);
          } else {
            // split along all allowed split directions, or none if none are allowed
            for (Edge nextEdge : nextEdges) {
              if (splitDirections & nextEdge.type) {
                next.push_back(nextEdge.to);
              }
            }
          }
        } else if (nextEdges.size() > 0) {
          // FIXME: EdgeType::random behavior doesn't work right with the way fadeUp is implemented
          next.push_back(nextEdges.at(random8()%nextEdges.size()).to);
        }
        break;
      }
    }
    // TODO: does not handle duplicates in the case of the same vertex being reachable via multiple edges
    assert(next.size() <= 4, "no pixel in this design has more than 4 adjacencies but index %i had %u", index, next.size());
    return next;
  }

  bool flowBit(uint8_t bitIndex) {
    vector<uint8_t> next = nextIndexes(bits[bitIndex].px, bits[bitIndex].directions);
    if (next.size() == 0) {
      // leaf behavior
      killBit(bitIndex);
      return false;
    } else {
      bits[bitIndex].px = next.front();
      for (unsigned i = 1; i < next.size(); ++i) {
        splitBit(bits[bitIndex], next[i]);
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
        Serial.print(bit.directions.pair & (1 << i));
      }
      Serial.println();
    }
    logf("--------");
  }

  vector<Bit> bits;
  uint8_t maxSpawnBits;
  uint8_t maxBitsPerSecond = 0; // limit how fast new bits are spawned, 0 = no limit
  uint8_t speed; // in pixels/second
  EdgeTypesPair bitDirections;

  unsigned long lifespan = 0; // in milliseconds, forever if 0

  FlowRule flowRule = random;
  SpawnRule spawnRule = maintainPopulation;
  uint8_t fadeUpDistance = 0; // fade up n pixels ahead of bit motion
  EdgeTypes splitDirections = EdgeType::all; // if flowRule is split, which directions are allowed to split
  
  const vector<uint8_t> *spawnPixels = NULL; // list of pixels to automatically spawn bits on
  const set<uint8_t> *allowedPixels = NULL; // set of pixels that bits are allowed to travel to

  function<void(Bit &)> handleNewBit = [](Bit &bit){};
  function<void(Bit &)> handleUpdateBit = [](Bit &bit){};

  BitsFiller(EVMDrawingContext &ctx, uint8_t maxSpawnBits, uint8_t speed, unsigned long lifespan, vector<EdgeTypes> bitDirections)
    : ctx(ctx), maxSpawnBits(maxSpawnBits), speed(speed), lifespan(lifespan) {
      this->bitDirections = MakeEdgeTypesPair(bitDirections);
    bits.reserve(maxSpawnBits);
  };

  void fadeUpForBit(Bit &bit, uint8_t px, int distanceRemaining, unsigned long lastMove) {
    vector<uint8_t> next = nextIndexes(px, bit.directions);

    unsigned long mils = millis();
    unsigned long fadeUpDuration = 1000 * fadeUpDistance / speed;
    for (uint8_t n : next) {
      unsigned long fadeTimeSoFar = mils - lastMove + distanceRemaining * 1000/speed;
      uint8_t progress = 0xFF * fadeTimeSoFar / fadeUpDuration;

      CRGB existing = ctx.leds[n];
      CRGB blended = blend(existing, bit.color, dim8_raw(progress));
      blended.nscale8(bit.brightness);
      ctx.leds[n] = blended;
      
      if (distanceRemaining > 0) {
        fadeUpForBit(bit, n, distanceRemaining-1, lastMove);
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
        if (bitAlive && bits[i].lifespan != 0 && bits[i].age() > bits[i].lifespan) {
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

class DownstreamPattern : public Pattern {
protected:
  BitsFiller *bitsFiller;
  unsigned circleBits = 0;
  unsigned numAutoRotateColors = 3;
  unsigned numAutoRotatePaletteCycles = 1;
public:
  DownstreamPattern() {
    EdgeType circledirection = (random8()%2 ? EdgeType::clockwise : EdgeType::counterclockwise);
    vector<EdgeTypes> directions = {circledirection, EdgeType::outbound};
    bitsFiller = new BitsFiller(ctx, 0, 24, 0, directions);
    bitsFiller->flowRule = BitsFiller::split;
  }
  ~DownstreamPattern() {
    delete bitsFiller;
  }

  void update() {
    bitsFiller->update();

    for (int i = 0; i < colorManager->trackedColorsCount(); ++i) {
      bitsFiller->bits[i].color = colorManager->getTrackedColor(i);
    }
    colorManager->paletteRotationTick();
  }

  virtual void colorModeChanged() {
    unsigned oldCircleBits = circleBits;
    if (colorManager->pauseRotation) {
      // color manager will track flag bands
      circleBits = colorManager->trackedColorsCount();
    } else {
      // keep it simple with 3 bits with doing full palette rotation
      colorManager->prepareTrackedColors(numAutoRotateColors, numAutoRotatePaletteCycles);
      circleBits = numAutoRotateColors;
    }
     
    if (circleBits != oldCircleBits) {
      bitsFiller->removeAllBits();
      for (unsigned i = 0; i < circleBits; ++i) {
        BitsFiller::Bit &bit = bitsFiller->addBit();
        bit.px = circleleds[i * circleleds.size() / circleBits];
      }
    }

    bitsFiller->fadeDown = circleBits+1;
    bitsFiller->fadeUpDistance = max(2, 6-(int)circleBits);
  }

  const char *description() {
    return "downstream";
  }
};

#endif
