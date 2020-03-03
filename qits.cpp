#include <cstdio>
#include <vector>
#include <queue>
#include <bitset>
#include <unordered_map>
#include <map>
#include <algorithm>
#include "qits.h"
#include "board_view.h"

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

    static PatternDatabase& patdb;

    PatType getClearedFires() const {
        auto kk = State::patdb.queryById(clearedFiresPatId);
        return kk;
    }

    unsigned int setClearedFiresPat(PatType pat) {
        unsigned int id = State::patdb.queryByPat(pat);
        return (clearedFiresPatId = id);
    }

    void print() {
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

static PatternDatabase PATTERN_DB_GLOBAL {};

PatternDatabase& State::patdb = PATTERN_DB_GLOBAL;

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

InitialState* findInitialState(const State& state) {
    const State* s = &state;
    while (s->age != 0) {
        s = s->previous;
    }
    return s->initial;
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

State pushIceBlock(BoardView& bview, const State& s, int pos, Direction d) {
    int bidx = bview.iceToIndex[pos];
    bool isGoldIce = (bview.config.iceType[bidx] == 1);
    State newState{s};

    newState.previous = const_cast<State*>(&s);
    newState.age = s.age + 1;
    newState.movedIceIndex = bidx;
    newState.oldPosition = static_cast<short int>(pos);

    PatType npat = s.getClearedFires();

    short int npos = newState.oldPosition, peek;

    while (peek = bview.next[npos][static_cast<int>(d)], peek > 0) {
        if (bview.isWall(peek)) {
            break;
        }

        npos = peek;

        if (bview.config.map[peek] == ObjectType::RECYCLER) {
            npos = -1;
            break;
        } else if (bview.isMarked(peek)) {
            if (bview.fireToIndex[peek] >= 0) {
                // encounter a FIRE
                npat[bview.fireToIndex[peek]] = 1;
                if (!isGoldIce) {
                    npos = -1;
                    break;
                }
            }
        }
    }

    newState.newPosition = npos;
    newState.setClearedFiresPat(npat);
    return newState;
}

// check for reachability & normalize magician position
void exploreBoard(BoardView& bview) {
    // BFS to explore reachable positions of magician
    vector<vector<int>> pushables;
    pushables.resize(bview.config.iceType.size());
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

        if (s < normalizedPosition) {
            normalizedPosition = s;
        }

        int t, dest;
        // left
        if (t = s-1, s % MAP_W != 0) {
            if (dest = bview.canPushTo(t, Direction::LEFT), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::LEFT] = dest;
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // right
        if (t = s+1, t % MAP_W != 0) {
            if (dest = bview.canPushTo(t, Direction::RIGHT), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::RIGHT] = dest;
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // up
        if (t = s-MAP_W, t >= 0) {
            if (dest = bview.canPushTo(t, Direction::UP), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::UP] = dest;
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // down
        if (t = s+MAP_W, t < MAP_SIZE) {
            if (dest = bview.canPushTo(t, Direction::DOWN), dest >= 0) {
                pushables[bview.iceToIndex[t]][(int)Direction::DOWN] = dest;
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
    }

    bview.magicianPos = normalizedPosition;

    bview.print();

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
    BoardView bview(board);

    for (int p = 0; p < MAP_SIZE; p++) {
        if (board.map[p] == ObjectType::RECYCLER) {
            bview.vis[p] = BoardView::MARKED;
        } else if (board.map[p] == ObjectType::WALL) {
            bview.vis[p] = BoardView::WALL;
        }
    }

    for (size_t i = 0; i < state_init.icePositions.size(); i++) {
        int p = state_init.icePositions[i];
        bview.iceToIndex[p] = i;
        bview.updateHash(p, ObjectType::ICE);
    }

    for (size_t i = 0; i < board.fires.size(); i++) {
        int p = board.fires[i];
        bview.fireToIndex[p] = i;
        bview.vis[p] = BoardView::MARKED;
        bview.updateHash(p, ObjectType::FIRE);
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

    exploreBoard(bview);

    // State s2 = pushIceBlock(bview, state_root, 233, Direction::UP);
    // s2.print();
}