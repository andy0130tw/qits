#include <cstdio>
#include <vector>
#include <queue>
#include <bitset>
#include <unordered_set>
#include <algorithm>
#include "qits.h"
#include "board_view.h"

PatternDatabase State::patdb{};

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
        buf[i] = board.iceType[i] ? '$' : '%';
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

BoardChange pushIceBlock(const BoardView& bview, const State& s, int pos, Direction d) {
    int bidx = bview.iceToIndex[pos];
    bool isGoldIce = (bview.config.iceType[bidx] == 1);
    BoardChange changes{s};
    State& newState = changes.state;

    newState.previous = const_cast<State*>(&s);
    newState.age = s.age + 1;
    newState.movedIceIndex = bidx;
    newState.oldPosition = static_cast<short int>(pos);

    PatType npat = s.getClearedFires();

    short int npos = newState.oldPosition, peek;

    while (peek = bview.next[npos][static_cast<int>(d)], peek > 0) {
        if (bview.isWall(peek) || bview.iceToIndex[peek] >= 0) {
            break;
        }

        npos = peek;

        if (bview.config.map[peek] == ObjectType::RECYCLER) {
            npos = -1;
            break;
        } else if (bview.isMarked(peek)) {
            if (bview.fireToIndex[peek] >= 0) {
                // encounter a FIRE
                changes.posClearedFires.push_back(peek);
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

    return changes;
}

// check for reachability & normalize magician position
// are encoded as (idx << 8) + direction
vector<int> exploreBoard(BoardView& bview) {
    // BFS to explore reachable positions of magician
    vector<int> pushables;
    auto& vis = bview.vis;
    bview.tick();

    // re-allocating is slow
    pushables.reserve(128);

    unsigned int normalizedPosition = bview.magicianPos;
    queue<int> q;

    vis[bview.magicianPos] = bview.ts;
    q.push(bview.magicianPos);

    while (!q.empty()) {
        int s = q.front();
        q.pop();

        if (static_cast<unsigned int>(s) < normalizedPosition) {
            normalizedPosition = s;
        }

        int t;
        // left
        if (t = s-1, s % MAP_W != 0) {
            if (bview.canPushTo(t, Direction::LEFT)) {
                pushables.push_back((t << 8) | static_cast<int>(Direction::LEFT));
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // right
        if (t = s+1, t % MAP_W != 0) {
            if (bview.canPushTo(t, Direction::RIGHT)) {
                pushables.push_back((t << 8) | static_cast<int>(Direction::RIGHT));
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // up
        if (t = s-MAP_W, t >= 0) {
            if (bview.canPushTo(t, Direction::UP)) {
                pushables.push_back((t << 8) | static_cast<int>(Direction::UP));
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
        // down
        if (t = s+MAP_W, t < MAP_SIZE) {
            if (bview.canPushTo(t, Direction::DOWN)) {
                pushables.push_back((t << 8) | static_cast<int>(Direction::DOWN));
            } else if (bview.isFresh(t)) {
                vis[t] = bview.ts;
                q.push(t);
            }
        }
    }

    bview.setMagicianPos(normalizedPosition);

    return pushables;
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

static vector<int> pushablesCache[256];
static unordered_set<uint64_t> stateHashTable;
static vector<BoardChange> solution;

bool dfs(BoardView& bview, const State& s, unsigned int depth, unsigned int depthLimit) {
    if (depth == depthLimit) {
        return false;
    }

    if (s.clearedFiresPatId == 1) {
        return true;
    }

    auto& pushables = pushablesCache[depth];
    auto len = pushables.size();

    struct {
        int idx;
        Direction dir;
        BoardChange change;
    } changeList[len];

    for (size_t i = 0; i < len; i++) {
        int idx = pushables[i] >> 8;
        Direction dir = static_cast<Direction>(pushables[i] & 0xff);
        changeList[i] = { idx, dir, pushIceBlock(bview, s, idx, dir) };
    }

    // prioritize a move that clears more fire
    sort(changeList, changeList + len, [](auto& a, auto& b) -> bool {
        return a.change.posClearedFires.size() > b.change.posClearedFires.size();
    });

    for (size_t i = 0; i < pushables.size(); i++) {
        auto& change = changeList[i].change;

        uint64_t hashBefore = bview.hash;
        unsigned int magicianPosOld = bview.magicianPos;

        bview.apply(change);
        // bview.print();
        pushablesCache[depth+1] = exploreBoard(bview);

        bool solved = false;
        if (stateHashTable.find(bview.hash) == stateHashTable.end()) {
            stateHashTable.insert(bview.hash);
            solved = dfs(bview, change.state, depth + 1, depthLimit);
        }

        bview.unapply(change);
        bview.setMagicianPos(magicianPosOld);

        // only verifying
        exploreBoard(bview);
        // bview.print();
        if (bview.hash != hashBefore) {
            printf("Hash mismatch!!\n");
            return false;
        }

        if (solved) {
            solution.push_back(move(change));
            return true;
        }
    }

    return false;
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
    bview.updateHash(bview.magicianPos, ObjectType::MAGICIAN);

    PatType completedPat{};

    for (size_t i = 0; i < board.fires.size(); i++) {
        completedPat[i] = 1;
    }

    if (State::patdb.queryByPat(completedPat) != 1) {
        printf("Owo no!\n");
    }

    pushablesCache[0] = exploreBoard(bview);
    stateHashTable.insert(bview.hash);

    bool solved = false;

    for (int lim = 0; lim < 20; lim++) {
        printf("Trying %d steps...\n", lim);
        stateHashTable.clear();

        solution.reserve(lim);
        bool s = dfs(bview, state_root, 0, lim);

        printf("Explored %zd states\n", stateHashTable.size());
        printf("Patterns generated = %zd\n", State::patdb.size());

        if (s) {
            printf("====== SOLVED! ======\n");

            reverse(solution.begin(), solution.end());

            for (auto& step: solution) {
                exploreBoard(bview);
                bview.print();
                printf("STEP -->  ");
                step.state.print();
                bview.apply(step);
            }
            exploreBoard(bview);
            bview.print();
            printf("====== END OF SOLUTION ======\n");
            solved = true;
            break;
        }
    }

    if (!solved) {
        printf("No solution.\n");
    }
}
