#ifndef MOVES_H
#define MOVES_H


static unsigned char penaltymap[64] = {1,2,3,4,4,3,2,1,2,3,4,5,5,4,3,2,3,4,5,6,6,5,4,3,4,5,6,7,7,6,5,4,4,5,6,7,7,6,5,4,3,4,5,6,6,5,4,3,2,3,4,5,5,4,3,2,1,2,3,4,4,3,2,1};

typedef struct {
    char data[5];   // [0] = left, [1] = forward, [2] = right, [3] = total, [4] = Pawn Status
} PawnMoves;

typedef struct {    // for (char i = 0; i < result; i++)
    char data[64];
} PieceArray;

typedef struct { 
    char data[27];
} MoveArray;


#include <stdio.h>
#include "lmath.h"

// Out bounds


int assessSquare(char color1,char x, char y, char board[8][8]);

PawnMoves pawn(char color,char detail, char x, char y, char board[8][8]);

PieceArray avalibleMoves(char piece,char board[8][8]);

#endif