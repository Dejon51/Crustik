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

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VNNI__)
#define NNUE_SIMD_512 1
#include <immintrin.h>
#elif defined(__AVX2__)
#define NNUE_SIMD_256 1
#include <immintrin.h>
#endif

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

#define NNUE_FT_SIZE (2 * NNUE_HL) 
#define NNUE_L1_BLOCKS (NNUE_FT_SIZE / 4)

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

static int8_t nnue_l1Packed[NNUE_L1 * NNUE_FT_SIZE] ALIGN64;
static float nnue_l2WeightsT[NNUE_L1][NNUE_L2] ALIGN64;

static int nnue_loaded = 0;
static int nnue_l1_simd_safe = 1;

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

static void nnue_packWeights(void)
{
	for (int b = 0; b < NNUE_L1_BLOCKS; b++)
		for (int o = 0; o < NNUE_L1; o++)
			for (int k = 0; k < 4; k++)
				nnue_l1Packed[b * (4 * NNUE_L1) + o * 4 + k] =
					nnue_l1Weights[o][b * 4 + k];

	for (int o = 0; o < NNUE_L2; o++)
		for (int i = 0; i < NNUE_L1; i++)
			nnue_l2WeightsT[i][o] = nnue_l2Weights[o][i];
}

static void nnue_checkL1Saturation(void)
{
	int worst = 0;
	for (int o = 0; o < NNUE_L1; o++)
		for (int i = 0; i < NNUE_FT_SIZE; i += 2)
		{
			int a = nnue_l1Weights[o][i];
			int b = nnue_l1Weights[o][i + 1];
			int m = (a < 0 ? -a : a) + (b < 0 ? -b : b);
			if (m > worst)
				worst = m;
		}

	long long bound = (long long)worst * HIDDEN_QUANT_SCALE;
	if (bound > 32767)
	{
		nnue_l1_simd_safe = 0;
		fprintf(stderr,
				"nnue: WARNING - L1 weight pairs reach %lld > 32767 against a "
				"max activation; the AVX-2 maddubs path could saturate. "
				"Falling back to the scalar L1. Rebuild with AVX-512 VNNI or "
				"retrain with tighter L1 weight clipping.\n",
				bound);
	}
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

	nnue_packWeights();
	nnue_checkL1Saturation();

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

static void nnue_ftHalf_scalar(const int16_t *acc, uint8_t *out)
{
	for (int h = 0; h < NNUE_HL; h++)
	{
		int c = acc[h];
		c = c < 0 ? 0 : (c > HIDDEN_QUANT_SCALE ? HIDDEN_QUANT_SCALE : c);
		out[h] = (uint8_t)((c * c) / HIDDEN_QUANT_SCALE);
	}
}

#if defined(NNUE_SIMD_512) || defined(NNUE_SIMD_256)
#define NNUE_DIV255_MAGIC ((short)0x8081)
#endif

#ifdef NNUE_SIMD_512
static void nnue_ftHalf_simd(const int16_t *acc, uint8_t *out)
{
	const __m512i zero = _mm512_setzero_si512();
	const __m512i qa = _mm512_set1_epi16(HIDDEN_QUANT_SCALE);
	const __m512i mag = _mm512_set1_epi16(NNUE_DIV255_MAGIC);
	const __m512i fix = _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7);

	for (int h = 0; h < NNUE_HL; h += 64)
	{
		__m512i a = _mm512_load_si512((const void *)(acc + h));
		__m512i b = _mm512_load_si512((const void *)(acc + h + 32));

		a = _mm512_min_epi16(_mm512_max_epi16(a, zero), qa);
		b = _mm512_min_epi16(_mm512_max_epi16(b, zero), qa);

		a = _mm512_mullo_epi16(a, a);
		b = _mm512_mullo_epi16(b, b);

		a = _mm512_srli_epi16(_mm512_mulhi_epu16(a, mag), 7);
		b = _mm512_srli_epi16(_mm512_mulhi_epu16(b, mag), 7);

		__m512i p = _mm512_packus_epi16(a, b);
		p = _mm512_permutexvar_epi64(fix, p);
		_mm512_store_si512((void *)(out + h), p);
	}
}
#elif defined(NNUE_SIMD_256)
static void nnue_ftHalf_simd(const int16_t *acc, uint8_t *out)
{
	const __m256i zero = _mm256_setzero_si256();
	const __m256i qa = _mm256_set1_epi16(HIDDEN_QUANT_SCALE);
	const __m256i mag = _mm256_set1_epi16(NNUE_DIV255_MAGIC);

	for (int h = 0; h < NNUE_HL; h += 32)
	{
		__m256i a = _mm256_load_si256((const __m256i *)(acc + h));
		__m256i b = _mm256_load_si256((const __m256i *)(acc + h + 16));

		a = _mm256_min_epi16(_mm256_max_epi16(a, zero), qa);
		b = _mm256_min_epi16(_mm256_max_epi16(b, zero), qa);

		a = _mm256_mullo_epi16(a, a);
		b = _mm256_mullo_epi16(b, b);

		a = _mm256_srli_epi16(_mm256_mulhi_epu16(a, mag), 7);
		b = _mm256_srli_epi16(_mm256_mulhi_epu16(b, mag), 7);

		__m256i p = _mm256_packus_epi16(a, b);
		p = _mm256_permute4x64_epi64(p, 0xD8); /* undo lane interleave */
		_mm256_store_si256((__m256i *)(out + h), p);
	}
}
#else
static void nnue_ftHalf_simd(const int16_t *acc, uint8_t *out)
{
	nnue_ftHalf_scalar(acc, out);
}
#endif


