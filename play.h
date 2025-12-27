#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t data[56];
} MoveArray;

typedef struct
{
    uint8_t data[8];
} HorseMoves;

typedef struct
{
    uint8_t data[28];
} BishopMoves;

typedef struct
{
    uint8_t data[28];
} RookMoves;

typedef struct
{
    uint8_t data[56];
} QueenMoves;

typedef struct
{
    uint8_t data[8];
} KingMoves;

static uint8_t penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

typedef struct
{
   uint8_t data[3]; // [0] = left, [1] = forward, [2] = right
} PawnMoves;

PawnMoves pawn(BitboardSet board,char color,int ind);
HorseMoves Horse(BitboardSet board, int ind);
BishopMoves Bishop(BitboardSet board,int ind);
void makeMove(BitboardSet board,bool turn);


#endif
