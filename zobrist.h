#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "lmath.h"

extern uint64_t zobrist_table[768];

uint64_t zobrist(Position *board);

#endif