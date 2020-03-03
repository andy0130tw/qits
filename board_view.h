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

    inline bool isWall(int pos) { return vis[pos] == BoardView::WALL; }
    inline bool isMarked(int pos) { return vis[pos] == BoardView::MARKED; }
    inline bool isFresh(int pos) { return (iceToIndex[pos] < 0 && vis[pos] < ts); }

    void tick();
    void print();
    int canPushTo(int pos, Direction d);
    void updateHash(int pos, ObjectType t);

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
};

#endif  // __QITS_BOARD_VIEW_H