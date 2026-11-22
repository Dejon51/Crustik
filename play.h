#ifndef PLAY_H
#define PLAY_H

#include "lmath.h"
#include <stdint.h>
#include <stdbool.h>

#define ILLEGALMOVE 42


typedef struct {
    uint16_t moves[218];
} LegalMoves;

void init_between_bb(void);

void init_castling_mask(void);

void init_pseudo_attacks(void);

Bitboard pawnMask(Position *board, bool color);

Bitboard horseMask(Position *board, bool color);

Bitboard bishopMask(Position *board, bool color);

Bitboard rookMask(Position *board, bool color);

Bitboard kingMask(Position *board, bool color);

bool squareAttacked(Position *b, int sq, int enemy);

void legalMoveGen(Position *board, MoveList *list);

void makeMove(Position *board,MoveList *list, int move);

void moveint(Position *board, uint16_t move);

uint64_t perft(Position *board, int depth, int divide);
uint64_t perftbulk(Position *board, int depth);
uint64_t perftpopcnt(Position *board, int depth);
#endif