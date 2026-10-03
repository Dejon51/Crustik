#include <stdbool.h>
#include "lmath.h"
#include "search.h"
#include "params.h"
#include "play.h"
#include "ordermoves.h"

MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move)
{
	MoveList ordered = *move_list;
	int scores[256] = {0};

	const int TT_SCORE = 100000000;
	const int CAPTURE_BASE = 90000000;
	const int KILLER_BASE = 80000000;

	bool have_cont = (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid);
	int cont_piece = have_cont ? cont_stack[ply - 1].piece : 0;
	int cont_to = have_cont ? cont_stack[ply - 1].to : 0;

	for (unsigned int i = 0; i < ordered.offset; i++)
	{
		uint16_t move = ordered.movelist[i];

		if (move == tt_move)
		{
			scores[i] = TT_SCORE;
			continue;
		}

		int from = move_from(move);
		int to = move_to(move);
		int attacker = piece_on_square(board, from);
		int victim = piece_on_square(board, to);
		bool is_capture = is_capture_move(board, move);

		if (is_capture && attacker != -1)
		{
			if (victim == -1)
				victim = 0;

			int mvv_lva = piece_value_lva(victim) * MVV_LVA_VICTIM_MULT - piece_value_lva(attacker);
			int history_score = capture_history[board->turn][attacker][to][victim];
			scores[i] = CAPTURE_BASE + mvv_lva + history_score;
			continue;
		}

		bool is_killer = false;
		if (ply < MAX_GAME_PLY)
		{
			if (move == killer_moves[ply][0] || move == killer_moves[ply][1])
			{
				int bonus = (move == killer_moves[ply][0]) ? 1 : 0;
				scores[i] = KILLER_BASE + bonus;
				is_killer = true;
			}
		}

		if (!is_killer)
		{
			int score = butterfly_hist[board->turn][from][to];

			if (have_cont && attacker != -1)
				score += cont_hist[board->turn][cont_piece][cont_to][attacker][to];

			scores[i] = score;
		}
	}

	for (unsigned int i = 0; i < ordered.offset; i++)
	{
		unsigned int best = i;
		for (unsigned int j = i + 1; j < ordered.offset; j++)
		{
			if (scores[j] > scores[best])
				best = j;
		}
		if (best != i)
		{
			uint16_t tmp_move = ordered.movelist[i];
			ordered.movelist[i] = ordered.movelist[best];
			ordered.movelist[best] = tmp_move;

			int tmp_score = scores[i];
			scores[i] = scores[best];
			scores[best] = tmp_score;
		}
	}

	return ordered;
}


enum { STAGE_TT, STAGE_NOISY, STAGE_QUIET, STAGE_DONE };

void movepicker_init(MovePicker *mp, Position *board, int ply, uint16_t tt_move)
{
	mp->board = board;
	mp->ply = ply;
	mp->stage = STAGE_TT;
	mp->idx = 0;
	mp->end = 0;
	mp->tt_move = 0;
	mp->list.offset = 0;
	mp->num_noisy = 0;

	MoveList all = {0};
	legalMoveGen(board, &all);

	const int CAPTURE_BASE = 90000000;
	const int KILLER_BASE = 80000000;

	bool have_cont = (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid);
	int cont_piece = have_cont ? cont_stack[ply - 1].piece : 0;
	int cont_to = have_cont ? cont_stack[ply - 1].to : 0;

	uint16_t quiet_moves[256];
	int quiet_scores[256];
	unsigned num_quiets = 0;

	for (unsigned i = 0; i < all.offset; i++)
	{
		uint16_t move = all.movelist[i];

		if (tt_move != 0 && move == tt_move)
		{
			mp->tt_move = move;
			continue;
		}

		int from = move_from(move);
		int to = move_to(move);
		int attacker = piece_on_square(board, from);
		int victim = piece_on_square(board, to);
		bool is_cap = is_capture_move(board, move);
		bool is_promo = is_promotion_move(move);

		if ((is_cap || is_promo) && attacker != -1)
		{
			if (victim == -1)
				victim = 0; // en passant

			int s = CAPTURE_BASE;
			if (is_cap)
				s += piece_value_lva(victim) * MVV_LVA_VICTIM_MULT -
					 piece_value_lva(attacker) +
					 capture_history[board->turn][attacker][to][victim];
			if (is_promo)
				s += 1000000;

			mp->scores[mp->num_noisy] = s;
			mp->list.movelist[mp->num_noisy++] = move;
		}
		else
		{
			int s;
			if (ply < MAX_GAME_PLY && move == killer_moves[ply][0])
				s = KILLER_BASE + 1;
			else if (ply < MAX_GAME_PLY && move == killer_moves[ply][1])
				s = KILLER_BASE;
			else
			{
				s = butterfly_hist[board->turn][from][to];
				if (have_cont && attacker != -1)
					s += cont_hist[board->turn][cont_piece][cont_to][attacker][to];
			}
			quiet_scores[num_quiets] = s;
			quiet_moves[num_quiets++] = move;
		}
	}

	// quiets go after the noisy moves in the same arrays
	for (unsigned i = 0; i < num_quiets; i++)
	{
		mp->list.movelist[mp->num_noisy + i] = quiet_moves[i];
		mp->scores[mp->num_noisy + i] = quiet_scores[i];
	}
	mp->list.offset = mp->num_noisy + num_quiets;
}

static uint16_t pick_best(MovePicker *mp)
{
	unsigned best = mp->idx;
	for (unsigned j = mp->idx + 1; j < mp->end; j++)
		if (mp->scores[j] > mp->scores[best])
			best = j;

	uint16_t m = mp->list.movelist[best];
	mp->list.movelist[best] = mp->list.movelist[mp->idx];
	mp->scores[best] = mp->scores[mp->idx];
	mp->idx++;
	return m;
}

uint16_t movepicker_next(MovePicker *mp)
{
	if (mp->stage == STAGE_TT)
	{
		mp->stage = STAGE_NOISY;
		mp->idx = 0;
		mp->end = mp->num_noisy;
		if (mp->tt_move)
			return mp->tt_move;
	}

	if (mp->stage == STAGE_NOISY)
	{
		if (mp->idx < mp->end)
			return pick_best(mp);
		mp->stage = STAGE_QUIET;
		mp->idx = mp->num_noisy;
		mp->end = mp->list.offset;
	}

	if (mp->stage == STAGE_QUIET)
	{
		if (mp->idx < mp->end)
			return pick_best(mp);
		mp->stage = STAGE_DONE;
	}

	return 0;
}