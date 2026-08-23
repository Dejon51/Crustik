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
#define EVALFILE "quant128hl.bin"
#endif

INCBIN(EvalFile, EVALFILE);

#define NNUE_INPUT   768
#define NNUE_HL      128
#define NNUE_QA      255
#define NNUE_QB      64
#define NNUE_SCALE   400

#define NNUE_FLIP(sq) ((sq) ^ 56)

static int16_t nnue_featureWeights[NNUE_INPUT][NNUE_HL];
static int16_t nnue_featureBiases[NNUE_HL];
static int16_t nnue_outputWeights[2 * NNUE_HL];
static int32_t nnue_outputBias;

static int nnue_loaded = 0;

static const int nnue_internalToNnueType[6] = {
    0,
    2,
    1,
    3,
    4,
    5,
};

typedef struct {
    int16_t v[2][NNUE_HL];
} NnueAccumulator;

static NnueAccumulator nnue_stack[NNUE_MAX_PLY];

static inline int nnue_clampPly(int ply)
{
    if (ply < 0) return 0;
    if (ply >= NNUE_MAX_PLY) return NNUE_MAX_PLY - 1;
    return ply;
}

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

static inline int nnue_featureIndex(int persp, int pieceIsOwn, int internalPiece, int sq)
{
    int nnueType = nnue_internalToNnueType[internalPiece];
    int relSq = (persp == 0) ? NNUE_FLIP(sq) : sq;
    return (pieceIsOwn ? nnueType : nnueType + 6) * 64 + relSq;
}

static inline void nnue_addFeature(int16_t acc[NNUE_HL], int featIdx)
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] += nnue_featureWeights[featIdx][h];
}

static inline void nnue_subFeature(int16_t acc[NNUE_HL], int featIdx)
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] -= nnue_featureWeights[featIdx][h];
}

static void nnue_touchPiece(NnueAccumulator *acc, int internalPiece, int color, int sq, int sign)
{
    for (int persp = 0; persp < 2; persp++)
    {
        int pieceIsOwn = (color == persp);
        int featIdx = nnue_featureIndex(persp, pieceIsOwn, internalPiece, sq);
        if (sign > 0)
            nnue_addFeature(acc->v[persp], featIdx);
        else
            nnue_subFeature(acc->v[persp], featIdx);
    }
}

static void nnue_buildSide(Position *board, int ownSide, int16_t acc[NNUE_HL])
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

void nnue_refresh(Position *board, int ply)
{
    ply = nnue_clampPly(ply);
    nnue_buildSide(board, 0, nnue_stack[ply].v[0]);
    nnue_buildSide(board, 1, nnue_stack[ply].v[1]);
}

void nnue_copy(int parentPly, int childPly)
{
    parentPly = nnue_clampPly(parentPly);
    childPly  = nnue_clampPly(childPly);
    if (parentPly == childPly) return;
    memcpy(&nnue_stack[childPly], &nnue_stack[parentPly], sizeof(NnueAccumulator));
}

void nnue_update(Position *board, uint16_t move, int parentPly, int childPly)
{
    parentPly = nnue_clampPly(parentPly);
    childPly  = nnue_clampPly(childPly);

    if (parentPly != childPly)
        memcpy(&nnue_stack[childPly], &nnue_stack[parentPly], sizeof(NnueAccumulator));

    NnueAccumulator *acc = &nnue_stack[childPly];

    int to   = move & 0x3F;
    int from = (move >> 6) & 0x3F;
    int flag = (move >> 12) & 0xF;

    int c    = board->turn;
    int them = !c;

    int piece  = board->mailbox[from];
    int victim = board->mailbox[to];

    if (piece == 6) return;

    nnue_touchPiece(acc, piece, c, from, -1);

    if (piece == PAWNNUMBER && to == board->epsquare && board->epsquare != -1)
    {
        int capSq = to + (c == 0 ? 8 : -8);
        nnue_touchPiece(acc, PAWNNUMBER, them, capSq, -1);
    }
    else if (victim != 6)
    {
        nnue_touchPiece(acc, victim, them, to, -1);
    }

    int placedPiece = piece;
    switch (flag)
    {
        case 5: placedPiece = BISHOPNUMBER; break;
        case 6: placedPiece = HORSENUMBER;  break;
        case 7: placedPiece = ROOKNUMBER;   break;
        case 8: placedPiece = QUEENNUMBER;  break;
    }
    nnue_touchPiece(acc, placedPiece, c, to, +1);

    switch (flag)
    {
        case 1:
            nnue_touchPiece(acc, ROOKNUMBER, c, H1, -1);
            nnue_touchPiece(acc, ROOKNUMBER, c, F1, +1);
            break;
        case 2:
            nnue_touchPiece(acc, ROOKNUMBER, c, A1, -1);
            nnue_touchPiece(acc, ROOKNUMBER, c, D1, +1);
            break;
        case 4:
            nnue_touchPiece(acc, ROOKNUMBER, c, H8, -1);
            nnue_touchPiece(acc, ROOKNUMBER, c, F8, +1);
            break;
        case 3:
            nnue_touchPiece(acc, ROOKNUMBER, c, A8, -1);
            nnue_touchPiece(acc, ROOKNUMBER, c, D8, +1);
            break;
    }
}

static int nnue_forward(Position *board, int ply)
{
    ply = nnue_clampPly(ply);
    int16_t *accUs   = nnue_stack[ply].v[board->turn];
    int16_t *accThem = nnue_stack[ply].v[board->turn ^ 1];

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

void init_tables(void)
{
    if (nnue_load() != 0)
    {
        fprintf(stderr, "FATAL: failed to load embedded NNUE network (built with EVALFILE=" EVALFILE ")\n");
        exit(1);
    }
}

int eval(Position *board, int ply)
{
    return nnue_forward(board, ply);
}