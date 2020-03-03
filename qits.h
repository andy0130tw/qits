#ifndef __QITS_QITS_H
#define __QITS_QITS_H

#include <cinttypes>
#include <vector>
#include <bitset>
#include "zobrist_values.h"

#define eprintf(...)  fprintf(stderr, __VA_ARGS__)

using namespace std;


static const int MAP_W = 20;
static const int MAP_H = 14;
static const int MAP_SIZE = MAP_W * MAP_H;

static const int MAX_FIRE = 256;


// FIXME: use X macros to tidy up these snippets
enum class ObjectType: unsigned char {
    EMPTY = 0,
    WALL,
    ICE,
    FIRE,
    RECYCLER,
    ICE_GOLD,
    MAGICIAN,

    _UNIMPLEMENTED,
    AR_UP,
    AR_DOWN,
    AR_LEFT,
    AR_RIGHT,
    DISPENSER,
    UNKNOWN,

    _ALL,
};

enum class Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    _ALL,
};

using PatType = bitset<MAX_FIRE>;

struct BoardConfiguration {
    vector<int> fires;
    vector<unsigned char> iceType;
    ObjectType map[MAP_SIZE];
};

#endif  // __QITS_QITS_H
