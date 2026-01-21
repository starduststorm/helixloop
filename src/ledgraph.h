#ifndef LEDGRAPH_H
#define LEDGRAPH_H

#include <vector>
#include <algorithm>
#include <FastLED.h>
#include <set>
#include <util.h>

#include <drawing.h>
#include <mapping.h>

using namespace std;

typedef enum : uint8_t {
   none             = 0,
   unused           = 1 << 6,
   helix1           = 1 << 5,
   helix2           = 1 << 4,
   inbound          = 1 << 3,
   outbound         = 1 << 2,
   clockwise        = 1 << 1,
   counterclockwise = 1 << 0,
   all              = 0xFF,
} EdgeType;

const uint8_t EdgeTypesCount = 8;

Graph ledgraph;

#define PRIMARY_HELIX_LED_COUNT 120
#define SECONDARY_HELIX_LED_COUNT 108
#define SPIRAL_COUNT 3
#define SPIRAL_LED_COUNT 47
static_assert(PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT + 3 * SPIRAL_LED_COUNT == LED_COUNT, "LED_COUNT issue");

#define SPIRAL_FIRST_ENTRANCE_INDEX 134
#define SPIRAL_FIRST_INDEX 228

#define SPIRAL_CENTERS 228,275,322
const PixelIndex HLSpiralCenters[] = {SPIRAL_CENTERS};

const PixelIndex helixIntersections[]    = {9,   20,  29,  40,  49,  60,  69,  80,  89,  100, 109, 0};

void initLEDGraph() {
    ledgraph = Graph({}, LED_COUNT);

      ledgraph.transposeMap = {
                            {helix1,helix1}, 
                            {helix2,helix2}, 
                            {inbound,outbound}, 
                            {outbound,inbound}, 
                            {clockwise,counterclockwise}, 
                            {counterclockwise,clockwise}, 
                          };

    for (int i = 0; i < PRIMARY_HELIX_LED_COUNT-1; ++i) {
        // first helix wiring
        ledgraph.addEdge(Edge(i, i+1, EdgeType::counterclockwise | EdgeType::helix1));
    }
    ledgraph.addEdge(Edge(PRIMARY_HELIX_LED_COUNT-1, 0, EdgeType::counterclockwise | EdgeType::helix1));

    // Connect second helix non-intersections
    for (int i = PRIMARY_HELIX_LED_COUNT; i < PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT; ++i) {
        int parity = i % 18; // 18 pixels in each secondary loop section
        if (parity != 3 && parity != 11) {
            // Part of second helix wiring, not intersection
            ledgraph.addEdge(Edge(i, i+1, EdgeType::counterclockwise | EdgeType::helix2));
        }
    }

    // Connect intersections
    int preIntersections[] = {129, 137, 147, 155, 165, 173, 183, 191, 201, 209, 219, 227};
    for (unsigned i = 0; i < ARRAY_SIZE(preIntersections); ++i) {
        int intersectionIndex = helixIntersections[i];
        int preIntersectionIndex = preIntersections[i];
        ledgraph.addEdge(Edge(preIntersectionIndex, intersectionIndex, EdgeType::counterclockwise | EdgeType::helix2));
        if (i != ARRAY_SIZE(preIntersections)-1) {
            // don't connect the helix wiring endpoint to the spiral
            ledgraph.addEdge(Edge(intersectionIndex, preIntersectionIndex+1, EdgeType::counterclockwise | EdgeType::helix2));
        }
    }
    ledgraph.addEdge(Edge(0, PRIMARY_HELIX_LED_COUNT, EdgeType::counterclockwise | EdgeType::helix2));

    // Three spirals
    for (int s = 0; s < SPIRAL_COUNT; ++s) {
        for (int i = 0; i < SPIRAL_LED_COUNT - 1; ++i) {
            int px = SPIRAL_FIRST_INDEX + s * SPIRAL_LED_COUNT + i;
            ledgraph.addEdge(Edge(px, px+1, EdgeType::outbound));
        }
        // Connect the helix to the spiral entrance
        // Spiral entrances are asymmetric on edge types, so we can differentiate the three-pixel connection properly
        PixelIndex helixExitIndex = SPIRAL_FIRST_ENTRANCE_INDEX + s * SECONDARY_HELIX_LED_COUNT / 3;
        PixelIndex spiralEntranceIndex = SPIRAL_FIRST_INDEX + (SPIRAL_COUNT-s) * SPIRAL_LED_COUNT - 1;
        ledgraph.addEdge(Edge(helixExitIndex, spiralEntranceIndex, EdgeType::inbound), false);
        ledgraph.addEdge(Edge(spiralEntranceIndex, helixExitIndex, EdgeType::outbound | EdgeType::counterclockwise | EdgeType::helix2), false);

        ledgraph.addEdge(Edge(helixExitIndex - 1, spiralEntranceIndex, EdgeType::inbound), false);
        ledgraph.addEdge(Edge(spiralEntranceIndex, helixExitIndex - 1, EdgeType::outbound | EdgeType::clockwise | EdgeType::helix2), false);
    }
}

void graphTest(DrawingContext &ctx) {
    ctx.leds.fill_solid(CRGB::Black);
    int leader = (360+millis() / 200) % LED_COUNT;
    // leader = millis() % 2000 < 1000 ? 0 : 228;
    
    for (int i = 0; i < LED_COUNT/41;++i) {
        vector<Edge> edges = ledgraph.adjList[(leader+41*i)%LED_COUNT];
        // logf("Highlighting leader = %i with %i edges", leader, edges.size());
        for (Edge edge : edges) {
            ctx.leds[edge.from] = CRGB::Blue;
            ctx.leds[edge.to] = (edge.types & EdgeType::counterclockwise ? CRGB::Green : CRGB::Red);
        }
    }
}

#endif
