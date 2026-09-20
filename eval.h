#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"
#include <stdint.h>

#define NNUE_MAX_PLY 256

void init_tables(void);

int eval(Position *board);
#endif