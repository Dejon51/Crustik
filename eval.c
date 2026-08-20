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
#include "incbin.h"

#ifndef EVALFILE
#define EVALFILE "quantised.bin"
#endif

INCBIN(EvalFile, EVALFILE);

#define NNUE_INPUT   768   // 12 piece-planes * 64 squares, perspective-relative
#define NNUE_HL      8    // hidden layer width (per perspective)
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

static int nnue_load(void)
{
    const unsigned char *data = gEvalFileData;
    size_t size = (size_t)gEvalFileSize;
    size_t offset = 0;

    size_t needed = sizeof(nnue_featureWeights)
                  + sizeof(nnue_featureBiases)
                  + sizeof(nnue_outputWeights)
                  + sizeof(int16_t);

    if (size < needed)
    {
        fprintf(stderr, "nnue_load: embedded network too small (%zu < %zu bytes)\n",
                size, needed);
        return 1;
    }

    memcpy(nnue_featureWeights, data + offset, sizeof(nnue_featureWeights));
    offset += sizeof(nnue_featureWeights);

    memcpy(nnue_featureBiases, data + offset, sizeof(nnue_featureBiases));
    offset += sizeof(nnue_featureBiases);

    memcpy(nnue_outputWeights, data + offset, sizeof(nnue_outputWeights));
    offset += sizeof(nnue_outputWeights);

    int16_t bias16 = 0;
    memcpy(&bias16, data + offset, sizeof(bias16));
    offset += sizeof(bias16);
    nnue_outputBias = bias16;

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
    if (nnue_load() != 0)
    {
        fprintf(stderr, "FATAL: failed to load embedded NNUE network (built with EVALFILE=" EVALFILE ")\n");
        exit(1);
    }
}

int eval(Position *board)
{
    return nnue_forward(board);
}