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

Bitboard pawnMask(Position *board, bool color);

Bitboard horseMask(Position *board, bool color);

Bitboard bishopMask(Position *board, bool color);

Bitboard rookMask(Position *board, bool color);

Bitboard kingMask(Position *board, bool color);

bool squareAttacked(Position *b, int sq, int enemy);

void legalMoveGen(Position *board, MoveList *list);

void makeMove(Position *board,MoveList *list, int move);

void moveint(Position *board, uint16_t move);

void captureMoves(Position *board, MoveList *list, bool color);

uint64_t perft(Position *board, int depth);


#endif
