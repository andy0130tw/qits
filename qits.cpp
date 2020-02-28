#include <cstdio>
#include <vector>
#include <queue>
#include <bitset>
#include <algorithm>
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

struct BoardConfigration {
    vector<int> fires;
    vector<unsigned char> iceType;
    ObjectType map[MAP_SIZE];
    char fireToIndex[MAP_SIZE];
    int moveDest[static_cast<int>(Direction::_ALL)][MAP_SIZE];

    void preprocess() {
        computeFireToIndex();
        computeMoveDest();
    }

private:
    void computeFireToIndex() {
        for (int i = 0; i < MAP_SIZE; i++) {
            fireToIndex[i] = -1;
        }
        for (size_t i = 0; i < fires.size(); i++) {
            fireToIndex[fires[i]] = i;
        }
    }

    void computeMoveDest() {
        // for a static block (except fire, because they can be removed),
        // moveDest is not defined
        // or it points to its nearest
        //  - non-empty block of specified direction, or
        //  - one cell off wall

        #define LOOP_BODY(DIR)  {  \
                int pos = base + offs;  \
                if (map[pos] == ObjectType::WALL) {  \
                    moveDest[static_cast<int>(DIR)][pos] = -1;  \
                    bound = pos + step;  /* overflowed value is not used :) */  \
                } else {  \
                    moveDest[static_cast<int>(DIR)][pos] = bound;  \
                    if (map[pos] != ObjectType::EMPTY) {  \
                        bound = pos;  \
                    }  \
                }  \
            }

        for (int base = 0; base < MAP_SIZE; base += MAP_W) {
            int bound = base, step = 1;
            for (int offs = 0; offs < MAP_W; offs += step) {
                LOOP_BODY(Direction::LEFT);
            }
        }

        for (int base = 0; base < MAP_SIZE; base += MAP_W) {
            int bound = base + MAP_W - 1, step = -1;
            for (int offs = MAP_W - 1; offs > 0; offs += step) {
                LOOP_BODY(Direction::RIGHT);
            }
        }

        for (int base = 0; base < MAP_W; base += 1) {
            int bound = base, step = MAP_W;
            for (int offs = 0; offs < MAP_SIZE; offs += step) {
                LOOP_BODY(Direction::UP);
            }
        }

        for (int base = 0; base < MAP_W; base += 1) {
            int bound = base + MAP_SIZE - MAP_W, step = -MAP_W;
            for (int offs = MAP_SIZE; offs > 0; offs += step) {
                LOOP_BODY(Direction::DOWN);
            }
        }

        #undef LOOP_BODY
    }
};

struct InitialState {
    vector<int> icePositions;
};

struct State {
    union {
        InitialState* initial;  // if age == 0
        State* previous;        // otherwise
    };

    unsigned int age;
    int magicianPos;

    /* only meaningful when age > 0 */
    int movedIceIndex;
    int newPosition;

    bitset<MAX_FIRE> clearedFires;
};


ObjectType reprToObjectType(char c) {
    switch (c) {
    case ' ': return ObjectType::EMPTY;
    case '#': return ObjectType::WALL;
    case '%': return ObjectType::ICE;
    case '*': return ObjectType::FIRE;
    case '-': return ObjectType::RECYCLER;
    case '$': return ObjectType::ICE_GOLD;
    case '@': return ObjectType::MAGICIAN;
    case '<': return ObjectType::AR_LEFT;
    case '>': return ObjectType::AR_RIGHT;
    case '^': return ObjectType::AR_UP;
    case 'v': return ObjectType::AR_DOWN;
    case '+': return ObjectType::DISPENSER;
    }
    return ObjectType::UNKNOWN;
}

char objectTypeToRepr(ObjectType tp) {
    const char idx2repr[] = " #%*-$@?<>^v+?";
    return idx2repr[static_cast<int>(tp)];
}

vector<int> icePositionsAtState(const State& state) {
    vector<const State*> slist {};
    const State* s = &state;

    slist.reserve(s->age);
    while (s->age != 0) {
        slist.push_back(s);
        s = s->previous;
    }

    InitialState* initial = s->initial;
    vector<int> poss = initial->icePositions;

    for (auto sp: slist) {
        poss[sp->movedIceIndex] = sp->newPosition;
    }

    return poss;
}

