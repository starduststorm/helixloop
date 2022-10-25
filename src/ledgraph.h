#ifndef LEDGRAPH_H
#define LEDGRAPH_H

#include <vector>
#include <algorithm>
#include <FastLED.h>
#include <set>
#include <util.h>

#include "drawing.h"

using namespace std;

typedef uint16_t PixelIndex;

const uint8_t EdgeTypesCount = 8;
typedef uint8_t EdgeTypes;

typedef union {
  struct {
    EdgeTypes first;
    EdgeTypes second;
  } edgeTypes;
  uint16_t pair;
} EdgeTypesPair;

struct Edge {
    typedef enum : uint8_t {
        none             = 0,
        unused           = 1 << 7,
        loop1            = 1 << 6,
        loop2            = 1 << 5,
        warp             = 1 << 4,
        inbound          = 1 << 3,
        outbound         = 1 << 2,
        clockwise        = 1 << 1,
        counterclockwise = 1 << 0,
        all              = 0xFF,
    } EdgeType;
    
    PixelIndex from, to;
    EdgeTypes types;
    
    Edge(PixelIndex from, PixelIndex to, EdgeType type) : from(from), to(to), types(type) {};
    Edge(PixelIndex from, PixelIndex to, EdgeTypes types) : from(from), to(to), types(types) {};

    Edge transpose() {
        EdgeTypes transposeTypes = none;
        if (types & loop1) transposeTypes |= loop1;
        if (types & loop2) transposeTypes |= loop2;
        if (types & warp) transposeTypes |= warp;
        if (types & inbound) transposeTypes |= outbound;
        if (types & outbound) transposeTypes |= inbound;
        if (types & clockwise) transposeTypes |= counterclockwise;
        if (types & counterclockwise) transposeTypes |= clockwise;
        return Edge(to, from, transposeTypes);
    }
};

typedef Edge::EdgeType EdgeType;
typedef uint8_t EdgeTypes;

EdgeTypesPair MakeEdgeTypesPair(EdgeTypes first, EdgeTypes second) {
    EdgeTypesPair pair;
    pair.edgeTypes.first = first;
    pair.edgeTypes.second = second;
    return pair;
}

EdgeTypesPair MakeEdgeTypesPair(vector<EdgeTypes> vec) {
    assert(vec.size() <= 2, "only two edge type directions allowed");
    unsigned size = vec.size();
    EdgeTypesPair pair = {0};
    if (size > 0) {
        pair.edgeTypes.first = vec[0];
    }
    if (size > 1) {
        pair.edgeTypes.second = vec[1];
    }
    return pair;
}

class Graph {
public:
    vector<vector<Edge> > adjList;
    Graph() { }
    Graph(vector<Edge> const &edges, int count) {
        adjList.resize(count);

        for (auto &edge : edges) {
            addEdge(edge);
        }
    }

    void addEdge(Edge edge, bool bidirectional=true) {
        adjList[edge.from].push_back(edge);
        if (bidirectional) {
            adjList[edge.to].push_back(edge.transpose());
        }
    }

    vector<Edge> adjacencies(PixelIndex vertex, EdgeTypesPair pair) {
        vector<Edge> adjList;
        getAdjacencies(vertex, pair.edgeTypes.first, adjList);
        getAdjacencies(vertex, pair.edgeTypes.second, adjList);
        return adjList;
    }

    void getAdjacencies(PixelIndex vertex, EdgeTypes matching, std::vector<Edge> &insertInto) {
        if (matching == 0) {
            return;
        }
        vector<Edge> &adj = adjList[vertex];
        for (Edge &edge : adj) {
            if ((edge.types & matching) == matching) {
                insertInto.push_back(edge);
            }
        }
    }
};

#define LED_COUNT (369)

Graph ledgraph;

