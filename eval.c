#include "eval.h"
#include "lmath.h"
#include "stdio.h"
#include "play.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

#ifndef NNUE_FILE
#define NNUE_FILE "beans.bin"
#endif

#define NNUE_INPUT   768   // 12 piece-planes * 64 squares, perspective-relative
#define NNUE_HL      64    // hidden layer width (per perspective)
#define NNUE_QA      255   // input/hidden quantization
#define NNUE_QB      64    // output layer quantization
#define NNUE_SCALE   400   // final centipawn scale

#define NNUE_FLIP(sq) ((sq) ^ 56)

static int16_t nnue_featureWeights[NNUE_INPUT][NNUE_HL];
static int16_t nnue_featureBiases[NNUE_HL];
static int16_t nnue_outputWeights[2 * NNUE_HL];
static int32_t nnue_outputBias;

static int nnue_loaded = 0;

static const int nnue_internalToNnueType[6] = {
    0, // pawn   -> 0
    2, // bishop -> 2
    1, // knight -> 1
    3, // rook   -> 3
    4, // queen  -> 4
    5, // king   -> 5
};

static int nnue_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "nnue_load: could not open %s\n", path);
        return 1;
    }

    size_t n;

    n = fread(nnue_featureWeights, sizeof(int16_t), (size_t)NNUE_INPUT * NNUE_HL, f);
    if (n != (size_t)NNUE_INPUT * NNUE_HL) { fclose(f); fprintf(stderr, "nnue_load: short read on feature weights (%zu)\n", n); return 2; }

    n = fread(nnue_featureBiases, sizeof(int16_t), NNUE_HL, f);
    if (n != NNUE_HL) { fclose(f); fprintf(stderr, "nnue_load: short read on feature biases (%zu)\n", n); return 3; }

    n = fread(nnue_outputWeights, sizeof(int16_t), 2 * NNUE_HL, f);
    if (n != 2 * NNUE_HL) { fclose(f); fprintf(stderr, "nnue_load: short read on output weights (%zu)\n", n); return 4; }

    int16_t bias16 = 0;
    n = fread(&bias16, sizeof(int16_t), 1, f);
    if (n != 1) { fclose(f); fprintf(stderr, "nnue_load: short read on output bias (%zu)\n", n); return 5; }
    nnue_outputBias = bias16;

    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long end = ftell(f);
    fclose(f);


    nnue_loaded = 1;
    return 0;
}

static inline int nnue_screlu(int16_t x)
{
    int v = x;
    if (v < 0) v = 0;
    if (v > NNUE_QA) v = NNUE_QA;
    return v * v;
}

static void nnue_buildAccumulator(Position *board, int ownSide, int16_t acc[NNUE_HL])
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] = nnue_featureBiases[h];

    int otherSide = ownSide ^ 1;

    for (int internalPiece = 0; internalPiece < 6; internalPiece++)
    {
        int nnueType = nnue_internalToNnueType[internalPiece];

        uint64_t bb = board->pieces[internalPiece] & board->color[ownSide];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;


            int relSq = (ownSide == 0) ? NNUE_FLIP(sq) : sq;
            int featIdx = nnueType * 64 + relSq;

            for (int h = 0; h < NNUE_HL; h++)
                acc[h] += nnue_featureWeights[featIdx][h];
        }

        bb = board->pieces[internalPiece] & board->color[otherSide];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int relSq = (ownSide == 0) ? NNUE_FLIP(sq) : sq;
            int featIdx = (nnueType + 6) * 64 + relSq;

            for (int h = 0; h < NNUE_HL; h++)
                acc[h] += nnue_featureWeights[featIdx][h];
        }
    }
}

static int nnue_forward(Position *board)
{


    int16_t accUs[NNUE_HL];
    int16_t accThem[NNUE_HL];

    nnue_buildAccumulator(board, board->turn, accUs);
    nnue_buildAccumulator(board, board->turn ^ 1, accThem);

    long long unscaled = 0;
    for (int h = 0; h < NNUE_HL; h++)
        unscaled += (long long)nnue_screlu(accUs[h]) * nnue_outputWeights[h];
    for (int h = 0; h < NNUE_HL; h++)
        unscaled += (long long)nnue_screlu(accThem[h]) * nnue_outputWeights[NNUE_HL + h];

    long long step = unscaled / NNUE_QA;
    step += nnue_outputBias;
    step *= NNUE_SCALE;
    step /= ((long long)NNUE_QA * NNUE_QB);

    return (int)step;
}

void init_tables()
{
    if (nnue_load(NNUE_FILE) != 0)
    {
        fprintf(stderr, "FATAL: failed to load NNUE network from \"%s\"\n", NNUE_FILE);
        exit(1);
    }
}

int eval(Position *board)
{
    return nnue_forward(board);
}