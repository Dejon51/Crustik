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
#define EVALFILE "quant256hl.bin"
#endif

INCBIN(EvalFile, EVALFILE);

#define NNUE_INPUT 768
#define NNUE_HL 256
#define HIDDEN_QUANT_SCALE 255
#define NNUE_OUTPUT_SCALE 64
#define NNUE_SCALE 400

#define NNUE_FLIP(sq) ((sq) ^ 56)

static int16_t nnue_featureWeights[NNUE_INPUT][NNUE_HL];
static int16_t nnue_hiddenBiases[NNUE_HL];
static int16_t nnue_outputWeights[2 * NNUE_HL];
static int32_t nnue_outputBias;

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
    int16_t vector[2][NNUE_HL];
    uint8_t mirror[2];
} NnueAccumulator;

static NnueAccumulator nnue_stack[NNUE_MAX_PLY];

static inline int nnue_clampPly(int ply)
{
    if (ply < 0)
        return 0;
    if (ply >= NNUE_MAX_PLY)
        return NNUE_MAX_PLY - 1;
    return ply;
}

static inline bool mirror_or_not(Bitboard king_bitboard){
    if (king_bitboard){
        int sq = __builtin_ctzll(king_bitboard);
        int file = sq & 7;  
        return file > 3;
    }
    else{
        return 0;
    }
}

// Loads nnue from bin file
static int nnue_load(void)
{
    const unsigned char *data = gEvalFileData;
    size_t size = (size_t)gEvalFileSize;
    size_t offset = 0;

    size_t required_size = sizeof(nnue_featureWeights) + sizeof(nnue_hiddenBiases) + sizeof(nnue_outputWeights) + sizeof(int16_t);

    if (size < required_size)
    {
        fprintf(stderr, "nnue_load: embedded network too small (%zu < %zu bytes)\n",
                size, required_size);
        return 1;
    }

    memcpy(nnue_featureWeights, data + offset, sizeof(nnue_featureWeights)); // Copy to static array
    offset += sizeof(nnue_featureWeights);                                   // Moves offset from weights to biases

    memcpy(nnue_hiddenBiases, data + offset, sizeof(nnue_hiddenBiases)); // Copy to static array
    offset += sizeof(nnue_hiddenBiases);                                  // Moves offset from biases to output weights

    memcpy(nnue_outputWeights, data + offset, sizeof(nnue_outputWeights)); // Copy to static array
    offset += sizeof(nnue_outputWeights);                                  // Moves offset from output weights to output neuron's bias

    int16_t bias16 = 0;
    memcpy(&bias16, data + offset, sizeof(bias16)); // Copy to to pointer
    offset += sizeof(bias16);
    nnue_outputBias = bias16;

    nnue_loaded = 1;
    return 0;
}

// Screlu activation function
static inline int nnue_screlu(int16_t x)
{
    int v = x;
    if (v < 0)
        v = 0;
    if (v > HIDDEN_QUANT_SCALE)
        v = HIDDEN_QUANT_SCALE;
    return v * v;
}

static inline int nnue_inputIndex(int persp, int pieceIsOwn, int internal_piece, int sq,bool mirror)
{
    int nnue_piece = internal_to_nnue_encoding[internal_piece];
    int relSq = (persp == 0) ? NNUE_FLIP(sq) : sq; // Flip relative perspective because internal representation is flipped
    if (mirror) relSq ^= 7; // flips square for mirroring
    return (pieceIsOwn ? nnue_piece : nnue_piece + 6) * 64 + relSq;
}

static inline void nnue_addFeature(int16_t acc[NNUE_HL], int input_index)
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] += nnue_featureWeights[input_index][h];
}

static inline void nnue_subFeature(int16_t acc[NNUE_HL], int input_index)
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] -= nnue_featureWeights[input_index][h];
}

// Adds or removes piece when board is changed
static void nnue_touchPiece(NnueAccumulator *acc, int internal_piece, int color, int sq, int sign)
{
    for (int persp = 0; persp < 2; persp++)
    {
        int pieceIsOwn = (color == persp);
        int input_index = nnue_inputIndex(persp, pieceIsOwn, internal_piece, sq, acc->mirror[persp]); // passes in mirror parameter
        if (sign > 0)
            nnue_addFeature(acc->vector[persp], input_index);
        else
            nnue_subFeature(acc->vector[persp], input_index);
    }
}

// Single sided touch piece function
static void nnue_touchPiecePerspective(NnueAccumulator *acc, int internal_piece, int color, int sq, int sign, int persp)
{
    int pieceIsOwn = (color == persp);
    int input_index = nnue_inputIndex(persp, pieceIsOwn, internal_piece, sq, acc->mirror[persp]);
    if (sign > 0)
        nnue_addFeature(acc->vector[persp], input_index);
    else
        nnue_subFeature(acc->vector[persp], input_index);
}