#define PRIMARY_HELIX_LED_COUNT 120
#define SECONDARY_HELIX_LED_COUNT 108
#define SPIRAL_COUNT 3
#define SPIRAL_LED_COUNT 47
static_assert(PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT + 3 * SPIRAL_LED_COUNT == LED_COUNT, "LED_COUNT issue");

#define SPIRAL_FIRST_ENTRANCE_INDEX 133
#define SPIRAL_FIRST_INDEX 228

const vector<PixelIndex> someleds = {};

#define SPIRAL_CENTERS 228,275,322
const PixelIndex HLSpiralCenters[] = {SPIRAL_CENTERS};

void initLEDGraph() {
    ledgraph = Graph({}, LED_COUNT);

    for (int i = 0; i < PRIMARY_HELIX_LED_COUNT-1; ++i) {
        // first helix wiring
        ledgraph.addEdge(Edge(i, i+1, Edge::counterclockwise | Edge::loop1));
    }
    ledgraph.addEdge(Edge(PRIMARY_HELIX_LED_COUNT-1, 0, Edge::counterclockwise | Edge::loop1));

    // Connect second helix non-intersections
    for (int i = PRIMARY_HELIX_LED_COUNT; i < PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT; ++i) {
        int parity = i % 18; // 18 pixels in each secondary loop section
        if (parity != 3 && parity != 11) {
            // Part of second helix wiring, not intersection
            ledgraph.addEdge(Edge(i, i+1, Edge::counterclockwise | Edge::loop2));
        }
    }

    // Connect intersections
    int preIntersections[] = {129, 137, 147, 155, 165, 173, 183, 191, 201, 209, 219, 227};
    int intersections[]    = {9,   20,  29,  40,  49,  60,  69,  80,  89,  100, 109, 0};
    for (unsigned i = 0; i < ARRAY_SIZE(preIntersections); ++i) {
        int intersectionIndex = intersections[i];
        int preIntersectionIndex = preIntersections[i];
        ledgraph.addEdge(Edge(preIntersectionIndex, intersectionIndex, Edge::counterclockwise | Edge::loop2));
        if (i != ARRAY_SIZE(preIntersections)-1) {
            // don't connect the helix wiring endpoint to the spiral
            ledgraph.addEdge(Edge(intersectionIndex, preIntersectionIndex+1, Edge::counterclockwise | Edge::loop2));
        }
    }
    ledgraph.addEdge(Edge(0, PRIMARY_HELIX_LED_COUNT, Edge::counterclockwise | Edge::loop2));

    // Three spirals
    for (int s = 0; s < SPIRAL_COUNT; ++s) {
        for (int i = 0; i < SPIRAL_LED_COUNT - 1; ++i) {
            int px = SPIRAL_FIRST_INDEX + s * SPIRAL_LED_COUNT + i;
            ledgraph.addEdge(Edge(px, px+1, Edge::outbound));
        }
        // Connect the helix to the spiral entrance
        // Spiral entrances are asymmetric on edge types, so we can differentiate the three-pixel connection properly
        PixelIndex helixExitIndex = SPIRAL_FIRST_ENTRANCE_INDEX + s * SECONDARY_HELIX_LED_COUNT / 3;
        PixelIndex spiralEntranceIndex = SPIRAL_FIRST_INDEX + (SPIRAL_COUNT-s) * SPIRAL_LED_COUNT - 1;
        ledgraph.addEdge(Edge(helixExitIndex, spiralEntranceIndex, Edge::inbound), false);
        ledgraph.addEdge(Edge(spiralEntranceIndex, helixExitIndex, Edge::outbound | Edge::clockwise | Edge::loop2), false);

        ledgraph.addEdge(Edge(helixExitIndex + 1, spiralEntranceIndex, Edge::inbound), false);
        ledgraph.addEdge(Edge(spiralEntranceIndex, helixExitIndex + 1, Edge::outbound | Edge::counterclockwise | Edge::loop2), false);
    }
}

typedef CustomDrawingContext<LED_COUNT, 1, CRGB, CRGBArray<LED_COUNT> > DrawingContext;

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
