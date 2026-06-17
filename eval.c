#include "eval.h"
#include "lmath.h"
#include "stdio.h"
#include "play.h"
#include <stdio.h>
#include <stdint.h>
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

// #define ILLEGALMOVE 42 // Answer to the universe

// static uint8_t penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

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
#define OTHER(side) ((side)^ 1)

#define EVAL_BIT(sq) (1ULL << (sq))
#define EVAL_FILE_OF(sq) ((sq) & 7)
#define EVAL_RANK_OF(sq) ((sq) >> 3)

enum
{
    EVAL_PAWN = 0,
    EVAL_BISHOP = 1,
    EVAL_KNIGHT = 2,
    EVAL_ROOK = 3,
    EVAL_QUEEN = 4,
    EVAL_KING = 5
};

static const uint64_t eval_file_mask[8] = {
    0x0101010101010101ULL,
    0x0202020202020202ULL,
    0x0404040404040404ULL,
    0x0808080808080808ULL,
    0x1010101010101010ULL,
    0x2020202020202020ULL,
    0x4040404040404040ULL,
    0x8080808080808080ULL
};

#define EVAL_FILE_A eval_file_mask[0]
#define EVAL_FILE_H eval_file_mask[7]

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

static inline int eval_relrank(int sq, int side)
{
    return side == 0 ? EVAL_RANK_OF(sq) : 7 - EVAL_RANK_OF(sq);
}

static inline uint64_t eval_pawn_attacks(uint64_t pawns, int side)
{
    if (side == 0)
        return ((pawns & ~EVAL_FILE_A) << 7) | ((pawns & ~EVAL_FILE_H) << 9);
    else
        return ((pawns & ~EVAL_FILE_H) >> 7) | ((pawns & ~EVAL_FILE_A) >> 9);
}

static int eval_king_sq(Position *board, int side)
{
    uint64_t k = board->pieces[EVAL_KING] & board->color[side];
    return k ? __builtin_ctzll(k) : -1;
}

static uint64_t eval_passed_mask(int sq, int side)
{
    uint64_t mask = 0ULL;
    int f = EVAL_FILE_OF(sq);
    int r = EVAL_RANK_OF(sq);

    if (side == 0) {
        for (int rr = r + 1; rr < 8; rr++) {
            for (int df = -1; df <= 1; df++) {
                int ff = f + df;
                if (ff >= 0 && ff < 8)
                    mask |= EVAL_BIT(rr * 8 + ff);
            }
        }
    } else {
        for (int rr = r - 1; rr >= 0; rr--) {
            for (int df = -1; df <= 1; df++) {
                int ff = f + df;
                if (ff >= 0 && ff < 8)
                    mask |= EVAL_BIT(rr * 8 + ff);
            }
        }
    }

    return mask;
}

static uint64_t eval_king_ring(int sq)
{
    uint64_t mask = 0ULL;
    int f = EVAL_FILE_OF(sq);
    int r = EVAL_RANK_OF(sq);

    for (int df = -1; df <= 1; df++) {
        for (int dr = -1; dr <= 1; dr++) {
            if (df == 0 && dr == 0) continue;

            int ff = f + df;
            int rr = r + dr;

            if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
                mask |= EVAL_BIT(rr * 8 + ff);
        }
    }

    return mask;
}