static void nnue_l1_scalar(const uint8_t *ft, int32_t *out)
{
	for (int o = 0; o < NNUE_L1; o++)
	{
		const int8_t *w = nnue_l1Weights[o];
		int32_t sum = 0;
		for (int i = 0; i < NNUE_FT_SIZE; i++)
			sum += (int32_t)ft[i] * (int32_t)w[i];
		out[o] = sum;
	}
}

#ifdef NNUE_SIMD_512
static void nnue_l1_simd(const uint8_t *ft, int32_t *out)
{
	__m512i a0 = _mm512_setzero_si512();
	__m512i a1 = _mm512_setzero_si512();
	__m512i a2 = _mm512_setzero_si512();
	__m512i a3 = _mm512_setzero_si512();

	const int8_t *w = nnue_l1Packed;
	const uint32_t *in = (const uint32_t *)ft;

	for (int b = 0; b < NNUE_L1_BLOCKS; b += 4, w += 256, in += 4)
	{
		a0 = _mm512_dpbusd_epi32(a0, _mm512_set1_epi32((int)in[0]),
								 _mm512_load_si512((const void *)(w)));
		a1 = _mm512_dpbusd_epi32(a1, _mm512_set1_epi32((int)in[1]),
								 _mm512_load_si512((const void *)(w + 64)));
		a2 = _mm512_dpbusd_epi32(a2, _mm512_set1_epi32((int)in[2]),
								 _mm512_load_si512((const void *)(w + 128)));
		a3 = _mm512_dpbusd_epi32(a3, _mm512_set1_epi32((int)in[3]),
								 _mm512_load_si512((const void *)(w + 192)));
	}

	__m512i s = _mm512_add_epi32(_mm512_add_epi32(a0, a1),
								 _mm512_add_epi32(a2, a3));
	_mm512_store_si512((void *)out, s);
}
#elif defined(NNUE_SIMD_256)
static inline __m256i nnue_dp256(__m256i acc, __m256i u, __m256i s)
{
#ifdef __AVXVNNI__
	return _mm256_dpbusd_avx_epi32(acc, u, s);
#else
	__m256i p = _mm256_maddubs_epi16(u, s);
	__m256i q = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
	return _mm256_add_epi32(acc, q);
#endif
}

