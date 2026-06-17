#include "eval.h"
#include "lmath.h"
#include "play.h"
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Position piece order:
// pawn   0
// bishop 1
// horse  2
// rook   3
// queen  4
// king   5

enum {
    PAWN   = 0,
    BISHOP = 1,
    HORSE  = 2,
    ROOK   = 3,
    QUEEN  = 4,
    KING   = 5
};

#define FLIP(sq) ((sq) ^ 56)
#define OTHER(side) ((side) ^ 1)

#define FILE_A 0x0101010101010101ULL
#define FILE_H 0x8080808080808080ULL

// Correct order for your engine:
// pawn, bishop, horse, rook, queen, king
int mg_value[6] = { 82, 365, 337, 477, 1025, 0 };
int eg_value[6] = { 94, 297, 281, 512,  936, 0 };

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

int mobility_mg[6] = { 0, 4, 4, 2, 1, 0 };
int mobility_eg[6] = { 0, 3, 2, 2, 1, 0 };

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

// Indexed by pc = 2 * piece + color.
// pawn, bishop, horse, rook, queen, king.
int gamephaseInc[12] = {
    0, 0,
    1, 1,
    1, 1,
    2, 2,
    4, 4,
    0, 0
};

int mg_table[12][64];
int eg_table[12][64];

static inline int file_of(int sq)
{
    return sq & 7;
}

static inline int rank_of(int sq)
{
    return sq >> 3;
}

static inline uint64_t file_mask(int file)
{
    return FILE_A << file;
}

static inline uint64_t pawn_attacks_bb(uint64_t pawns, int side)
{
    if (side == 0)
    {
        return ((pawns & ~FILE_A) << 7) |
               ((pawns & ~FILE_H) << 9);
    }
    else
    {
        return ((pawns & ~FILE_H) >> 7) |
               ((pawns & ~FILE_A) >> 9);
    }
}

void init_tables()
{
    int pc, p, sq;

    for (p = 0, pc = 0; p <= 5; pc += 2, p++)
    {
        for (sq = 0; sq < 64; sq++)
        {
            mg_table[pc][sq]     = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[pc][sq]     = eg_value[p] + eg_pesto_table[p][sq];

            mg_table[pc + 1][sq] = mg_value[p] + mg_pesto_table[p][FLIP(sq)];
            eg_table[pc + 1][sq] = eg_value[p] + eg_pesto_table[p][FLIP(sq)];
        }
    }
}

static int is_passed_pawn(Position *board, int sq, int side)
{
    int f = file_of(sq);
    int r = rank_of(sq);

    uint64_t enemy_pawns = board->pieces[PAWN] & board->color[OTHER(side)];

    for (int df = -1; df <= 1; df++)
    {
        int nf = f + df;

        if (nf < 0 || nf > 7)
            continue;

        uint64_t pawns = enemy_pawns & file_mask(nf);

        while (pawns)
        {
            int esq = __builtin_ctzll(pawns);
            pawns &= pawns - 1;

            int er = rank_of(esq);

            if (side == 0)
            {
                if (er > r)
                    return 0;
            }
            else
            {
                if (er < r)
                    return 0;
            }
        }
    }

    return 1;
}

static void eval_pawn_structure(Position *board, int side, int *mg, int *eg)
{
    static const int passed_mg[8] = { 0, 5, 12, 25, 45, 80, 130, 0 };
    static const int passed_eg[8] = { 0, 10, 25, 50, 85, 140, 220, 0 };

    uint64_t pawns = board->pieces[PAWN] & board->color[side];
    uint64_t original_pawns = pawns;

    int file_count[8] = {0};

    uint64_t temp = pawns;

    while (temp)
    {
        int sq = __builtin_ctzll(temp);
        temp &= temp - 1;

        file_count[file_of(sq)]++;
    }

    for (int f = 0; f < 8; f++)
    {
        if (file_count[f] > 1)
        {
            int extra = file_count[f] - 1;

            *mg -= 10 * extra;
            *eg -= 18 * extra;
        }
    }

    uint64_t own_pawn_attacks = pawn_attacks_bb(original_pawns, side);

    while (pawns)
    {
        int sq = __builtin_ctzll(pawns);
        pawns &= pawns - 1;

        int f = file_of(sq);
        int r = rank_of(sq);
        int rel_rank = side == 0 ? r : 7 - r;

        int has_neighbor =
            (f > 0 && file_count[f - 1] > 0) ||
            (f < 7 && file_count[f + 1] > 0);

        if (!has_neighbor)
        {
            *mg -= 12;
            *eg -= 18;
        }

        if (is_passed_pawn(board, sq, side))
        {
            int mg_bonus = passed_mg[rel_rank];
            int eg_bonus = passed_eg[rel_rank];

            if (own_pawn_attacks & (1ULL << sq))
            {
                mg_bonus += mg_bonus / 4;
                eg_bonus += eg_bonus / 4;
            }

            *mg += mg_bonus;
            *eg += eg_bonus;
        }
    }
}

static void eval_bishop_pair(Position *board, int side, int *mg, int *eg)
{
    uint64_t bishops = board->pieces[BISHOP] & board->color[side];

    if (__builtin_popcountll(bishops) >= 2)
    {
        *mg += 30;
        *eg += 50;
    }
}

