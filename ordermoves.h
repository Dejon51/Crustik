#ifndef ORDERMOVES_H
#define ORDERMOVES_H

#include <stdint.h>
#include <stdbool.h>
#include "play.h"

MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move);

typedef enum
{
	ST_TT,
	ST_GEN_CAPS,
	ST_GOOD_CAPS,
	ST_GEN_QUIETS,
	ST_QUIETS,
	ST_BAD_CAPS,
	ST_GEN_EVASIONS,
	ST_EVASIONS,
	ST_DONE
} PickerStage;

typedef struct
{
	Position *board;
	int ply;
	int stage;
	bool in_check;
	bool skip_quiets;
	uint16_t tt_move;

	MoveList caps;
	MoveList quiets;
	int cap_scores[256];
	int quiet_scores[256];
	int cap_i;
	int quiet_i;

	uint16_t bad[64];
	int n_bad;
	int bad_i;
} MovePicker;

void movepicker_init(MovePicker *mp, Position *board, int ply, uint16_t tt_move, bool in_check);
uint16_t movepicker_next(MovePicker *mp);
bool move_is_pseudolegal(Position *b, uint16_t m);

#endif