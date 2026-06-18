#include "eval.h"
#include "lmath.h"
#include "stdio.h"
#include "play.h"
#include <stdio.h>
#include <stdint.h>
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

typedef enum
{
    PAWNVAL = 1,
    BISHOPVAL = 3,
    KNIGHTVAL = 3,
    ROOKVAL = 5,
    QUEENVAL = 9,
    KINGVAL = 11,
} pieceVal;

#define PCOLOR(p) ((p)&1)

#define FLIP(sq) ((sq)^56)
#define OTHER(side) ((side)^1)

// =============================
// Eval feature switches
// =============================

// Original eval parts
#define USE_BASE_PESTO      1
#define USE_MOBILITY        1
#define USE_TAPERED_EVAL    1

// Added features
#define USE_BISHOP_PAIR     0
#define USE_ISOLATED_PAWNS  1
#define USE_DOUBLED_PAWNS   1
#define USE_ROOK_FILES      0

// =============================
// Added feature values
// =============================

#define BISHOP_PAIR_MG 25
#define BISHOP_PAIR_EG 35

#define ISOLATED_PAWN_MG 8
#define ISOLATED_PAWN_EG 12

#define DOUBLED_PAWN_MG 8
#define DOUBLED_PAWN_EG 14

#define ROOK_OPEN_FILE_MG 18
#define ROOK_OPEN_FILE_EG 12

#define ROOK_SEMI_OPEN_FILE_MG 8
#define ROOK_SEMI_OPEN_FILE_EG 6

int mg_value[6] = { 82, 337, 365, 477, 1025,  0};
int eg_value[6] = { 94, 281, 297, 512,  936,  0};

int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int mobility_mg[6] = {0, 4, 4, 2, 1, 0};
int mobility_eg[6] = {0, 2, 3, 2, 1, 0};

int* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_bishop_table,
    mg_knight_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_bishop_table,
    eg_knight_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

int gamephaseInc[12] = {0,0,1,1,1,1,2,2,4,4,0,0};
int mg_table[12][64];
int eg_table[12][64];

#if USE_ISOLATED_PAWNS || USE_DOUBLED_PAWNS || USE_ROOK_FILES
static uint64_t eval_file_mask[8];
#endif

void init_tables()
{
#if USE_ISOLATED_PAWNS || USE_DOUBLED_PAWNS || USE_ROOK_FILES
    for (int file = 0; file < 8; file++) {
        eval_file_mask[file] = 0ULL;

        for (int rank = 0; rank < 8; rank++) {
            int sq = rank * 8 + file;
            eval_file_mask[file] |= 1ULL << sq;
        }
    }
#endif

    int pc, p, sq;
    for (p = 0, pc = 0; p <= 5; pc += 2, p++) {
        for (sq = 0; sq < 64; sq++) {
            mg_table[pc][sq] = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[pc][sq] = eg_value[p] + eg_pesto_table[p][sq];

            mg_table[pc + 1][sq] = mg_value[p] + mg_pesto_table[p][FLIP(sq)];
            eg_table[pc + 1][sq] = eg_value[p] + eg_pesto_table[p][FLIP(sq)];
        }
    }
}

int getMobility(Position *board, int piece, int sq, int side)
{
    uint64_t occ = board->color[0] | board->color[1];
    uint64_t own = board->color[side];

    switch (piece)
    {
        case 2: { // knight
            uint64_t attacks = knighttable[sq] & ~own;
            return __builtin_popcountll(attacks);
        }

        case 1: { // bishop
            uint64_t attacks = getbishopAttacks(sq, occ) & ~own;
            return __builtin_popcountll(attacks);
        }

        case 3: { // rook
            uint64_t attacks = getrookAttacks(sq, occ) & ~own;
            return __builtin_popcountll(attacks);
        }

        case 4: { // queen
            uint64_t attacks =
                (getbishopAttacks(sq, occ) |
                 getrookAttacks(sq, occ)) & ~own;

            int mob = __builtin_popcountll(attacks);

            return mob > 14 ? 14 : mob;
        }

        default:
            return 0;
    }
}

static void addBishopPair(Position *board, int side, int *mgScore, int *egScore)
{
#if USE_BISHOP_PAIR
    uint64_t bishops = board->pieces[1] & board->color[side];

    if (__builtin_popcountll(bishops) >= 2) {
        *mgScore += BISHOP_PAIR_MG;
        *egScore += BISHOP_PAIR_EG;
    }
#else
    (void)board;
    (void)side;
    (void)mgScore;
    (void)egScore;
#endif
}

