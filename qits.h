#ifndef __QITS_QITS_H
#define __QITS_QITS_H

#include <cinttypes>
#include <vector>
#include <bitset>
#include <unordered_map>
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


class PatternDatabase {
public:
    PatternDatabase() {
        reset();
    }

    auto size() { return id2pat.size(); }

    void reset() {
        id2pat.clear();
        id2pat.reserve(131072);
        pat2id.clear();
        // initialize an empty pattern as id 0
        queryByPat({});
    }

    unsigned int queryByPat(PatType pat) {
        auto it = pat2id.find(pat);
        if (it != pat2id.end()) {
            return it->second;
        }

        int newId = size();
        pat2id.insert({pat, newId});
        id2pat.push_back(pat);
        return newId;
    }

    PatType queryById(unsigned int id) {
        if (id >= id2pat.size()) {
            return PatType(0);
        }
        return id2pat[id];
    }
private:
    unordered_map<PatType, unsigned int> pat2id;
    vector<PatType> id2pat;
};

struct BoardConfiguration {
    vector<int> fires;
    vector<unsigned char> iceType;
    ObjectType map[MAP_SIZE];
};


struct InitialState {
    vector<int> icePositions;
};

struct State {
    union {
        InitialState* initial;  // if age == 0
        State* previous;        // otherwise
    };

    uint64_t hash;
    int magicianPos;

    /* only meaningful when age > 0 */
    unsigned short int age;
    short int movedIceIndex;
    short int oldPosition;
    short int newPosition;

    unsigned int clearedFiresPatId;

    static PatternDatabase patdb;

    PatType getClearedFires() const {
        auto kk = State::patdb.queryById(clearedFiresPatId);
        return kk;
    }
    unsigned int setClearedFiresPat(PatType pat) {
        unsigned int id = State::patdb.queryByPat(pat);
        return (clearedFiresPatId = id);
    }

    void print() const {
        printf("<State age=%d, pos=%d", age, magicianPos);
        if (age > 0) {
            auto clearedFires = State::patdb.queryById(clearedFiresPatId);
            printf(", #%d: %d -> %d, cf=[", movedIceIndex, oldPosition, newPosition);
            bool first = true;
            for (size_t i = 0; i < clearedFires.size(); i++) {
                if (!clearedFires[i]) continue;
                printf(first ? "%zd" : ",%zd", i);
                first = false;
            }
        }
        printf("]>\n");
    }
};

struct BoardChange {
    State state;

    // often, it has only one element,
    // but golden ices make it uncertain QQ
    vector<int> posClearedFires;
};

#endif  // __QITS_QITS_H
