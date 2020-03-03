#include <cstdio>
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

void BoardView::tick() {
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

void BoardView::print() {
    printf("   +");
    for (int j = 0; j < MAP_W; j++) {
        printf("-%02d-", j);
    }
    printf("\n");

    for (int i = 0; i < MAP_H; i++) {
        printf("%3d|", i * 20);
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

int BoardView::canPushTo(int pos, Direction d) {
    // only an ice block can be pushed
    if (iceToIndex[pos] < 0) {
        return -1;
    }

    int peek = next[pos][static_cast<int>(d)];
    if (peek > 0 && !isWall(peek) && iceToIndex[peek] < 0) {
        return peek;
    }
    return -1;
}

void BoardView::updateHash(int pos, ObjectType t) {
    const uint64_t* blk;
    switch (t) {
    case ObjectType::ICE:      blk = ZOBRIST_VALUES[0]; break;
    case ObjectType::FIRE:     blk = ZOBRIST_VALUES[1]; break;
    case ObjectType::ICE_GOLD: blk = ZOBRIST_VALUES[2]; break;
    case ObjectType::MAGICIAN: blk = ZOBRIST_VALUES[3]; break;
    default:
        // fprintf(stderr, "hash upd tp=%zd pos=%d\n",
        //         (blk - ZOBRIST_VALUES[0]) / MAP_SIZE,
        //         pos);
        __builtin_unreachable();
    }

    hash ^= blk[pos];
}