static void eval_features(Position *board, int mg[2], int eg[2])
{
    uint64_t occ = board->color[0] | board->color[1];
    uint64_t allPawns = board->pieces[EVAL_PAWN];

    static const int passedMg[8] = {0, 5, 10, 20, 35, 60, 100, 0};
    static const int passedEg[8] = {0, 10, 20, 40, 75, 130, 200, 0};

    for (int side = 0; side < 2; side++) {
        int enemy = side ^ 1;

        uint64_t pawns = board->pieces[EVAL_PAWN] & board->color[side];
        uint64_t enemyPawns = board->pieces[EVAL_PAWN] & board->color[enemy];

        uint64_t ownPawnAttacks = eval_pawn_attacks(pawns, side);
        uint64_t enemyPawnAttacks = eval_pawn_attacks(enemyPawns, enemy);

        uint64_t bb = pawns;

        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int file = EVAL_FILE_OF(sq);
            int rr = eval_relrank(sq, side);
            uint64_t b = EVAL_BIT(sq);

            uint64_t sameFile = pawns & eval_file_mask[file];

            if (__builtin_popcountll(sameFile) > 1) {
                mg[side] -= 8;
                eg[side] -= 12;
            }

            uint64_t adjacentFiles = 0ULL;
            if (file > 0) adjacentFiles |= eval_file_mask[file - 1];
            if (file < 7) adjacentFiles |= eval_file_mask[file + 1];

            if (!(pawns & adjacentFiles)) {
                mg[side] -= 12;
                eg[side] -= 14;
            }

            if (ownPawnAttacks & b) {
                mg[side] += 5;
                eg[side] += 8;
            }

            if (!(enemyPawns & eval_passed_mask(sq, side))) {
                int bonusMg = passedMg[rr];
                int bonusEg = passedEg[rr];

                int front = side == 0 ? sq + 8 : sq - 8;

                if (front >= 0 && front < 64 && (occ & EVAL_BIT(front))) {
                    bonusMg /= 2;
                    bonusEg /= 2;
                }

                mg[side] += bonusMg;
                eg[side] += bonusEg;
            }
        }

        uint64_t bishops = board->pieces[EVAL_BISHOP] & board->color[side];

        if (__builtin_popcountll(bishops) >= 2) {
            mg[side] += 30;
            eg[side] += 45;
        }

        uint64_t knights = board->pieces[EVAL_KNIGHT] & board->color[side];
        bb = knights;

        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            uint64_t b = EVAL_BIT(sq);
            int rr = eval_relrank(sq, side);

            if (rr >= 3 && rr <= 5 &&
                (ownPawnAttacks & b) &&
                !(enemyPawnAttacks & b)) {
                mg[side] += 22;
                eg[side] += 10;
            }
        }

        bb = bishops;

        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int bishopColor = (EVAL_FILE_OF(sq) + EVAL_RANK_OF(sq)) & 1;
            int blocked = 0;

            uint64_t pp = pawns;

            while (pp) {
                int psq = __builtin_ctzll(pp);
                pp &= pp - 1;

                if (((EVAL_FILE_OF(psq) + EVAL_RANK_OF(psq)) & 1) == bishopColor)
                    blocked++;
            }

            mg[side] -= blocked * 2;
            eg[side] -= blocked * 3;
        }

        uint64_t rooks = board->pieces[EVAL_ROOK] & board->color[side];
        bb = rooks;

        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int file = EVAL_FILE_OF(sq);
            int rr = eval_relrank(sq, side);

            uint64_t fm = eval_file_mask[file];

            if (!(allPawns & fm)) {
                mg[side] += 18;
                eg[side] += 12;
            } else if (!(pawns & fm)) {
                mg[side] += 10;
                eg[side] += 8;
            }

            if (rr == 6) {
                mg[side] += 18;
                eg[side] += 28;
            }
        }

        int ksq = eval_king_sq(board, side);

        if (ksq != -1) {
            int kf = EVAL_FILE_OF(ksq);
            int kr = EVAL_RANK_OF(ksq);
            int dir = side == 0 ? 1 : -1;

            if ((side == 0 && kr <= 1 && kf >= 2 && kf <= 5) ||
                (side == 1 && kr >= 6 && kf >= 2 && kf <= 5)) {
                mg[side] -= 25;
            }

            for (int df = -1; df <= 1; df++) {
                int f = kf + df;
                if (f < 0 || f > 7) continue;

                int shieldRank = kr + dir;

                if (shieldRank >= 0 && shieldRank < 8) {
                    if (pawns & EVAL_BIT(shieldRank * 8 + f))
                        mg[side] += 10;
                    else
                        mg[side] -= 10;
                }

                if (!(pawns & eval_file_mask[f]))
                    mg[side] -= 8;

                if (!(allPawns & eval_file_mask[f]))
                    mg[side] -= 10;
            }

            uint64_t ring = eval_king_ring(ksq);
            int attacks = 0;

            uint64_t enemyKnights = board->pieces[EVAL_KNIGHT] & board->color[enemy];
            while (enemyKnights) {
                int sq = __builtin_ctzll(enemyKnights);
                enemyKnights &= enemyKnights - 1;
                attacks += 2 * __builtin_popcountll(knighttable[sq] & ring);
            }

            uint64_t enemyBishops = board->pieces[EVAL_BISHOP] & board->color[enemy];
            while (enemyBishops) {
                int sq = __builtin_ctzll(enemyBishops);
                enemyBishops &= enemyBishops - 1;
                attacks += 2 * __builtin_popcountll(getbishopAttacks(sq, occ) & ring);
            }

            uint64_t enemyRooks = board->pieces[EVAL_ROOK] & board->color[enemy];
            while (enemyRooks) {
                int sq = __builtin_ctzll(enemyRooks);
                enemyRooks &= enemyRooks - 1;
                attacks += 3 * __builtin_popcountll(getrookAttacks(sq, occ) & ring);
            }

            uint64_t enemyQueens = board->pieces[EVAL_QUEEN] & board->color[enemy];
            while (enemyQueens) {
                int sq = __builtin_ctzll(enemyQueens);
                enemyQueens &= enemyQueens - 1;

                uint64_t qatt = getbishopAttacks(sq, occ) | getrookAttacks(sq, occ);
                attacks += 4 * __builtin_popcountll(qatt & ring);
            }

            if (!(board->pieces[EVAL_QUEEN] & board->color[enemy]))
                attacks /= 2;

            mg[side] -= attacks * 3;
            eg[side] -= attacks;
        }
    }
}