static void nnue_l1_simd(const uint8_t *ft, int32_t *out)
{
	__m256i a0 = _mm256_setzero_si256(), b0 = _mm256_setzero_si256();
	__m256i a1 = _mm256_setzero_si256(), b1 = _mm256_setzero_si256();
	__m256i a2 = _mm256_setzero_si256(), b2 = _mm256_setzero_si256();
	__m256i a3 = _mm256_setzero_si256(), b3 = _mm256_setzero_si256();

	const int8_t *w = nnue_l1Packed;
	const uint32_t *in = (const uint32_t *)ft;

	for (int b = 0; b < NNUE_L1_BLOCKS; b += 4, w += 256, in += 4)
	{
		__m256i i0 = _mm256_set1_epi32((int)in[0]);
		__m256i i1 = _mm256_set1_epi32((int)in[1]);
		__m256i i2 = _mm256_set1_epi32((int)in[2]);
		__m256i i3 = _mm256_set1_epi32((int)in[3]);

		a0 = nnue_dp256(a0, i0, _mm256_load_si256((const __m256i *)(w)));
		b0 = nnue_dp256(b0, i0, _mm256_load_si256((const __m256i *)(w + 32)));
		a1 = nnue_dp256(a1, i1, _mm256_load_si256((const __m256i *)(w + 64)));
		b1 = nnue_dp256(b1, i1, _mm256_load_si256((const __m256i *)(w + 96)));
		a2 = nnue_dp256(a2, i2, _mm256_load_si256((const __m256i *)(w + 128)));
		b2 = nnue_dp256(b2, i2, _mm256_load_si256((const __m256i *)(w + 160)));
		a3 = nnue_dp256(a3, i3, _mm256_load_si256((const __m256i *)(w + 192)));
		b3 = nnue_dp256(b3, i3, _mm256_load_si256((const __m256i *)(w + 224)));
	}

	a0 = _mm256_add_epi32(_mm256_add_epi32(a0, a1), _mm256_add_epi32(a2, a3));
	b0 = _mm256_add_epi32(_mm256_add_epi32(b0, b1), _mm256_add_epi32(b2, b3));

	_mm256_store_si256((__m256i *)(out), a0);
	_mm256_store_si256((__m256i *)(out + 8), b0);
}
#else
static void nnue_l1_simd(const uint8_t *ft, int32_t *out)
{
	nnue_l1_scalar(ft, out);
}
#endif

static inline void nnue_l2(const float *l1out, float *l2out)
{
#if defined(NNUE_SIMD_256) || defined(NNUE_SIMD_512)
	__m256 o0 = _mm256_load_ps(nnue_l2Biases + 0);
	__m256 o1 = _mm256_load_ps(nnue_l2Biases + 8);
	__m256 o2 = _mm256_load_ps(nnue_l2Biases + 16);
	__m256 o3 = _mm256_load_ps(nnue_l2Biases + 24);

	for (int i = 0; i < NNUE_L1; i++)
	{
		const __m256 x = _mm256_set1_ps(l1out[i]);
		const float *w = nnue_l2WeightsT[i];
#ifdef __FMA__
		o0 = _mm256_fmadd_ps(x, _mm256_load_ps(w + 0), o0);
		o1 = _mm256_fmadd_ps(x, _mm256_load_ps(w + 8), o1);
		o2 = _mm256_fmadd_ps(x, _mm256_load_ps(w + 16), o2);
		o3 = _mm256_fmadd_ps(x, _mm256_load_ps(w + 24), o3);
#else
		o0 = _mm256_add_ps(o0, _mm256_mul_ps(x, _mm256_load_ps(w + 0)));
		o1 = _mm256_add_ps(o1, _mm256_mul_ps(x, _mm256_load_ps(w + 8)));
		o2 = _mm256_add_ps(o2, _mm256_mul_ps(x, _mm256_load_ps(w + 16)));
		o3 = _mm256_add_ps(o3, _mm256_mul_ps(x, _mm256_load_ps(w + 24)));
#endif
	}

	const __m256 lo = _mm256_setzero_ps();
	const __m256 hi = _mm256_set1_ps(1.0f);
	o0 = _mm256_min_ps(_mm256_max_ps(o0, lo), hi);
	o1 = _mm256_min_ps(_mm256_max_ps(o1, lo), hi);
	o2 = _mm256_min_ps(_mm256_max_ps(o2, lo), hi);
	o3 = _mm256_min_ps(_mm256_max_ps(o3, lo), hi);

	_mm256_store_ps(l2out + 0, _mm256_mul_ps(o0, o0));
	_mm256_store_ps(l2out + 8, _mm256_mul_ps(o1, o1));
	_mm256_store_ps(l2out + 16, _mm256_mul_ps(o2, o2));
	_mm256_store_ps(l2out + 24, _mm256_mul_ps(o3, o3));
#else
	for (int o = 0; o < NNUE_L2; o++)
	{
		const float *w = nnue_l2Weights[o];
		float y = nnue_l2Biases[o];
		for (int i = 0; i < NNUE_L1; i++)
			y += l1out[i] * w[i];
		l2out[o] = nnue_screluf(y);
	}
#endif
}

