#include "eval.h"
#include "incbin.h"
#include "lmath.h"
#include "play.h"
#include "precomputed.h"
#include "stdio.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN64 __attribute__((aligned(64)))

#ifndef EVALFILE
#define EVALFILE "joe.bin"
#endif

INCBIN(EvalFile, EVALFILE);

#define NNUE_INPUT 768
#define NNUE_HL 512
#define NNUE_L1 16
#define NNUE_L2 32
#define HIDDEN_QUANT_SCALE 255 /* QA */
#define NNUE_OUTPUT_SCALE 64   /* QB */

#define NNUE_SCALE (400.0 * 0.6306804414)

#define NNUE_FLIP(sq) ((sq) ^ 56)

static int16_t nnue_featureWeights[NNUE_INPUT][NNUE_HL] ALIGN64;
static int16_t nnue_hiddenBiases[NNUE_HL] ALIGN64;
static int8_t nnue_l1Weights[NNUE_L1][2 * NNUE_HL] ALIGN64;
static float nnue_l1Biases[NNUE_L1];
static float nnue_l2Weights[NNUE_L2][NNUE_L1];
static float nnue_l2Biases[NNUE_L2];
static float nnue_l3Weights[NNUE_L2];
static float nnue_l3Bias;

static int nnue_loaded = 0;

// Converts internal engine piece encoding into nnue encoding
static const int internal_to_nnue_encoding[6] = {
	0,
	2,
	1,
	3,
	4,
	5,
};

typedef struct
{
	int16_t vector[2][NNUE_HL] ALIGN64;
	uint8_t mirror[2];
	uint8_t needsRefresh[2];
} NnueAccumulator;

static NnueAccumulator nnue_stack[NNUE_MAX_PLY] ALIGN64;

static inline int nnue_clampPly(int ply)
{
	if (ply < 0)
		return 0;
	if (ply >= NNUE_MAX_PLY)
		return NNUE_MAX_PLY - 1;
	return ply;
}

static inline bool mirror_or_not(Bitboard king_bitboard)
{
	if (!king_bitboard)
		return 0;
	return (__builtin_ctzll(king_bitboard) & 7) > 3;
}

// Loads nnue from bin file
static int nnue_load(void)
{
	const unsigned char *data = gEvalFileData;
	size_t size = (size_t)gEvalFileSize;
	size_t offset = 0;

	const size_t required_size =
		sizeof(nnue_featureWeights) + sizeof(nnue_hiddenBiases) +
		sizeof(nnue_l1Weights) + sizeof(nnue_l1Biases) +
		sizeof(nnue_l2Weights) + sizeof(nnue_l2Biases) +
		sizeof(nnue_l3Weights) + sizeof(nnue_l3Bias);

	if (size < required_size || size >= required_size + 64)
	{
		fprintf(stderr,
				"nnue_load: embedded network is %zu bytes, expected %zu "
				"(plus up to 63 bytes of padding). Wrong net or wrong "
				"architecture.\n",
				size, required_size);
		return 1;
	}

	memcpy(nnue_featureWeights, data + offset, sizeof(nnue_featureWeights));
	offset += sizeof(nnue_featureWeights);

	memcpy(nnue_hiddenBiases, data + offset, sizeof(nnue_hiddenBiases));
	offset += sizeof(nnue_hiddenBiases);

	memcpy(nnue_l1Weights, data + offset, sizeof(nnue_l1Weights));
	offset += sizeof(nnue_l1Weights);

	memcpy(nnue_l1Biases, data + offset, sizeof(nnue_l1Biases));
	offset += sizeof(nnue_l1Biases);

	memcpy(nnue_l2Weights, data + offset, sizeof(nnue_l2Weights));
	offset += sizeof(nnue_l2Weights);

	memcpy(nnue_l2Biases, data + offset, sizeof(nnue_l2Biases));
	offset += sizeof(nnue_l2Biases);

	memcpy(nnue_l3Weights, data + offset, sizeof(nnue_l3Weights));
	offset += sizeof(nnue_l3Weights);

	memcpy(&nnue_l3Bias, data + offset, sizeof(nnue_l3Bias));
	offset += sizeof(nnue_l3Bias);

	nnue_loaded = 1;
	return 0;
}

static inline int nnue_inputIndex(int persp, int pieceIsOwn, int internal_piece,
								  int sq, bool mirror)
{
	int nnue_piece = internal_to_nnue_encoding[internal_piece];
	int relSq = (persp == 0) ? NNUE_FLIP(sq) : sq;
	if (mirror)
		relSq ^= 7;
	return (pieceIsOwn ? nnue_piece : nnue_piece + 6) * 64 + relSq;
}

static inline void nnue_addFeature(int16_t acc[NNUE_HL], int input_index)
{
	const int16_t *w = nnue_featureWeights[input_index];
	for (int h = 0; h < NNUE_HL; h++)
		acc[h] += w[h];
}

