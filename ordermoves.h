#ifndef ORDERMOVES_H
#define ORDERMOVES_H

#include <stdint.h>
#include "play.h"

// Old one-shot ordering still used by quiesce() and ProbCut
MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move);

typedef struct {
	MoveList list;
	int scores[256];
	unsigned num_noisy;
	unsigned idx;
	unsigned end;
	int stage;
	int ply;
	uint16_t tt_move;
	Position *board;
} MovePicker;

void movepicker_init(MovePicker *mp, Position *board, int ply, uint16_t tt_move);
uint16_t movepicker_next(MovePicker *mp);

#endif