#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"
#include <stdint.h>

#define NNUE_MAX_PLY 256
#define NNUE_HL 512

#define NNUE_ALIGN64 __attribute__((aligned(64)))

typedef struct {
	int16_t vector[2][NNUE_HL] NNUE_ALIGN64;
	uint8_t mirror[2];
	uint8_t needsRefresh[2];
} NnueAccumulator;

typedef struct ThreadData ThreadData;

void init_tables(void);

int eval(ThreadData *td, Position *board, int ply);

void nnue_refresh(ThreadData *td, Position *board, int ply);

void nnue_update(ThreadData *td, Position *board, uint16_t move, int parentPly, int childPly);

void nnue_copy(ThreadData *td, int parentPly, int childPly);

#endif
