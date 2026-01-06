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

Bitboard pawnMask(BitboardSet *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

Bitboard horseMask(BitboardSet *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

Bitboard bishopMask(BitboardSet *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

Bitboard rookMask(Bitboard *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

Bitboard queenMask(Bitboard *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

Bitboard kingMask(Bitboard *board, Bitboard *whitebitboard, Bitboard *blackbitboard, bool color);

void legalMoveGen(BitboardSet *board, int move, bool turn);

void makeMove(BitboardSet *board, int move, bool turn);

#endif
