#ifndef EVAL_H
#define EVAL_H

#include "lmath.h"
#include <stdint.h>

#define NNUE_MAX_PLY 256   // headroom above MAX_SEARCH_PLY (qsearch can run deeper)

void init_tables(void);

// Evaluate the position using the accumulator already stored at `ply`.
int eval(Position *board, int ply);

// Full rebuild from scratch into slot `ply`. Call once, at the root,
// whenever the board is freshly set up (new game / new FEN / new search).
void nnue_refresh(Position *board, int ply);

// Incremental update: reads board's PRE-MOVE state (mailbox, epsquare,
// turn) to figure out what changed, and writes the resulting accumulator
// into `childPly`, based on `parentPly`. Call this BEFORE makeMove().
void nnue_update(Position *board, uint16_t move, int parentPly, int childPly);

// No piece changes (null move) — just carries the accumulator forward.
void nnue_copy(int parentPly, int childPly);

#endif