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

#define EVAL_FILE_A 0x0101010101010101ULL
#define EVAL_FILE_H 0x8080808080808080ULL
#define EVAL_NOT_FILE_A (~EVAL_FILE_A)
#define EVAL_NOT_FILE_H (~EVAL_FILE_H)

#define TEMPO_BONUS 10

#define BISHOP_PAIR_MG 30
#define BISHOP_PAIR_EG 35

#define ISOLATED_PAWN_MG 12
#define ISOLATED_PAWN_EG 16

#define DOUBLED_PAWN_MG 10
#define DOUBLED_PAWN_EG 18

#define CONNECTED_PAWN_MG 8
#define CONNECTED_PAWN_EG 12

#define ROOK_OPEN_FILE_MG 25
#define ROOK_OPEN_FILE_EG 20

#define ROOK_SEMI_OPEN_FILE_MG 12
#define ROOK_SEMI_OPEN_FILE_EG 10

int mg_value[6] = {82, 337, 365, 477, 1025, 0};
int eg_value[6] = {94, 281, 297, 512, 936, 0};

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

static uint64_t eval_file_mask[8];
static uint64_t passed_mask_white[64];
static uint64_t passed_mask_black[64];

static int knightMobMg[9] = {
    -40, -25, -15, -5, 5, 12, 18, 22, 25
};

static int knightMobEg[9] = {
    -30, -18, -10, -2, 5, 10, 14, 17, 20
};

static int bishopMobMg[14] = {
    -30, -20, -12, -5, 4, 10, 16, 21, 25, 28, 30, 32, 33, 34
};

static int bishopMobEg[14] = {
    -25, -16, -8, -2, 5, 11, 16, 20, 23, 25, 27, 28, 29, 30
};

static int rookMobMg[15] = {
    -20, -15, -10, -5, 0, 5, 9, 13, 16, 19, 21, 23, 24, 25, 26
};

static int rookMobEg[15] = {
    -15, -10, -6, -2, 2, 6, 10, 13, 16, 18, 20, 21, 22, 23, 24
};

static int queenMobMg[28] = {
    -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 25, 25, 25, 25
};

static int queenMobEg[28] = {
    -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 21, 22, 22, 23, 23, 24, 24, 24
};

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int abs_int(int x)
{
    return x < 0 ? -x : x;
}

static int kingDistance(int a, int b)
{
    int af = a & 7;
    int ar = a >> 3;

    int bf = b & 7;
    int br = b >> 3;

    return max_int(abs_int(af - bf), abs_int(ar - br));
}

static int getKingSq(Position *board, int side)
{
    uint64_t king = board->pieces[5] & board->color[side];

    if (!king)
        return -1;

    return __builtin_ctzll(king);
}

static void init_eval_masks()
{
    for (int file = 0; file < 8; file++) {
        eval_file_mask[file] = 0ULL;

        for (int rank = 0; rank < 8; rank++) {
            int sq = rank * 8 + file;
            eval_file_mask[file] |= 1ULL << sq;
        }
    }

    for (int sq = 0; sq < 64; sq++) {
        int file = sq & 7;
        int rank = sq >> 3;

        passed_mask_white[sq] = 0ULL;
        passed_mask_black[sq] = 0ULL;

        for (int df = -1; df <= 1; df++) {
            int f = file + df;

            if (f < 0 || f > 7)
                continue;

            for (int r = rank + 1; r < 8; r++)
                passed_mask_white[sq] |= 1ULL << (r * 8 + f);

            for (int r = rank - 1; r >= 0; r--)
                passed_mask_black[sq] |= 1ULL << (r * 8 + f);
        }
    }
}

static uint64_t pawnAttacksBySide(uint64_t pawns, int side)
{
    if (side == 0) {
        return ((pawns << 7) & EVAL_NOT_FILE_H) |
               ((pawns << 9) & EVAL_NOT_FILE_A);
    } else {
        return ((pawns >> 7) & EVAL_NOT_FILE_A) |
               ((pawns >> 9) & EVAL_NOT_FILE_H);
    }
}

static uint64_t enemyPawnAttacks(Position *board, int side)
{
    uint64_t enemyPawns = board->pieces[0] & board->color[OTHER(side)];
    return pawnAttacksBySide(enemyPawns, OTHER(side));
}

static int mobilityBonusMg(int piece, int mob)
{
    switch (piece) {
        case 2:
            if (mob > 8) mob = 8;
            return knightMobMg[mob];

        case 1:
            if (mob > 13) mob = 13;
            return bishopMobMg[mob];

        case 3:
            if (mob > 14) mob = 14;
            return rookMobMg[mob];

        case 4:
            if (mob > 27) mob = 27;
            return queenMobMg[mob];

        default:
            return 0;
    }
}

static int mobilityBonusEg(int piece, int mob)
{
    switch (piece) {
        case 2:
            if (mob > 8) mob = 8;
            return knightMobEg[mob];

        case 1:
            if (mob > 13) mob = 13;
            return bishopMobEg[mob];

        case 3:
            if (mob > 14) mob = 14;
            return rookMobEg[mob];

        case 4:
            if (mob > 27) mob = 27;
            return queenMobEg[mob];

        default:
            return 0;
    }
}

static int bishopPair(Position *board, int side, int endgame)
{
    uint64_t bishops = board->pieces[1] & board->color[side];

    if (__builtin_popcountll(bishops) >= 2)
        return endgame ? BISHOP_PAIR_EG : BISHOP_PAIR_MG;

    return 0;
}

