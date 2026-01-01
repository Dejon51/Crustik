#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"

static const int bitVal[6] = {100, 300, 300, 500, 900, 20000};

typedef struct 
{
    Bitboard color[2];
} ThreatBoard;


int assessSquare(int ind, BitboardSet *board);

short eval(BitboardSet board);

#endif
