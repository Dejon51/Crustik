#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"
#include "moves.h"

typedef struct {
    int x;
    int y;
} PieceLocation;

PieceLocation locatePiece(char piece, char id, char board[8][8]);
void makeMove(char board[8][8]);

#endif