static void addIsolatedPawns(Position *board, int side, int *mgScore, int *egScore)
{
#if USE_ISOLATED_PAWNS
    uint64_t pawns = board->pieces[0] & board->color[side];
    uint64_t allPawns = pawns;

    while (pawns)
    {
        int sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        int file = sq & 7;
        uint64_t adjacentFiles = 0ULL;

        if (file > 0)
            adjacentFiles |= eval_file_mask[file - 1];

        if (file < 7)
            adjacentFiles |= eval_file_mask[file + 1];

        if ((allPawns & adjacentFiles) == 0) {
            *mgScore -= ISOLATED_PAWN_MG;
            *egScore -= ISOLATED_PAWN_EG;
        }
    }
#else
    (void)board;
    (void)side;
    (void)mgScore;
    (void)egScore;
#endif
}

static void addDoubledPawns(Position *board, int side, int *mgScore, int *egScore)
{
#if USE_DOUBLED_PAWNS
    uint64_t pawns = board->pieces[0] & board->color[side];

    for (int file = 0; file < 8; file++) {
        int count = __builtin_popcountll(pawns & eval_file_mask[file]);

        if (count > 1) {
            int extraPawns = count - 1;

            *mgScore -= DOUBLED_PAWN_MG * extraPawns;
            *egScore -= DOUBLED_PAWN_EG * extraPawns;
        }
    }
#else
    (void)board;
    (void)side;
    (void)mgScore;
    (void)egScore;
#endif
}

static void addRookFileBonus(Position *board, int side, int sq, int *mgScore, int *egScore)
{
#if USE_ROOK_FILES
    int file = sq & 7;
    uint64_t fileMask = eval_file_mask[file];

    uint64_t ownPawns = board->pieces[0] & board->color[side];
    uint64_t enemyPawns = board->pieces[0] & board->color[OTHER(side)];

    if ((ownPawns & fileMask) == 0) {
        if ((enemyPawns & fileMask) == 0) {
            *mgScore += ROOK_OPEN_FILE_MG;
            *egScore += ROOK_OPEN_FILE_EG;
        } else {
            *mgScore += ROOK_SEMI_OPEN_FILE_MG;
            *egScore += ROOK_SEMI_OPEN_FILE_EG;
        }
    }
#else
    (void)board;
    (void)side;
    (void)sq;
    (void)mgScore;
    (void)egScore;
#endif
}

int eval(Position *board)
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int gamePhase = 0;

    for (int piece = 0; piece < 6; piece++)
    {
        // White pieces
        uint64_t bb = board->pieces[piece] & board->color[0];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2 * piece + 0;

#if USE_MOBILITY
            int mob = getMobility(board, piece, sq, 0);

            mg[0] += mob * mobility_mg[piece];
            eg[0] += mob * mobility_eg[piece];
#endif

#if USE_BASE_PESTO
            mg[0] += mg_table[pc][sq];
            eg[0] += eg_table[pc][sq];
#endif

#if USE_ROOK_FILES
            if (piece == 3) {
                addRookFileBonus(board, 0, sq, &mg[0], &eg[0]);
            }
#endif

            gamePhase += gamephaseInc[pc];
        }

        // Black pieces
        bb = board->pieces[piece] & board->color[1];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2 * piece + 1;

#if USE_MOBILITY
            int mob = getMobility(board, piece, sq, 1);

            mg[1] += mob * mobility_mg[piece];
            eg[1] += mob * mobility_eg[piece];
#endif

#if USE_BASE_PESTO
            mg[1] += mg_table[pc][sq];
            eg[1] += eg_table[pc][sq];
#endif

#if USE_ROOK_FILES
            if (piece == 3) {
                addRookFileBonus(board, 1, sq, &mg[1], &eg[1]);
            }
#endif

            gamePhase += gamephaseInc[pc];
        }
    }

#if USE_ISOLATED_PAWNS
    addIsolatedPawns(board, 0, &mg[0], &eg[0]);
    addIsolatedPawns(board, 1, &mg[1], &eg[1]);
#endif

#if USE_DOUBLED_PAWNS
    addDoubledPawns(board, 0, &mg[0], &eg[0]);
    addDoubledPawns(board, 1, &mg[1], &eg[1]);
#endif

#if USE_BISHOP_PAIR
    addBishopPair(board, 0, &mg[0], &eg[0]);
    addBishopPair(board, 1, &mg[1], &eg[1]);
#endif

    int mgScore = mg[board->turn] - mg[OTHER(board->turn)];
    int egScore = eg[board->turn] - eg[OTHER(board->turn)];

#if USE_TAPERED_EVAL
    if (gamePhase > 24)
        gamePhase = 24;

    int egPhase = 24 - gamePhase;
    int base_eval = (mgScore * gamePhase + egScore * egPhase) / 24;

    return base_eval;
#else
    return mgScore;
#endif
}