void printConfiguration(BoardConfigration& board, State& state) {
    char buf[MAP_SIZE] {};

    for (int i = 0; i < MAP_SIZE; i++) {
        buf[i] = objectTypeToRepr(board.map[i]);
    }

    auto ices = icePositionsAtState(state);

    for (auto i: ices) {
        buf[i] = '%';
    }

    for (int i = 0; i < MAP_H; i++) {
        for (int j = 0; j < MAP_W; j++) {
            printf("%c", buf[i * MAP_W + j]);
        }
        puts("");
    }

    printf("Fire block list:\n");
    for (size_t i = 0; i < board.fires.size(); i++) {
        printf("| #%2zd: %d\n", i, board.fires[i]);
    }

    printf("Ice block list:\n");
    for (size_t i = 0; i < ices.size(); i++) {
        printf("| #%2zd: %d, tp=%d\n", i, ices[i], board.iceType[i]);
    }
}

// returns the position it stops at, if the ice block of index `idx`
// is pushed toward that direction. If it encounters a fire first, the
// position of that fire is returned instead.
int moveToward(const BoardConfigration& board, const State& s, unsigned int idx, Direction d) {
    auto ices = icePositionsAtState(s);
    int posOrig = ices[idx];

    int posCand = posOrig;

    while (1) {
        int newPos = board.moveDest[static_cast<int>(d)][posCand];
        // be careful of infinite loop!
        if (newPos == posCand) {
            break;
        }
        posCand = newPos;
        int fidx = board.fireToIndex[posCand];
        // continue moving if a fire is cleared
        if (!(fidx >= 0 && s.clearedFires[fidx])) {
            break;
        }
    }

    // note that either value is used in each branch; no need to
    // update these value if posCand is updated
    int rowCand = posCand / MAP_W;
    int colCand = posCand % MAP_W;

    // do a search on ice blocks to "limit" the pos an ice can go
    if (d == Direction::LEFT) {
        for (auto b: ices) {
            if (b / MAP_W == rowCand && b > posCand && b < posOrig) {
                posCand = b + 1;
            }
        }
    } else if (d == Direction::RIGHT) {
        for (auto b: ices) {
            if (b / MAP_W == rowCand && b > posOrig && b < posCand) {
                posCand = b - 1;
            }
        }
    } else if (d == Direction::UP) {
        for (auto b: ices) {
            if (b % MAP_W == colCand && b > posCand && b < posOrig) {
                posCand = b + MAP_W;
            }
        }
    } else {  // d == Direction::DOWN
        for (auto b: ices) {
            if (b % MAP_W == colCand && b > posOrig && b < posCand) {
                posCand = b - MAP_W;
            }
        }
    }

    return posCand;
}

bool readFloorFileFromStdin(BoardConfigration& board, InitialState& state_init, State& state_root) {
    for (int i = 0; i < MAP_H; i++) {
        int idxBase = i * MAP_W;
        int idx = idxBase;

        char c;
        while (scanf("%c", &c) == 1 && c != '\n') {
            if (idx - idxBase == MAP_W) {
                continue;
            }
            ObjectType tp = reprToObjectType(c);
            if (tp == ObjectType::UNKNOWN) {
                eprintf("Line %d col %d: Unknown char '%c'\n", i + 1, idx - idxBase + 1, c);
                return false;
            }

            // refuse to process unimplemented features
            if (tp > ObjectType::_UNIMPLEMENTED) {
                eprintf("Line %d col %d: Unimplemented element '%c'\n", i + 1, idx - idxBase + 1, c);
                return false;
            }

            // static blocks are mapped to board
            if (tp == ObjectType::WALL ||
                tp == ObjectType::FIRE ||
                tp == ObjectType::RECYCLER) {
                board.map[idx] = tp;
            }

            switch (tp) {
            case ObjectType::MAGICIAN:
                state_root.magicianPos = idx;
                break;
            case ObjectType::FIRE:
                board.fires.push_back(idx);
                break;
            case ObjectType::ICE:
                board.iceType.push_back(0);
                state_init.icePositions.push_back(idx);
                break;
            case ObjectType::ICE_GOLD:
                board.iceType.push_back(1);
                state_init.icePositions.push_back(idx);
                break;
            default: break;
            }

            idx++;
        }
    }

    return true;
}

int main() {
    BoardConfigration board {};
    InitialState state_init {};
    State state_root {.initial = &state_init};

    bool success = readFloorFileFromStdin(board, state_init, state_root);

    if (!success) {
        eprintf("Error loading from <stdin>.\n");
        return 1;
    }

    board.preprocess();
    // TODO: normalize magician position

    printConfiguration(board, state_root);
}