static inline void nnue_addSub(int16_t *dst, const int16_t *src,
							   const int16_t *a, const int16_t *s)
{
	for (int i = 0; i < NNUE_HL; i++)
		dst[i] = src[i] + a[i] - s[i];
}

static inline void nnue_addSubSub(int16_t *dst, const int16_t *src,
								  const int16_t *a, const int16_t *s1,
								  const int16_t *s2)
{
	for (int i = 0; i < NNUE_HL; i++)
		dst[i] = src[i] + a[i] - s1[i] - s2[i];
}

static inline void nnue_addSubAddSub(int16_t *dst, const int16_t *src,
									 const int16_t *a1, const int16_t *s1,
									 const int16_t *a2, const int16_t *s2)
{
	for (int i = 0; i < NNUE_HL; i++)
		dst[i] = src[i] + a1[i] - s1[i] + a2[i] - s2[i];
}

static void nnue_buildSide(Position *board, int ownSide, int16_t acc[NNUE_HL],
						   bool mirror)
{
	memcpy(acc, nnue_hiddenBiases, sizeof(nnue_hiddenBiases));
	int otherSide = ownSide ^ 1;

	for (int p = 0; p < 6; p++)
	{
		uint64_t bb = board->pieces[p] & board->color[ownSide];
		while (bb)
		{
			int sq = pop_lsb(&bb);
			nnue_addFeature(acc, nnue_inputIndex(ownSide, 1, p, sq, mirror));
		}
		bb = board->pieces[p] & board->color[otherSide];
		while (bb)
		{
			int sq = pop_lsb(&bb);
			nnue_addFeature(acc, nnue_inputIndex(ownSide, 0, p, sq, mirror));
		}
	}
}

static void nnue_refreshPersp(Position *board, NnueAccumulator *acc,
							  int persp)
{
	acc->mirror[persp] =
		mirror_or_not(board->pieces[KINGNUMBER] & board->color[persp]);
	nnue_buildSide(board, persp, acc->vector[persp], acc->mirror[persp]);
	acc->needsRefresh[persp] = 0;
}

static inline void nnue_ensureReady(Position *board, int ply)
{
	NnueAccumulator *acc = &nnue_stack[ply];
	if (acc->needsRefresh[0])
		nnue_refreshPersp(board, acc, 0);
	if (acc->needsRefresh[1])
		nnue_refreshPersp(board, acc, 1);
}

void nnue_refresh(Position *board, int ply)
{
	ply = nnue_clampPly(ply);
	NnueAccumulator *acc = &nnue_stack[ply];
	nnue_refreshPersp(board, acc, WHITE);
	nnue_refreshPersp(board, acc, BLACK);
}

void nnue_copy(int parent_ply, int child_ply)
{
	parent_ply = nnue_clampPly(parent_ply);
	child_ply = nnue_clampPly(child_ply);
	if (parent_ply == child_ply)
		return;
	memcpy(&nnue_stack[child_ply], &nnue_stack[parent_ply],
		   sizeof(NnueAccumulator));
}

void nnue_update(Position *board, uint16_t move, int parent_ply,
				 int child_ply)
{
	parent_ply = nnue_clampPly(parent_ply);
	child_ply = nnue_clampPly(child_ply);

	int to = move_to(move);
	int from = move_from(move);
	int flag = move_flag(move);
	int color = board->turn;
	int them = !color;
	int piece = board->mailbox[from];
	int victim = board->mailbox[to];

	if (piece == EMPTYNUMBER)
	{
		nnue_copy(parent_ply, child_ply);
		return;
	}

	nnue_ensureReady(board, parent_ply);

	NnueAccumulator *parent = &nnue_stack[parent_ply];
	NnueAccumulator *child = &nnue_stack[child_ply];

	bool isEp =
		(piece == PAWNNUMBER && to == board->epsquare && board->epsquare != -1);
	int capSq = to + (color == 0 ? 8 : -8);

	int placed = piece;
	switch (flag)
	{ // Promotions
	case 5:
		placed = BISHOPNUMBER;
		break;
	case 6:
		placed = HORSENUMBER;
		break;
	case 7:
		placed = ROOKNUMBER;
		break;
	case 8:
		placed = QUEENNUMBER;
		break;
	}

	int rookFrom = -1, rookTo = -1;
	switch (flag)
	{ // Castling
	case 1:
		rookFrom = H1;
		rookTo = F1;
		break;
	case 2:
		rookFrom = A1;
		rookTo = D1;
		break;
	case 4:
		rookFrom = H8;
		rookTo = F8;
		break;
	case 3:
		rookFrom = A8;
		rookTo = D8;
		break;
	}

	bool kingMirrorFlip =
		(piece == KINGNUMBER) && (((from & 7) > 3) != ((to & 7) > 3));

	for (int persp = 0; persp < 2; persp++)
	{
		if (kingMirrorFlip && persp == color)
		{
			child->mirror[persp] = ((to & 7) > 3);
			child->needsRefresh[persp] = 1;
			continue;
		}

		bool mir = parent->mirror[persp];
		child->mirror[persp] = parent->mirror[persp];
		child->needsRefresh[persp] = 0;

		int own = (color == persp);
		int theirs = (them == persp);
		int addIdx[2], subIdx[2], na = 0, ns = 0;

		subIdx[ns++] = nnue_inputIndex(persp, own, piece, from, mir);
		if (isEp)
			subIdx[ns++] =
				nnue_inputIndex(persp, theirs, PAWNNUMBER, capSq, mir);
		else if (victim != EMPTYNUMBER)
			subIdx[ns++] = nnue_inputIndex(persp, theirs, victim, to, mir);

		addIdx[na++] = nnue_inputIndex(persp, own, placed, to, mir);
		if (rookFrom >= 0)
		{
			subIdx[ns++] =
				nnue_inputIndex(persp, own, ROOKNUMBER, rookFrom, mir);
			addIdx[na++] = nnue_inputIndex(persp, own, ROOKNUMBER, rookTo, mir);
		}

		int16_t *dst = child->vector[persp];
		const int16_t *src = parent->vector[persp];

		if (na == 1 && ns == 1)
			nnue_addSub(dst, src, nnue_featureWeights[addIdx[0]],
						nnue_featureWeights[subIdx[0]]);
		else if (na == 1 && ns == 2)
			nnue_addSubSub(dst, src, nnue_featureWeights[addIdx[0]],
						   nnue_featureWeights[subIdx[0]],
						   nnue_featureWeights[subIdx[1]]);
		else
			nnue_addSubAddSub(dst, src, nnue_featureWeights[addIdx[0]],
							  nnue_featureWeights[subIdx[0]],
							  nnue_featureWeights[addIdx[1]],
							  nnue_featureWeights[subIdx[1]]);
	}
}

