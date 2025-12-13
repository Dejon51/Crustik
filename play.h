#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"

typedef struct
{
    char data[27];
} MoveArray;

static unsigned char penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

typedef struct
{
    char data[4]; // [0] = left, [1] = forward, [2] = right, [3] = total
} PawnMoves;

PawnMoves pawn(BitboardSet board,char ind);
MoveArray Horse(BitboardSet board ,char ind);

void makeMove(BitboardSet board,char turn);


#endif
