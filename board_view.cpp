#include <cstdio>
#include <cassert>
#include "board_view.h"

int BoardView::next[][static_cast<int>(Direction::_ALL)];
bool BoardView::nextInited;

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

    printf("Normalized pos: %d\n", magicianPos);
}

void BoardView::updateHash(int pos, ObjectType t) {
    const uint64_t* blk;
    switch (t) {
    case ObjectType::ICE:      blk = ZOBRIST_VALUES[0]; break;
    case ObjectType::FIRE:     blk = ZOBRIST_VALUES[1]; break;
    case ObjectType::ICE_GOLD: blk = ZOBRIST_VALUES[2]; break;
    case ObjectType::MAGICIAN: blk = ZOBRIST_VALUES[3]; break;
    default:
        __builtin_unreachable();
    }


    // fprintf(stderr, "hash upd tp=%zd pos=%d\n",
    //         (blk - ZOBRIST_VALUES[0]) / MAP_SIZE,
    //         pos);

    hash ^= blk[pos];
}

void BoardView::setMagicianPos(unsigned int npos) {
    if (magicianPos != npos) {
        updateHash(magicianPos, ObjectType::MAGICIAN);
        updateHash(npos, ObjectType::MAGICIAN);
        magicianPos = npos;
    }
}

void BoardView::moveIceBlock(int idx, int from, int to) {
    // TODO: check if this application is legitimate
    assert(from != to);

    auto iceType = (config.iceType[idx] == 1 ? ObjectType::ICE_GOLD : ObjectType::ICE);

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

void transit(const State& s1, const State& s2) {
    // TODO: find x = LCA(s1, s2), and restore ice blocks with
    // s1 -> x -> s2

    // slow operation due to not storing concrete changes on fires
}