void init_tables()
{
    int pc, p, sq;
    for (p = 0, pc = 0; p <= 5; pc += 2, p++) {
        for (sq = 0; sq < 64; sq++) {
            mg_table[pc]  [sq] = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[pc]  [sq] = eg_value[p] + eg_pesto_table[p][sq];
            mg_table[pc+1][sq] = mg_value[p] + mg_pesto_table[p][FLIP(sq)];
            eg_table[pc+1][sq] = eg_value[p] + eg_pesto_table[p][FLIP(sq)];
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

            // prevent eval explosion
            return mob > 14 ? 14 : mob;
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

    for (int piece = 0; piece < 6; piece++)
    {
        // white pieces
        uint64_t bb = board->pieces[piece] & board->color[0];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2*piece + 0;
            int mob = getMobility(board, piece, sq, 0);
            mg[0] += mob * mobility_mg[piece];
            eg[0] += mob * mobility_eg[piece];
            mg[0] += mg_table[pc][sq];
            eg[0] += eg_table[pc][sq];
            gamePhase += gamephaseInc[pc];
        }

        // black pieces
        bb = board->pieces[piece] & board->color[1];
        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2*piece + 1;
            int mob = getMobility(board, piece, sq, 1);
            mg[1] += mob * mobility_mg[piece];
            eg[1] += mob * mobility_eg[piece];
            mg[1] += mg_table[pc][sq];
            eg[1] += eg_table[pc][sq];
            gamePhase += gamephaseInc[pc];
        }
    }

    eval_features(board, mg, eg);

    // Tapered eval
    int mgScore = mg[board->turn] - mg[!board->turn];
    int egScore = eg[board->turn] - eg[!board->turn];

    if (gamePhase > 24) gamePhase = 24; // cap in case of promotions
    int egPhase = 24 - gamePhase;
    int base_eval = (mgScore * gamePhase + egScore * egPhase) / 24;

    // Tempo bonus because the score is from side-to-move perspective.
    return base_eval + 10;
}