static int nnue_forward(Position *board, int ply)
{
	ply = nnue_clampPly(ply);
	nnue_ensureReady(board, ply);

	const int16_t *accUs = nnue_stack[ply].vector[board->turn];
	const int16_t *accThem = nnue_stack[ply].vector[board->turn ^ 1];

	uint8_t ft[NNUE_FT_SIZE] ALIGN64;
	nnue_ftHalf_simd(accUs, ft);
	nnue_ftHalf_simd(accThem, ft + NNUE_HL);

	int32_t l1sum[NNUE_L1] ALIGN64;
	if (nnue_l1_simd_safe)
		nnue_l1_simd(ft, l1sum);
	else
		nnue_l1_scalar(ft, l1sum);

	const float l1_dequant =
		1.0f / ((float)HIDDEN_QUANT_SCALE * (float)NNUE_OUTPUT_SCALE);

	float l1out[NNUE_L1] ALIGN64;
	for (int o = 0; o < NNUE_L1; o++)
		l1out[o] =
			nnue_screluf((float)l1sum[o] * l1_dequant + nnue_l1Biases[o]);

	float l2out[NNUE_L2] ALIGN64;
	nnue_l2(l1out, l2out);

	float out = nnue_l3Bias;
	for (int i = 0; i < NNUE_L2; i++)
		out += l2out[i] * nnue_l3Weights[i];

	return (int)(out * NNUE_SCALE);
}

static uint64_t nnue_testRng(uint64_t *s)
{
	uint64_t x = *s;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	*s = x;
	return x;
}

static int nnue_selfTest(void)
{
	static int16_t acc[2][NNUE_HL] ALIGN64;
	static uint8_t ftA[NNUE_FT_SIZE] ALIGN64;
	static uint8_t ftB[NNUE_FT_SIZE] ALIGN64;
	static int32_t outA[NNUE_L1] ALIGN64;
	static int32_t outB[NNUE_L1] ALIGN64;

	uint64_t s = 0x9E3779B97F4A7C15ull;
	int bad = 0;

	for (int trial = 0; trial < 8 && !bad; trial++)
	{
		for (int p = 0; p < 2; p++)
			for (int h = 0; h < NNUE_HL; h++)
				acc[p][h] = (int16_t)((int)(nnue_testRng(&s) % 1201) - 600);

		nnue_ftHalf_scalar(acc[0], ftA);
		nnue_ftHalf_scalar(acc[1], ftA + NNUE_HL);
		nnue_ftHalf_simd(acc[0], ftB);
		nnue_ftHalf_simd(acc[1], ftB + NNUE_HL);

		for (int i = 0; i < NNUE_FT_SIZE; i++)
			if (ftA[i] != ftB[i])
			{
				fprintf(stderr,
						"nnue_selfTest: FT mismatch at %d (scalar %u, simd "
						"%u)\n",
						i, (unsigned)ftA[i], (unsigned)ftB[i]);
				bad = 1;
				break;
			}
		if (bad)
			break;

		nnue_l1_scalar(ftA, outA);
		if (nnue_l1_simd_safe)
			nnue_l1_simd(ftA, outB);
		else
			nnue_l1_scalar(ftA, outB);

		for (int o = 0; o < NNUE_L1; o++)
			if (outA[o] != outB[o])
			{
				fprintf(stderr,
						"nnue_selfTest: L1 mismatch at %d (scalar %d, simd "
						"%d)\n",
						o, outA[o], outB[o]);
				bad = 1;
				break;
			}
	}

	return bad;
}

void init_tables(void)
{
	if (nnue_load() != 0)
	{
		fprintf(stderr, "FATAL: failed to load embedded NNUE network (built "
						"with EVALFILE=" EVALFILE ")\n");
		exit(1);
	}

	if (nnue_selfTest() != 0)
	{
		fprintf(stderr, "FATAL: NNUE SIMD path disagrees with the scalar "
						"reference. Refusing to run.\n");
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