static void nnue_buildSide(Position *board, int ownSide, int16_t acc[NNUE_HL], bool mirror)
{
    for (int h = 0; h < NNUE_HL; h++)
        acc[h] = nnue_hiddenBiases[h];

    int otherSide = ownSide ^ 1;

    for (int internal_piece = 0; internal_piece < 6; internal_piece++)
    {
        uint64_t bb = board->pieces[internal_piece] & board->color[ownSide];
        while (bb)
        {
            int sq = pop_lsb(&bb);
            nnue_addFeature(acc, nnue_inputIndex(ownSide, 1, internal_piece, sq, mirror));
        }

        bb = board->pieces[internal_piece] & board->color[otherSide];
        while (bb)
        {
            int sq = pop_lsb(&bb);
            nnue_addFeature(acc, nnue_inputIndex(ownSide, 0, internal_piece, sq, mirror));
        }
    }
}

void nnue_refresh(Position *board, int ply)
{
    ply = nnue_clampPly(ply);
    NnueAccumulator *acc = &nnue_stack[ply];

    acc->mirror[WHITE] = mirror_or_not(board->pieces[KINGNUMBER] & board->color[WHITE]); // Sets flag for white to mirror or not
    acc->mirror[BLACK] = mirror_or_not(board->pieces[KINGNUMBER] & board->color[BLACK]); // Sets flag for black to mirror or not

    nnue_buildSide(board, 0, acc->vector[0], acc->mirror[WHITE]);
    nnue_buildSide(board, 1, acc->vector[1], acc->mirror[BLACK]);
}

void nnue_copy(int parent_ply, int child_ply)
{
    parent_ply = nnue_clampPly(parent_ply);
    child_ply = nnue_clampPly(child_ply);
    if (parent_ply == child_ply)
        return;
    memcpy(&nnue_stack[child_ply], &nnue_stack[parent_ply], sizeof(NnueAccumulator));
}

