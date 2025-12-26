#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"

typedef struct
{
    char data[27];
} MoveArray;

typedef struct
{
    char data[8];
} HorseMoves;

typedef struct
{
    char data[8];
} BishopMoves;

static unsigned char penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

typedef struct
{
    char data[3]; // [0] = left, [1] = forward, [2] = right
} PawnMoves;

PawnMoves pawn(BitboardSet board,char color,char ind);
HorseMoves Horse(BitboardSet board ,char ind);
BishopMoves Bishop(BitboardSet board,char ind);
void makeMove(BitboardSet board,char turn);


#endif
