#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"
#include <stdint.h>

#define NNUE_MAX_PLY 256

void init_tables(void);

int eval(Position *board, int ply);

void nnue_refresh(Position *board, int ply);

void nnue_update(Position *board, uint16_t move, int parentPly, int childPly);

void nnue_copy(int parentPly, int childPly);

#endif