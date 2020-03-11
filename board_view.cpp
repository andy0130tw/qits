#include <cstdio>
#include <cassert>
#include "board_view.h"

int BoardView::next[][static_cast<int>(Direction::_ALL)];
bool BoardView::nextInited;

static inline const uint64_t* getZobristValues(ObjectType t) {
    switch (t) {
    case ObjectType::ICE:      return ZOBRIST_VALUES[0];
    case ObjectType::FIRE:     return ZOBRIST_VALUES[1];
    case ObjectType::ICE_GOLD: return ZOBRIST_VALUES[2];
    case ObjectType::MAGICIAN: return ZOBRIST_VALUES[3];
    default:
        __builtin_unreachable();
        return nullptr;
    }
}

BoardView::BoardView(const BoardConfiguration& config):
    config(config), vis(), ts(0), hash(0) {
    if (!nextInited) {
        initNextTable();
    }

    for (int i = 0; i < MAP_SIZE; i++) {
        iceToIndex[i] = -1;
        fireToIndex[i] = -1;
    }
}

void BoardView::print() {
    printf("   +");
    for (int j = 0; j < MAP_W; j++) {
        printf("-%02d-", j);
    }
    printf("\n");

    for (int i = 0; i < MAP_H; i++) {
        printf("%3d|", i * MAP_W);
        for (int j = 0; j < MAP_W; j++) {
            unsigned int p = i * MAP_W + j;
            unsigned int val = vis[p];
            if (iceToIndex[p] >= 0) {
                printf("%c%2d ", config.iceType[iceToIndex[p]] ? '$' : '%', iceToIndex[p]);
                // note that the underlying cell may have a non-EMPTY ObjectType
                // e.g. recycler
            } else if (val == BoardView::WALL) {
                printf("  X ");
            } else if (val == BoardView::MARKED) {
                if (fireToIndex[p] >= 0) {
                    printf("*%2d ", fireToIndex[p]);
                } else {
                    printf("  ! ");
                }
            } else if (val == ts) {
                // marked as reachable
                if (p == magicianPos) {
                    printf(" &. ");
                } else {
                    printf("  . ");
                }
            } else {
                // unexplored empty area
                printf("  _ ");
            }
        }
        printf("\n");
    }

    printf("Magician position: %d\n", magicianPos);
}

void BoardView::updateHash(int pos, ObjectType t) {
    // fprintf(stderr, "hash upd tp=%zd pos=%d\n",
    //         (getZobristValues(t) - ZOBRIST_VALUES[0]) / MAP_SIZE,
    //         pos);
    hash ^= getZobristValues(t)[pos];
}

bool BoardView::verifyHash() const {
    uint64_t test = 0;
    for (int i = 0; i < MAP_SIZE; i++) {
        if (iceToIndex[i] >= 0) {
            ObjectType tp = config.getIceTypeAtIndex(iceToIndex[i]);
            test ^= getZobristValues(tp)[i];
        } else if (isMarked(i) && fireToIndex[i] >= 0) {
            test ^= getZobristValues(ObjectType::FIRE)[i];
        }
    }

    test ^= getZobristValues(ObjectType::MAGICIAN)[magicianPos];

    return hash == test;
}

void BoardView::moveIceBlock(int idx, int from, int to) {
    // TODO: check if this application is legitimate
    assert(from != to);

    ObjectType iceType = config.getIceTypeAtIndex(idx);

    if (from >= 0) {
        iceToIndex[from] = -1;
        updateHash(from, iceType);
    }
    // for compatibility of unapply

    if (to >= 0) {
        iceToIndex[to] = idx;
        updateHash(to, iceType);
    }
    // else it is elimiated from map, do nothing
}

void BoardView::apply(const BoardChange& change) {
    auto& s = change.state;
    assert(s.oldPosition >= 0 &&
           iceToIndex[s.oldPosition] >= 0);

    moveIceBlock(s.movedIceIndex, s.oldPosition, s.newPosition);

    for (auto fpos: change.posClearedFires) {
        // TODO: check if the cell is fire
        updateHash(fpos, ObjectType::FIRE);
        vis[fpos] = 0;
    }
}

void BoardView::unapply(const BoardChange& change) {
    // TODO: revert to previous state;
    // should find an alternative, "static allocated" way to
    // effectively iterate over fire positions

    auto& s = change.state;
    assert(s.oldPosition >= 0 &&
           iceToIndex[s.oldPosition] < 0);

    // FIXME: guard (s.newPosition < 0 || iceToIndex[s.newPosition] >= 0)
    // TODO: check consistency with parent state
    moveIceBlock(s.movedIceIndex, s.newPosition, s.oldPosition);

    for (auto fpos: change.posClearedFires) {
        // TODO: check if the cell is fire
        updateHash(fpos, ObjectType::FIRE);
        vis[fpos] = MARKED;
    }
}

void BoardView::transit(const State& _s1, const State& _s2) {
    // TODO: find x = LCA(s1, s2), and restore ice blocks with
    // s1 -> x -> s2
    const State* s1 = &_s1, * s2 = &_s2;

    vector<const State*> backwardStates;
    vector<const State*> forwardStates;

    if (s1->age > s2->age) {
        while (s1->age != s2->age) {
            backwardStates.push_back(s1);
            s1 = s1->previous;
        }
    } else {
        while (s1->age != s2->age) {
            forwardStates.push_back(s2);
            s2 = s2->previous;
        }
    }

    while (s1 != s2) {
        backwardStates.push_back(s1);
        forwardStates.push_back(s2);
        s1 = s1->previous;
        s2 = s2->previous;
    }

    // s1 -> x
    for (auto s: backwardStates) {
        moveIceBlock(s->movedIceIndex, s->newPosition, s->oldPosition);
    }
    // x -> s2
    for (auto s: forwardStates) {
        moveIceBlock(s->movedIceIndex, s->oldPosition, s->newPosition);
    }

    PatType shouldBeCleared = _s2.getClearedFires();

    // slow operation due to not storing concrete changes on fires
    for (size_t i = 0; i < config.fires.size(); i++) {
        unsigned int fpos = config.fires[i];
        if (shouldBeCleared[i] && vis[fpos] == MARKED) {
            updateHash(fpos, ObjectType::FIRE);
            vis[fpos] = 0;
        } else if (!shouldBeCleared[i] && vis[fpos] != MARKED) {
            updateHash(fpos, ObjectType::FIRE);
            vis[fpos] = MARKED;
        }
    }
}

