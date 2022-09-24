#ifndef LEDGRAPH_H
#define LEDGRAPH_H

#include <vector>
#include <algorithm>
#include <FastLED.h>
#include <set>
#include <util.h>

#include "drawing.h"

using namespace std;

const uint8_t EdgeTypesCount = 4;

typedef union {
  struct {
    uint8_t first:4;
    uint8_t second:4;
  } edgeTypes;
  uint8_t pair;
} EdgeTypesPair;

struct Edge {
    typedef enum : uint8_t {
        none             = 0,
        inbound          = 1 << 0,
        outbound         = 1 << 1,
        clockwise        = 1 << 2,
        counterclockwise = 1 << 3,
        all              = 0xFF,
    } EdgeType;
    
    uint8_t from, to;
    EdgeType type;
    Edge(uint8_t from, uint8_t to, EdgeType type) : from(from), to(to), type(type) {};
    Edge transpose() {
        EdgeType transposeType;
        switch (type) {
            case inbound: transposeType = outbound; break;
            case outbound: transposeType = inbound; break;
            case clockwise: transposeType = counterclockwise; break;
            case counterclockwise: transposeType = clockwise; break;
            default: transposeType = none; break;
        }
        return Edge(to, from, transposeType);
    }
};

typedef Edge::EdgeType EdgeType;
typedef uint8_t EdgeTypes;

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

    vector<Edge> adjacencies(uint8_t vertex, EdgeTypesPair pair) {
        vector<Edge> adjList;
        getAdjacencies(vertex, pair.edgeTypes.first, adjList);
        getAdjacencies(vertex, pair.edgeTypes.second, adjList);
        return adjList;
    }

    void getAdjacencies(uint8_t vertex, EdgeTypes matching, std::vector<Edge> &insertInto) {
        if (matching == 0) {
            return;
        }
        vector<Edge> &adj = adjList[vertex];
        for (Edge &edge : adj) {
            if (edge.type & matching) {
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
assert(PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT + 3 * SPIRAL_LED_COUNT == LED_COUNT, "LED_COUNT issue");

#define SPIRAL_FIRST_ENTRANCE_INDEX 133
#define SPIRAL_FIRST_INDEX 228

const vector<uint16_t> someleds = {};
const set<uint8_t> circleMarsLeds = {CIRCLE_LEDS, MARS_LEDS};

void initLEDGraph() {
    vector<Edge> edges = {
        //30, 12, Edge::outbound}, {38, 39, Edge::outbound}, {65,66, Edge::outbound},
    };
    ledgraph = Graph(edges, LED_COUNT);

    for (int i = 0; i < PRIMARY_HELIX_LED_COUNT; ++i) {
        // first helix wiring
        ledgraph.addEdge(Edge(i, i+1, Edge::counterclockwise));
    }

    // Connect intersections    
    for (int i = PRIMARY_HELIX_LED_COUNT; i < PRIMARY_HELIX_LED_COUNT + SECONDARY_HELIX_LED_COUNT; ++i) {
        int parity = i % 18; // 18 pixels in each secondary loop section
        if (parity == 3 || parity == 11) {
            // pixel prior to helix intersection
            int intersectionIndex = i - PRIMARY_HELIX_LED_COUNT;
            ledgraph.addEdge(Edge(i, intersectionIndex, Edge::counterclockwise));
            ledgraph.addEdge(Edge(intersectionIndex, i+1, Edge::counterclockwise));
        } else {
            // Part of second helix wiring
            ledgraph.addEdge(Edge(i, i+1, Edge::counterclockwise));
        }
    }

    // Three spirals
    for (int s = 0; s < SPIRAL_COUNT; ++s) {
        for (int i = 0; i < SPIRAL_LED_COUNT - 1; ++i) {
            int px = SPIRAL_FIRST_INDEX + s * SPIRAL_LED_COUNT + i;
            ledgraph.addEdge(Edge(px, px+1, Edge::outbound));
        }
        // Connect the helix to the spiral entrance
        int helixExitIndex = SPIRAL_FIRST_ENTRANCE_INDEX + s * SECONDARY_HELIX_LED_COUNT / 3;
        ledgraph.addEdge(Edge(helixExitIndex, 
                              SPIRAL_FIRST_INDEX + (SPIRAL_COUNT-s) * SPIRAL_LED_COUNT - 1, 
                              Edge::inbound));
        ledgraph.addEdge(Edge(helixExitIndex + 1, 
                              SPIRAL_FIRST_INDEX + (SPIRAL_COUNT-s) * SPIRAL_LED_COUNT - 1, 
                              Edge::inbound));
    }
}

typedef CustomDrawingContext<LED_COUNT, 1, CRGB, CRGBArray<LED_COUNT> > DrawingContext;

#endif
