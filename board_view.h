#ifndef __QITS_BOARD_VIEW_H
#define __QITS_BOARD_VIEW_H

#include "qits.h"

struct BoardView {
    const BoardConfiguration& config;
    char iceToIndex[MAP_SIZE];
    char fireToIndex[MAP_SIZE];
    unsigned int vis[MAP_SIZE];
    unsigned int ts;
    unsigned int magicianPos;
    uint64_t hash;

    static const unsigned int TS_MAX = 1 << 28;
    static const unsigned int WALL = -1024;
    static const unsigned int MARKED = -1023;
    static int next[MAP_SIZE][static_cast<int>(Direction::_ALL)];
    static bool nextInited;

    BoardView(const BoardConfiguration& config);

    inline bool isWall(int pos) { return vis[pos] == WALL; }
    inline bool isMarked(int pos) { return vis[pos] == MARKED; }
    inline bool isFresh(int pos) { return (iceToIndex[pos] < 0 && vis[pos] < ts); }

    inline void tick() {
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

    void print();
    inline bool canPushTo(int pos, Direction d) {
        // only an ice block can be pushed
        if (iceToIndex[pos] < 0) {
            return false;
        }

        int peek = next[pos][static_cast<int>(d)];
        if (peek > 0 && !isWall(peek) && iceToIndex[peek] < 0) {
            return true;
        }
        return false;
    }
    void updateHash(int pos, ObjectType t);
    void setMagicianPos(unsigned int npos);

    void apply(const BoardChange& change);
    void unapply(const BoardChange& change);
    void transit(const State& s1, const State& s2);

    static void initNextTable() {
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

    void moveIceBlock(int idx, int from, int to);
};

#endif  // __QITS_BOARD_VIEW_H