void nnue_update(Position *board, uint16_t move, int parent_ply, int child_ply)
{
    parent_ply = nnue_clampPly(parent_ply);
    child_ply = nnue_clampPly(child_ply);

    if (parent_ply != child_ply)
        memcpy(&nnue_stack[child_ply], &nnue_stack[parent_ply], sizeof(NnueAccumulator));

    NnueAccumulator *acc = &nnue_stack[child_ply];

    int to = move_to(move);
    int from = move_from(move);
    int flag = move_flag(move);

    int color = board->turn;
    int them = !color;

    int piece = board->mailbox[from];
    int victim = board->mailbox[to];

    if (piece == EMPTYNUMBER)
        return;

    nnue_touchPiece(acc, piece, color, from, -1);

    if (piece == PAWNNUMBER && to == board->epsquare && board->epsquare != -1)
    {
        int capSq = to + (color == 0 ? 8 : -8);
        nnue_touchPiece(acc, PAWNNUMBER, them, capSq, -1);
    }
    else if (victim != EMPTYNUMBER)
    {
        nnue_touchPiece(acc, victim, them, to, -1);
    }

    int placedPiece = piece;
    switch (flag) // Promotions
    {
    case 5:
        placedPiece = BISHOPNUMBER;
        break;
    case 6:
        placedPiece = HORSENUMBER;
        break;
    case 7:
        placedPiece = ROOKNUMBER;
        break;
    case 8:
        placedPiece = QUEENNUMBER;
        break;
    }

    bool mirror_flip = false;
    if (piece == KINGNUMBER)
    {
        bool old_mirror = ((from & 7) > 3);
        bool new_mirror = ((to & 7) > 3);
        mirror_flip = (old_mirror != new_mirror);
    }

    if (!mirror_flip)
    {
        // Normal path
        nnue_touchPiece(acc, placedPiece, color, to, +1);

        switch (flag) // Castling
        {
        case 1:
            nnue_touchPiece(acc, ROOKNUMBER, color, H1, -1);
            nnue_touchPiece(acc, ROOKNUMBER, color, F1, +1);
            break;
        case 2:
            nnue_touchPiece(acc, ROOKNUMBER, color, A1, -1);
            nnue_touchPiece(acc, ROOKNUMBER, color, D1, +1);
            break;
        case 4:
            nnue_touchPiece(acc, ROOKNUMBER, color, H8, -1);
            nnue_touchPiece(acc, ROOKNUMBER, color, F8, +1);
            break;
        case 3:
            nnue_touchPiece(acc, ROOKNUMBER, color, A8, -1);
            nnue_touchPiece(acc, ROOKNUMBER, color, D8, +1);
            break;
        }

        return;
    }
    // If mirror flip is true then rebuild the vector
    acc->mirror[color] = ((to & 7) > 3);

    uint64_t pieces[6];
    uint64_t colorBB[2];
    memcpy(pieces, board->pieces, sizeof(pieces));
    memcpy(colorBB, board->color, sizeof(colorBB));

    pieces[piece]  &= ~((uint64_t)1 << from);
    colorBB[color] &= ~((uint64_t)1 << from);

    if (piece == PAWNNUMBER && to == board->epsquare && board->epsquare != -1)
    {
        int capSq = to + (color == 0 ? 8 : -8);
        pieces[PAWNNUMBER] &= ~((uint64_t)1 << capSq);
        colorBB[them]      &= ~((uint64_t)1 << capSq);
    }
    else if (victim != EMPTYNUMBER)
    {
        pieces[victim] &= ~((uint64_t)1 << to);
        colorBB[them]  &= ~((uint64_t)1 << to);
    }

    pieces[placedPiece] |= ((uint64_t)1 << to);
    colorBB[color]      |= ((uint64_t)1 << to);

    switch (flag)
    {
    case 1:
        pieces[ROOKNUMBER] &= ~((uint64_t)1 << H1); colorBB[color] &= ~((uint64_t)1 << H1);
        pieces[ROOKNUMBER] |=  ((uint64_t)1 << F1); colorBB[color] |=  ((uint64_t)1 << F1);
        break;
    case 2:
        pieces[ROOKNUMBER] &= ~((uint64_t)1 << A1); colorBB[color] &= ~((uint64_t)1 << A1);
        pieces[ROOKNUMBER] |=  ((uint64_t)1 << D1); colorBB[color] |=  ((uint64_t)1 << D1);
        break;
    case 4:
        pieces[ROOKNUMBER] &= ~((uint64_t)1 << H8); colorBB[color] &= ~((uint64_t)1 << H8);
        pieces[ROOKNUMBER] |=  ((uint64_t)1 << F8); colorBB[color] |=  ((uint64_t)1 << F8);
        break;
    case 3:
        pieces[ROOKNUMBER] &= ~((uint64_t)1 << A8); colorBB[color] &= ~((uint64_t)1 << A8);
        pieces[ROOKNUMBER] |=  ((uint64_t)1 << D8); colorBB[color] |=  ((uint64_t)1 << D8);
        break;
    }

    for (int h = 0; h < NNUE_HL; h++)
        acc->vector[color][h] = nnue_hiddenBiases[h];

    int otherSide = them;
    for (int internal_piece = 0; internal_piece < 6; internal_piece++)
    {
        uint64_t bb = pieces[internal_piece] & colorBB[color];
        while (bb)
        {
            int sq = pop_lsb(&bb);
            nnue_addFeature(acc->vector[color], nnue_inputIndex(color, 1, internal_piece, sq, acc->mirror[color]));
        }
        bb = pieces[internal_piece] & colorBB[otherSide];
        while (bb)
        {
            int sq = pop_lsb(&bb);
            nnue_addFeature(acc->vector[color], nnue_inputIndex(color, 0, internal_piece, sq, acc->mirror[color]));
        }
    }

    // their mirror didnt change so keep updating that side incrementally
    nnue_touchPiecePerspective(acc, placedPiece, color, to, +1, them);
    switch (flag)
    {
    case 1:
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, H1, -1, them);
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, F1, +1, them);
        break;
    case 2:
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, A1, -1, them);
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, D1, +1, them);
        break;
    case 4:
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, H8, -1, them);
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, F8, +1, them);
        break;
    case 3:
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, A8, -1, them);
        nnue_touchPiecePerspective(acc, ROOKNUMBER, color, D8, +1, them);
        break;
    }

    return;
}

static int nnue_forward(Position *board, int ply)
{
    ply = nnue_clampPly(ply);
    int16_t *accUs   = nnue_stack[ply].vector[board->turn];
    int16_t *accThem = nnue_stack[ply].vector[board->turn ^ 1];

    long long sum = 0;
    for (int h = 0; h < NNUE_HL; h++)
    {
        sum += (long long)nnue_screlu(accUs[h])   * nnue_outputWeights[h];
        sum += (long long)nnue_screlu(accThem[h]) * nnue_outputWeights[NNUE_HL + h];
    }

    sum = sum / HIDDEN_QUANT_SCALE + nnue_outputBias;
    sum = sum * NNUE_SCALE / ((long long)HIDDEN_QUANT_SCALE * NNUE_OUTPUT_SCALE);

    return (int)sum;
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