static void pawnStructure(Position *board, int side, int *mgScore, int *egScore)
{
    uint64_t pawns = board->pieces[0] & board->color[side];
    uint64_t enemyPawns = board->pieces[0] & board->color[OTHER(side)];
    uint64_t friendlyPawns = pawns;
    uint64_t friendlyPawnAttacks = pawnAttacksBySide(friendlyPawns, side);

    int friendlyKing = getKingSq(board, side);
    int enemyKing = getKingSq(board, OTHER(side));

    while (pawns)
    {
        int sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        int file = sq & 7;
        int rank = sq >> 3;
        int relativeRank = side == 0 ? rank : 7 - rank;

        uint64_t sameFile = eval_file_mask[file];
        uint64_t adjacentFiles = 0ULL;

        if (file > 0)
            adjacentFiles |= eval_file_mask[file - 1];

        if (file < 7)
            adjacentFiles |= eval_file_mask[file + 1];

        if ((friendlyPawns & adjacentFiles) == 0) {
            *mgScore -= ISOLATED_PAWN_MG;
            *egScore -= ISOLATED_PAWN_EG;
        }

        if (__builtin_popcountll(friendlyPawns & sameFile) > 1) {
            *mgScore -= DOUBLED_PAWN_MG;
            *egScore -= DOUBLED_PAWN_EG;
        }

        if (friendlyPawnAttacks & (1ULL << sq)) {
            *mgScore += CONNECTED_PAWN_MG;
            *egScore += CONNECTED_PAWN_EG;
        }

        uint64_t passMask = side == 0 ? passed_mask_white[sq] : passed_mask_black[sq];

        if ((enemyPawns & passMask) == 0) {
            int passedMg = 8 + relativeRank * relativeRank * 2;
            int passedEg = 15 + relativeRank * relativeRank * 5;

            int forwardSq = side == 0 ? sq + 8 : sq - 8;
            uint64_t occ = board->color[0] | board->color[1];

            if (forwardSq >= 0 && forwardSq < 64) {
                if (occ & (1ULL << forwardSq)) {
                    passedMg -= 10;
                    passedEg -= 20;
                }
            }

            if (friendlyKing != -1 && enemyKing != -1) {
                int friendlyDist = kingDistance(friendlyKing, sq);
                int enemyDist = kingDistance(enemyKing, sq);

                passedEg += enemyDist * 4;
                passedEg -= friendlyDist * 2;
            }

            *mgScore += passedMg;
            *egScore += passedEg;
        }
    }
}

static void rookFileBonus(Position *board, int side, int sq, int *mgScore, int *egScore)
{
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
}

static int kingShield(Position *board, int side)
{
    uint64_t king = board->pieces[5] & board->color[side];

    if (!king)
        return 0;

    int ksq = __builtin_ctzll(king);
    int file = ksq & 7;
    int rank = ksq >> 3;

    uint64_t pawns = board->pieces[0] & board->color[side];

    int score = 0;
    int forward = side == 0 ? 1 : -1;

    for (int df = -1; df <= 1; df++) {
        int f = file + df;
        int r = rank + forward;

        if (f < 0 || f > 7 || r < 0 || r > 7)
            continue;

        int sq = r * 8 + f;

        if (pawns & (1ULL << sq))
            score += 10;
        else
            score -= 8;
    }

    return score;
}

void init_tables()
{
    init_eval_masks();

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
    uint64_t unsafe = enemyPawnAttacks(board, side);

    switch (piece)
    {
        case 2: { // knight
            uint64_t attacks = knighttable[sq] & ~own;
            attacks &= ~unsafe;
            return __builtin_popcountll(attacks);
        }

        case 1: { // bishop
            uint64_t attacks = getbishopAttacks(sq, occ) & ~own;
            attacks &= ~unsafe;
            return __builtin_popcountll(attacks);
        }

        case 3: { // rook
            uint64_t attacks = getrookAttacks(sq, occ) & ~own;
            attacks &= ~unsafe;
            return __builtin_popcountll(attacks);
        }

        case 4: { // queen
            uint64_t attacks =
                (getbishopAttacks(sq, occ) |
                 getrookAttacks(sq, occ)) & ~own;

            attacks &= ~unsafe;

            int mob = __builtin_popcountll(attacks);

            return mob > 27 ? 27 : mob;
        }

        default:
            return 0;
    }
}

int eval(Position *board)
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int gamePhase = 0;

    for (int side = 0; side < 2; side++)
    {
        for (int piece = 0; piece < 6; piece++)
        {
            uint64_t bb = board->pieces[piece] & board->color[side];

            while (bb)
            {
                int sq = __builtin_ctzll(bb);
                bb &= bb - 1;

                int pc = 2 * piece + side;

                int mob = getMobility(board, piece, sq, side);

                mg[side] += mg_table[pc][sq];
                eg[side] += eg_table[pc][sq];

                mg[side] += mobilityBonusMg(piece, mob);
                eg[side] += mobilityBonusEg(piece, mob);

                if (piece == 3) {
                    rookFileBonus(board, side, sq, &mg[side], &eg[side]);
                }

                gamePhase += gamephaseInc[pc];
            }
        }

        pawnStructure(board, side, &mg[side], &eg[side]);

        mg[side] += bishopPair(board, side, 0);
        eg[side] += bishopPair(board, side, 1);

        mg[side] += kingShield(board, side);
    }

    int mgScore = mg[board->turn] - mg[OTHER(board->turn)];
    int egScore = eg[board->turn] - eg[OTHER(board->turn)];

    if (gamePhase > 24)
        gamePhase = 24;

    int egPhase = 24 - gamePhase;

    int base_eval = (mgScore * gamePhase + egScore * egPhase) / 24;

    base_eval += TEMPO_BONUS;

    return base_eval;
}