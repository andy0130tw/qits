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

    unsigned int age;
    int magicianPos;

    /* only meaningful when age > 0 */
    uint64_t hash;
    int movedIceIndex;
    short int oldPosition;
    short int newPosition;

    bitset<MAX_FIRE> clearedFires;
};

struct BoardView {
    char iceToIndex[MAP_SIZE];
    char fireToIndex[MAP_SIZE];
    unsigned int vis[MAP_SIZE];
    unsigned int ts;
    unsigned int magicianPos;
    static const unsigned int TS_MAX = 1 << 28;
    static const unsigned int OBSTACLE = -1024;
    static int next[MAP_SIZE][static_cast<int>(Direction::_ALL)];
    static bool nextInited;

    BoardView(): vis(), ts(0) {
        if (!nextInited) {
            initNextTable();
        }

        for (int i = 0; i < MAP_SIZE; i++) {
            iceToIndex[i] = -1;
            fireToIndex[i] = -1;
        }
    }

    inline bool isObstacle(int pos) {
        return vis[pos] == BoardView::OBSTACLE;
    }

    void tick() {
        if (ts < BoardView::TS_MAX) {
            ts++;
        } else {
            for (int i = 0; i < MAP_SIZE; i++) {
                if (vis[i] < TS_MAX) {
                    vis[i] = 0;
                }
            }
            ts = 1;
        }
    }

    int canPushTo(int pos, Direction d) {
        if (!(isObstacle(pos) && iceToIndex[pos] >= 0)) {
            return -1;
        }

        int pnext = pos, peek;
        while (peek = next[pnext][static_cast<int>(d)], peek > 0) {
            if (isObstacle(peek) && fireToIndex[peek] < 0) {
                break;
            }
            pnext = peek;
        }

        if (pnext == pos) {
            return -1;
        }
        return pnext;
    }

    void initNextTable() {
        for (int i = 0; i < MAP_H; i++) {
            for (int j = 0; j < MAP_W; j++) {
                int p = i * MAP_W + j;
                next[p][static_cast<int>(Direction::UP)]    = (i != 0         ? p-MAP_W : -1);
                next[p][static_cast<int>(Direction::DOWN)]  = (i != (MAP_H-1) ? p+MAP_W : -1);
                next[p][static_cast<int>(Direction::LEFT)]  = (j != 0         ? p-1     : -1);
                next[p][static_cast<int>(Direction::RIGHT)] = (j != (MAP_W-1) ? p+1     : -1);
            }
        }
        nextInited = true;
    }
};

int BoardView::next[][static_cast<int>(Direction::_ALL)];
bool BoardView::nextInited;

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

void printConfiguration(BoardConfiguration& board, State& state) {
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

// check for reachability & normalize magician position
void exploreBoard(const BoardConfiguration& board, BoardView& bview) {
    // BFS to explore reachable positions of magician
    vector<vector<int>> pushables;
    pushables.resize(board.iceType.size());
    auto& vis = bview.vis;
    bview.tick();

    int normalizedPosition = bview.magicianPos;
    queue<int> q;
    vis[bview.magicianPos] = bview.ts;
    q.push(bview.magicianPos);


    for (auto& x: pushables) {
        for (int i = 0; i < static_cast<int>(Direction::_ALL); i++) {
            x.push_back(-1);
        }
    }

    while (!q.empty()) {
        int s = q.front();
        q.pop();

        if (board.map[s] == ObjectType::WALL) {
            vis[s] = BoardView::OBSTACLE;
            continue;
        }

        if (s < normalizedPosition) {
            normalizedPosition = s;
        }

        int t, dest;
        // left
        if (t = s-1, s % MAP_W != 0) {
            if (dest = bview.canPushTo(t, Direction::LEFT), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::LEFT] = dest;
            } else if (vis[t] < bview.ts) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // right
        if (t = s+1, t % MAP_W != 0) {
            if (dest = bview.canPushTo(t, Direction::RIGHT), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::RIGHT] = dest;
            } else if (vis[t] < bview.ts) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // up
        if (t = s-MAP_W, t >= 0) {
            if (dest = bview.canPushTo(t, Direction::UP), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::UP] = dest;
            } else if (vis[t] < bview.ts) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // down
        if (t = s+MAP_W, t < MAP_SIZE) {
            if (dest = bview.canPushTo(t, Direction::DOWN), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::DOWN] = dest;
            } else if (vis[t] < bview.ts) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
    }

    printf("Normalized pos: %d\n", normalizedPosition);
    bview.magicianPos = normalizedPosition;

    for (int i = 0; i < MAP_H; i++) {
        for (int j = 0; j < MAP_W; j++) {
            unsigned int val = vis[i * MAP_W + j];
            if (bview.iceToIndex[i * MAP_W + j] >= 0) {
                printf("%2d ", bview.iceToIndex[i * MAP_W + j]);
                if (val != BoardView::OBSTACLE) {
                    printf("[[reserve not set]]");
                }
            } else if (val == BoardView::OBSTACLE) {
                printf(" X ");
            } else if (val == bview.ts) {
                printf(" . ");
            } else {
                printf(" _ ");
            }
        }
        printf("\n");
    }

    printf("Pushables ---\n");
    for (size_t i = 0; i < pushables.size(); i++) {
        bool any = false;
        for (int j = 0; j < 4; j++) {
            if (pushables[i][j] >= 0) {
                any = true;
                break;
            }
        }
        if (!any) continue;

        printf("%3zd: ", i);
        for (int j = 0; j < 4; j++) {
            if (pushables[i][j] >= 0) {
                printf("%c: %3d ", "^v<>"[j], pushables[i][j]);
            } else {
                printf("       ");
            }
        }
        printf("\n");
    }
}

bool readFloorFileFromStdin(BoardConfiguration& board, InitialState& state_init, State& state_root) {
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

BoardView initBoardView(const BoardConfiguration& board, const InitialState& state_init) {
    BoardView bview{};

    for (size_t i = 0; i < state_init.icePositions.size(); i++) {
        int p = state_init.icePositions[i];
        bview.iceToIndex[p] = i;
        bview.vis[p] = BoardView::OBSTACLE;
    }

    for (size_t i = 0; i < board.fires.size(); i++) {
        int p = board.fires[i];
        bview.fireToIndex[p] = i;
        bview.vis[p] = BoardView::OBSTACLE;
    }

    return bview;
}

int main() {
    BoardConfiguration board {};
    InitialState state_init {};
    State state_root {.initial = &state_init};

    bool success = readFloorFileFromStdin(board, state_init, state_root);

    if (!success) {
        eprintf("Error loading from <stdin>.\n");
        return 1;
    }

    // TODO: normalize magician position

    printConfiguration(board, state_root);

    BoardView bview = initBoardView(board, state_init);
    bview.magicianPos = state_root.magicianPos;

    exploreBoard(board, bview);

    // for (int i = 0; i < MAP_H; i++) {
    //     for (int j = 0; j < MAP_W; j++) {
    //         printf("%3d ", board.moveDest[2][i * MAP_W + j]);
    //     }
    //     printf("\n");
    // }

    // int p = moveToward(board, state_root, 0, Direction::RIGHT);
    // printf("%d\n", p);
}