static inline float nnue_screluf(float x)
{
	if (x < 0.0f)
		x = 0.0f;
	else if (x > 1.0f)
		x = 1.0f;
	return x * x;
}

static int nnue_forward(Position *board, int ply)
{
	ply = nnue_clampPly(ply);
	nnue_ensureReady(board, ply);

	const int16_t *accUs = nnue_stack[ply].vector[board->turn];
	const int16_t *accThem = nnue_stack[ply].vector[board->turn ^ 1];

	uint8_t ft[2 * NNUE_HL] ALIGN64;
	for (int h = 0; h < NNUE_HL; h++)
	{
		int c = accUs[h];
		c = c < 0 ? 0 : (c > HIDDEN_QUANT_SCALE ? HIDDEN_QUANT_SCALE : c);
		ft[h] = (uint8_t)((c * c) / HIDDEN_QUANT_SCALE);

		c = accThem[h];
		c = c < 0 ? 0 : (c > HIDDEN_QUANT_SCALE ? HIDDEN_QUANT_SCALE : c);
		ft[NNUE_HL + h] = (uint8_t)((c * c) / HIDDEN_QUANT_SCALE);
	}

	const float l1_dequant =
		1.0f / ((float)HIDDEN_QUANT_SCALE * (float)NNUE_OUTPUT_SCALE);

	float l1out[NNUE_L1];
	for (int o = 0; o < NNUE_L1; o++)
	{
		const int8_t *w = nnue_l1Weights[o];
		int32_t sum = 0;
		for (int i = 0; i < 2 * NNUE_HL; i++)
			sum += (int32_t)ft[i] * (int32_t)w[i];
		l1out[o] = nnue_screluf((float)sum * l1_dequant + nnue_l1Biases[o]);
	}

	float l2out[NNUE_L2];
	for (int o = 0; o < NNUE_L2; o++)
	{
		const float *w = nnue_l2Weights[o];
		float y = nnue_l2Biases[o];
		for (int i = 0; i < NNUE_L1; i++)
			y += l1out[i] * w[i];
		l2out[o] = nnue_screluf(y);
	}

	float out = nnue_l3Bias;
	for (int i = 0; i < NNUE_L2; i++)
		out += l2out[i] * nnue_l3Weights[i];

	return (int)(out * NNUE_SCALE);
}

void init_tables(void)
{
	if (nnue_load() != 0)
	{
		fprintf(stderr, "FATAL: failed to load embedded NNUE network (built "
						"with EVALFILE=" EVALFILE ")\n");
		exit(1);
	}
}

int material_phase(Position *board)
{
	int pawns = __builtin_popcountll(board->pieces[PAWNNUMBER]);
	int bishops = __builtin_popcountll(board->pieces[BISHOPNUMBER]);
	int horses = __builtin_popcountll(board->pieces[HORSENUMBER]);
	int rooks = __builtin_popcountll(board->pieces[ROOKNUMBER]);
	int queens = __builtin_popcountll(board->pieces[QUEENNUMBER]);

	return 100 * pawns + 300 * bishops + 300 * horses + 500 * rooks +
		   900 * queens;
}

int eval(Position *board, int ply)
{
	return nnue_forward(board, ply) * (25000 + material_phase(board)) / 32768;
}