static void eval_rook_files(Position *board, int side, int *mg, int *eg)
{
    uint64_t rooks = board->pieces[ROOK] & board->color[side];

    uint64_t own_pawns = board->pieces[PAWN] & board->color[side];
    uint64_t enemy_pawns = board->pieces[PAWN] & board->color[OTHER(side)];

    while (rooks)
    {
        int sq = __builtin_ctzll(rooks);
        rooks &= rooks - 1;

        int f = file_of(sq);
        int r = rank_of(sq);
        int rel_rank = side == 0 ? r : 7 - r;

        uint64_t fm = file_mask(f);

        int own_pawn_on_file = !!(own_pawns & fm);
        int enemy_pawn_on_file = !!(enemy_pawns & fm);

        if (!own_pawn_on_file && !enemy_pawn_on_file)
        {
            *mg += 18;
            *eg += 10;
        }
        else if (!own_pawn_on_file && enemy_pawn_on_file)
        {
            *mg += 10;
            *eg += 6;
        }

        if (rel_rank == 6)
        {
            *mg += 15;
            *eg += 25;
        }
    }
}

static void eval_king_safety(Position *board, int side, int *mg)
{
    uint64_t king_bb = board->pieces[KING] & board->color[side];

    if (!king_bb)
        return;

    int king_sq = __builtin_ctzll(king_bb);

    int kf = file_of(king_sq);
    int kr = rank_of(king_sq);

    uint64_t own_pawns = board->pieces[PAWN] & board->color[side];

    int dir = side == 0 ? 1 : -1;

    int shield = 0;
    int penalty = 0;

    for (int df = -1; df <= 1; df++)
    {
        int f = kf + df;

        if (f < 0 || f > 7)
            continue;

        if (!(own_pawns & file_mask(f)))
            penalty += 10;

        for (int dist = 1; dist <= 2; dist++)
        {
            int r = kr + dir * dist;

            if (r < 0 || r > 7)
                continue;

            int sq = r * 8 + f;

            if (own_pawns & (1ULL << sq))
            {
                shield += dist == 1 ? 12 : 5;
                break;
            }

            if (dist == 1)
                penalty += 4;
        }
    }

    *mg += shield - penalty;
}

int getMobility(Position *board, int piece, int sq, int side, uint64_t enemy_pawn_attacks)
{
    uint64_t occ = board->color[0] | board->color[1];
    uint64_t own = board->color[side];

    uint64_t allowed = ~own & ~enemy_pawn_attacks;

    switch (piece)
    {
        case HORSE:
        {
            uint64_t attacks = knighttable[sq] & allowed;
            return __builtin_popcountll(attacks);
        }

        case BISHOP:
        {
            uint64_t attacks = getbishopAttacks(sq, occ) & allowed;
            return __builtin_popcountll(attacks);
        }

        case ROOK:
        {
            uint64_t attacks = getrookAttacks(sq, occ) & allowed;
            return __builtin_popcountll(attacks);
        }

        case QUEEN:
        {
            uint64_t attacks =
                (getbishopAttacks(sq, occ) |
                 getrookAttacks(sq, occ)) & allowed;

            int mob = __builtin_popcountll(attacks);

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

    uint64_t white_pawns = board->pieces[PAWN] & board->color[0];
    uint64_t black_pawns = board->pieces[PAWN] & board->color[1];

    uint64_t pawn_attacks[2];

    pawn_attacks[0] = pawn_attacks_bb(white_pawns, 0);
    pawn_attacks[1] = pawn_attacks_bb(black_pawns, 1);

    for (int piece = 0; piece < 6; piece++)
    {
        uint64_t bb = board->pieces[piece] & board->color[0];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2 * piece;

            int mob = getMobility(board, piece, sq, 0, pawn_attacks[1]);

            mg[0] += mg_table[pc][sq];
            eg[0] += eg_table[pc][sq];

            mg[0] += mob * mobility_mg[piece];
            eg[0] += mob * mobility_eg[piece];

            gamePhase += gamephaseInc[pc];
        }

        bb = board->pieces[piece] & board->color[1];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int pc = 2 * piece + 1;

            int mob = getMobility(board, piece, sq, 1, pawn_attacks[0]);

            mg[1] += mg_table[pc][sq];
            eg[1] += eg_table[pc][sq];

            mg[1] += mob * mobility_mg[piece];
            eg[1] += mob * mobility_eg[piece];

            gamePhase += gamephaseInc[pc];
        }
    }

    for (int side = 0; side < 2; side++)
    {
        eval_pawn_structure(board, side, &mg[side], &eg[side]);
        eval_bishop_pair(board, side, &mg[side], &eg[side]);
        eval_rook_files(board, side, &mg[side], &eg[side]);
        eval_king_safety(board, side, &mg[side]);
    }

    if (gamePhase > 24)
        gamePhase = 24;

    int egPhase = 24 - gamePhase;

    // Side-to-move relative eval.
    // This is correct for negamax.
    int mgScore = mg[board->turn] - mg[OTHER(board->turn)];
    int egScore = eg[board->turn] - eg[OTHER(board->turn)];

    int score = (mgScore * gamePhase + egScore * egPhase) / 24;

    return score;
}