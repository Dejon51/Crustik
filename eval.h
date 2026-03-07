#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"

static uint8_t penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

static const int bitVal[6] = {100, 300, 300, 500, 900, 20000};

typedef struct 
{
    Bitboard color[2];
} ThreatBoard;


int assessSquare(int ind, Position *board);

int eval(Position *board);

#endif
