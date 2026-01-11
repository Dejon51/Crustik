#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"
#include <stdint.h>
#include <stdbool.h>

#define ILLEGALMOVE 42

#define WHITE -1;

typedef struct {
    uint16_t moves[218];
} LegalMoves;

static uint8_t penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

MoveList* legalMoveGen(const Position *board,MoveList *list, bool turn);

void makeMove(Position *board,MoveList *list, int move, bool turn);


#endif
