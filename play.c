#include "play.h"
#include "inttypes.h"
#include "lmath.h"
#include "magics.h"
#include "precomputed.h"
#include "uci.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    uint64_t pawns;
    uint64_t knights;
    uint64_t bishopsQueens;
    uint64_t rooksQueens;
    uint64_t king;
} AttackSet;

static inline AttackSet buildAttackSet(Position *b, int enemy)
{
    uint64_t enemyPieces = b->color[enemy];

    AttackSet a;
    a.pawns = b->pieces[PAWNNUMBER] & enemyPieces;
    a.knights = b->pieces[HORSENUMBER] & enemyPieces;
    a.bishopsQueens =
        (b->pieces[BISHOPNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces;
    a.rooksQueens =
        (b->pieces[ROOKNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces;
    a.king = b->pieces[KINGNUMBER] & enemyPieces;

    return a;
}

static uint64_t ray_between_table[64 * 64];

void init_between_bb(void)
{
    for (int sq1 = 0; sq1 < 64; sq1++)
    {
        int r1 = sq1 >> 3;
        int f1 = sq1 & 7;

        for (int sq2 = 0; sq2 < 64; sq2++)
        {
            int r2 = sq2 >> 3;
            int f2 = sq2 & 7;
            int dr = r2 - r1;
            int df = f2 - f1;

            uint64_t bb = 0;

            if (dr == 0)
            {
                if (df != 0)
                {
                    int dir = (df > 0) ? 1 : -1;
                    for (int f = f1 + dir; f != f2; f += dir)
                        bb |= 1ULL << (r1 * 8 + f);
                }
            }
            else if (df == 0)
            {
                int dir = (dr > 0) ? 1 : -1;
                for (int r = r1 + dir; r != r2; r += dir)
                    bb |= 1ULL << (r * 8 + f1);
            }
            else if (dr == df || dr == -df)
            {
                int step_r = (dr > 0) ? 1 : -1;
                int step_f = (df > 0) ? 1 : -1;
                int r = r1 + step_r;
                int f = f1 + step_f;
                while (r != r2)
                {
                    bb |= 1ULL << (r * 8 + f);
                    r += step_r;
                    f += step_f;
                }
            }

            ray_between_table[sq1 * 64 + sq2] = bb;
        }
    }
}

static uint8_t castling_mask[64];

void init_castling_mask(void)
{
    for (int i = 0; i < 64; i++)
        castling_mask[i] = 0xFF;
    castling_mask[A1] &= ~(1U << WHITE_QUEENSIDE);
    castling_mask[H1] &= ~(1U << WHITE_KINGSIDE);
    castling_mask[E1] &= ~((1U << WHITE_KINGSIDE) | (1U << WHITE_QUEENSIDE));
    castling_mask[A8] &= ~(1U << BLACK_QUEENSIDE);
    castling_mask[H8] &= ~(1U << BLACK_KINGSIDE);
    castling_mask[E8] &= ~((1U << BLACK_KINGSIDE) | (1U << BLACK_QUEENSIDE));
}

static inline bool squareAttacked_fast(int sq, int enemy, uint64_t occ,
                                       const AttackSet *a)
{
    if ((enemy == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
        a->pawns)
        return true;

    if (knighttable[sq] & a->knights)
        return true;

    if (kingtable[sq] & a->king)
        return true;

    if (getBishopAttacks(sq, occ) & a->bishopsQueens)
        return true;

    if (getRookAttacks(sq, occ) & a->rooksQueens)
        return true;

    return false;
}

#define ADD_MOVE(list, from, to) \
    ((list)->movelist[(list)->offset++] = ((uint16_t)(((from) << 6) | (to))))

#define ADD_FLAG_MOVE(list, flag, from, to) \
    ((list)->movelist[(list)->offset++] =   \
         ((uint16_t)(((flag) << 12) | ((from) << 6) | (to))))

bool squareAttacked(Position *b, int sq, int enemy)
{

    uint64_t occ = b->color[0] | b->color[1];
    uint64_t enemyPieces = b->color[enemy];

    if ((enemy == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
        (b->pieces[PAWNNUMBER] & enemyPieces))
        return true;

    if (knighttable[sq] & (b->pieces[HORSENUMBER] & enemyPieces))
        return true;

    if (kingtable[sq] & (b->pieces[KINGNUMBER] & enemyPieces))
        return true;

    if (getBishopAttacks(sq, occ) &
        ((b->pieces[BISHOPNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    if (getRookAttacks(sq, occ) &
        ((b->pieces[ROOKNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    return false;
}

Bitboard pawnMask(Position *board, bool color)
{
    Bitboard pawns = board->pieces[PAWNNUMBER] & board->color[color];

    if (color)
    {
        return ((pawns << 7) & 0x7f7f7f7f7f7f7f7fULL) |
               ((pawns << 9) & 0xfefefefefefefefeULL);
    }
    else
    {
        return ((pawns >> 7) & 0xfefefefefefefefeULL) |
               ((pawns >> 9) & 0x7f7f7f7f7f7f7f7fULL);
    }
}
Bitboard horseMask(Position *board, bool color)
{
    const Bitboard FILE_A = 0x0101010101010101ULL;
    const Bitboard FILE_B = 0x0202020202020202ULL;
    const Bitboard FILE_G = 0x4040404040404040ULL;
    const Bitboard FILE_H = 0x8080808080808080ULL;

    const Bitboard NOT_A_FILE = ~FILE_A;
    const Bitboard NOT_H_FILE = ~FILE_H;
    const Bitboard NOT_AB_FILE = ~(FILE_A | FILE_B);
    const Bitboard NOT_GH_FILE = ~(FILE_G | FILE_H);

    Bitboard knights = board->pieces[HORSENUMBER] & board->color[color];
    Bitboard horsemask = 0ULL;

    horsemask |= (knights & NOT_H_FILE) << 17;
    horsemask |= (knights & NOT_A_FILE) << 15;
    horsemask |= (knights & NOT_GH_FILE) << 10;
    horsemask |= (knights & NOT_AB_FILE) << 6;

    horsemask |= (knights & NOT_A_FILE) >> 17;
    horsemask |= (knights & NOT_H_FILE) >> 15;
    horsemask |= (knights & NOT_AB_FILE) >> 10;
    horsemask |= (knights & NOT_GH_FILE) >> 6;

    return horsemask;
}

Bitboard bishopMask(Position *board, bool color)
{
    Bitboard bishopmask = 0ULL;

    uint64_t sliding =
        (board->pieces[1] | board->pieces[QUEENNUMBER]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getBishopAttacks(ind, board->color[OCCUPANCY]);

        attacks &= ~board->color[color];

        bishopmask |= attacks;
    }

    return bishopmask;
}

Bitboard rookMask(Position *board, bool color)
{
    Bitboard rookmask = 0ULL;

    uint64_t sliding =
        (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) &
        board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getRookAttacks(ind, board->color[OCCUPANCY]);

        attacks &= ~board->color[color];

        rookmask |= attacks;
    }

    return rookmask;
}

Bitboard kingMask(Position *board, bool color)
{
    Bitboard kingscolor = board->pieces[KINGNUMBER] & board->color[color];

    if (!kingscolor)
        return 0ULL;

    return kingtable[__builtin_ctzll(kingscolor)];
}

// Optimized: Separate white pawn moves (no color branching)
void pawnMovesWhite(Position *board, MoveList *list)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[WHITE];
    if (!pawns)
        return;

    uint64_t empty = ~(board->color[OCCUPANCY]);
    uint64_t enemies = board->color[BLACK];

    if (board->epsquare != -1)
    {
        int cap_sq = board->epsquare + 8;
        if ((board->pieces[PAWNNUMBER] & board->color[BLACK]) &
            (1ULL << cap_sq))
        {
            enemies |= (1ULL << board->epsquare);
        }
    }

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;
    const uint64_t RANK_7 = 0x000000000000FF00ULL;
    const uint64_t RANK_3 = 0x0000FF0000000000ULL;

    uint64_t norm_pawns = pawns & ~RANK_7;
    uint64_t prom_pawns = pawns & RANK_7;

    uint64_t single_push = (norm_pawns >> 8) & empty;
    uint64_t double_push = ((single_push & RANK_3) >> 8) & empty;
    uint64_t capture_l = ((norm_pawns & ~FILE_A) >> 9) & enemies;
    uint64_t capture_r = ((norm_pawns & ~FILE_H) >> 7) & enemies;

    while (single_push)
    {
        int to = pop_lsb(&single_push);
        int from = to + 8;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to + 16;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to + 9;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to + 7;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    // Promotions
    if (prom_pawns)
    {
        uint64_t prom_single = (prom_pawns >> 8) & empty;
        uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) >> 9) & enemies;
        uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) >> 7) & enemies;

        while (prom_single)
        {
            int to = pop_lsb(&prom_single);
            int from = to + 8;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to + 9;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to + 7;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }
    }
}

// Optimized: Separate black pawn moves (no color branching)
void pawnMovesBlack(Position *board, MoveList *list)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[BLACK];
    if (!pawns)
        return;

    uint64_t empty = ~(board->color[OCCUPANCY]);
    uint64_t enemies = board->color[WHITE];

    if (board->epsquare != -1)
    {
        int cap_sq = board->epsquare - 8;
        if ((board->pieces[PAWNNUMBER] & board->color[WHITE]) &
            (1ULL << cap_sq))
        {
            enemies |= (1ULL << board->epsquare);
        }
    }

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;
    const uint64_t RANK_2 = 0x00FF000000000000ULL;
    const uint64_t RANK_6 = 0x0000000000FF0000ULL;

    uint64_t norm_pawns = pawns & ~RANK_2;
    uint64_t prom_pawns = pawns & RANK_2;

    uint64_t single_push = (norm_pawns << 8) & empty;
    uint64_t double_push = ((single_push & RANK_6) << 8) & empty;
    uint64_t capture_l = ((norm_pawns & ~FILE_A) << 7) & enemies;
    uint64_t capture_r = ((norm_pawns & ~FILE_H) << 9) & enemies;

    while (single_push)
    {
        int to = pop_lsb(&single_push);
        int from = to - 8;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to - 16;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to - 7;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to - 9;
        list->movelist[list->offset++] = (from << 6) | to;
    }

    if (prom_pawns)
    {
        uint64_t prom_single = (prom_pawns << 8) & empty;
        uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) << 7) & enemies;
        uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) << 9) & enemies;

        while (prom_single)
        {
            int to = pop_lsb(&prom_single);
            int from = to - 8;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to - 7;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to - 9;
            list->movelist[list->offset++] = (5U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (6U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (7U << 12) | (from << 6) | to;
            list->movelist[list->offset++] = (8U << 12) | (from << 6) | to;
        }
    }
}

Bitboard horseMoves(Position *board, bool color)
{
    const Bitboard FILE_A = 0x0101010101010101ULL;
    const Bitboard FILE_B = 0x0202020202020202ULL;
    const Bitboard FILE_G = 0x4040404040404040ULL;
    const Bitboard FILE_H = 0x8080808080808080ULL;

    const Bitboard NOT_A_FILE = ~FILE_A;
    const Bitboard NOT_H_FILE = ~FILE_H;
    const Bitboard NOT_AB_FILE = ~(FILE_A | FILE_B);
    const Bitboard NOT_GH_FILE = ~(FILE_G | FILE_H);

    Bitboard knights = board->pieces[HORSENUMBER] & board->color[color];
    Bitboard attacks = 0ULL;

    attacks |= (knights & NOT_H_FILE) << 17;
    attacks |= (knights & NOT_A_FILE) << 15;
    attacks |= (knights & NOT_GH_FILE) << 10;
    attacks |= (knights & NOT_AB_FILE) << 6;

    attacks |= (knights & NOT_A_FILE) >> 17;
    attacks |= (knights & NOT_H_FILE) >> 15;
    attacks |= (knights & NOT_AB_FILE) >> 10;
    attacks |= (knights & NOT_GH_FILE) >> 6;

    return attacks & ~board->color[color];
}

void bishopMoves(Position *board, MoveList *list, bool color)
{
    uint64_t sliding =
        (board->pieces[1] | board->pieces[QUEENNUMBER]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getBishopAttacks(ind, board->color[OCCUPANCY]);

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = (ind << 6) | target;
        }
    }
}

void rookMoves(Position *board, MoveList *list, bool color)
{
    uint64_t sliding =
        (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) &
        board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getRookAttacks(ind, board->color[OCCUPANCY]);

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = (ind << 6) | target;
        }
    }
}

bool squareAttacked_custom(Position *b, int sq, int enemy,
                           uint64_t custom_occ)
{
    uint64_t enemyPieces = b->color[enemy] & custom_occ;

    if ((enemy == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
        (b->pieces[PAWNNUMBER] & enemyPieces))
        return true;

    if (knighttable[sq] & (b->pieces[HORSENUMBER] & enemyPieces))
        return true;

    if (kingtable[sq] & (b->pieces[KINGNUMBER] & enemyPieces))
        return true;

    if (getBishopAttacks(sq, custom_occ) &
        ((b->pieces[1] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    if (getRookAttacks(sq, custom_occ) &
        ((b->pieces[3] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    return false;
}

void kingMoves(Position *board, MoveList *list, bool color, int check_count)
{
    int us = color;
    int them = !color;
    uint64_t kings = board->pieces[KINGNUMBER] & board->color[us];

    int from = __builtin_ctzll(kings);
    uint64_t occupancy = board->color[OCCUPANCY];

    uint64_t attacks = kingtable[from];
    attacks &= ~board->color[us];

    while (attacks)
    {
        int to = pop_lsb(&attacks);
        list->movelist[list->offset++] = (from << 6) | to;
    }

    if (check_count == 0)
    {
        if (us == 0 && from == E1) // White King
        {
            // Kingside (White)
            if (board->castling & (1U << WHITE_KINGSIDE))
            {
                if (!(occupancy & ((1ULL << F1) | (1ULL << G1))))
                {
                    if (!squareAttacked_custom(board, F1, them, occupancy))
                    {
                        list->movelist[list->offset++] =
                            (1U << 12) | (from << 6) | G1;
                    }
                }
            }
            // Queenside (White)
            if (board->castling & (1U << WHITE_QUEENSIDE))
            {
                if (!(occupancy &
                      ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))))
                {
                    if (!squareAttacked_custom(board, D1, them, occupancy))
                    {
                        list->movelist[list->offset++] =
                            (2U << 12) | (from << 6) | C1;
                    }
                }
            }
        }
        else if (us == 1 && from == E8)
        {
            // Kingside (Black)
            if (board->castling & (1U << BLACK_KINGSIDE))
            {
                if (!(occupancy & ((1ULL << F8) | (1ULL << G8))))
                {
                    if (!squareAttacked_custom(board, F8, them, occupancy))
                    {
                        list->movelist[list->offset++] =
                            (4U << 12) | (from << 6) | G8;
                    }
                }
            }
            // Queenside (Black)
            if (board->castling & (1U << BLACK_QUEENSIDE))
            {
                if (!(occupancy &
                      ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))))
                {
                    if (!squareAttacked_custom(board, D8, them, occupancy))
                    {
                        list->movelist[list->offset++] =
                            (3U << 12) | (from << 6) | C8;
                    }
                }
            }
        }
    }
}

uint64_t get_checkers(Position *board, int sq, int enemy_color)
{
    uint64_t occ = board->color[OCCUPANCY];
    uint64_t checkers = 0;

    checkers |=
        (enemy_color == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
        (board->pieces[PAWNNUMBER] & board->color[enemy_color]);
    checkers |= knighttable[sq] &
                (board->pieces[HORSENUMBER] & board->color[enemy_color]);
    checkers |= getBishopAttacks(sq, occ) &
                ((board->pieces[1] | board->pieces[QUEENNUMBER]) &
                 board->color[enemy_color]);
    checkers |= getRookAttacks(sq, occ) &
                ((board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) &
                 board->color[enemy_color]);

    return checkers;
}

uint64_t get_potential_pinners(Position *board, int king_sq, int enemy_color)
{
    uint64_t pinners = 0;
    pinners |= getBishopAttacks(king_sq, 0) &
               ((board->pieces[1] | board->pieces[QUEENNUMBER]) &
                board->color[enemy_color]);
    pinners |= getRookAttacks(king_sq, 0) &
               ((board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) &
                board->color[enemy_color]);
    return pinners;
}

static inline void tryAddLegalMove(MoveList *list, int from, int to, int flag,
                                   uint64_t check_mask, uint64_t pinned_pieces,
                                   const uint64_t pinner_ray[64])
{
    uint64_t from_bb = 1ULL << from;
    uint64_t to_bb = 1ULL << to;

    if (from_bb & pinned_pieces)
    {
        if (!(to_bb & pinner_ray[from]))
            return;
    }

    if (!(to_bb & check_mask))
        return;

    if (flag)
        ADD_FLAG_MOVE(list, flag, from, to);
    else
        ADD_MOVE(list, from, to);
}

static inline void
tryAddLegalPawnMove(Position *board, MoveList *list, int us, int them, int from,
                    int to, int flag, uint64_t check_mask,
                    uint64_t pinned_pieces, const uint64_t pinner_ray[64],
                    int check_count, int king_sq, uint64_t occ, bool is_ep)
{
    uint64_t from_bb = 1ULL << from;
    uint64_t to_bb = 1ULL << to;

    if (from_bb & pinned_pieces)
    {
        if (!(to_bb & pinner_ray[from]))
            return;
    }

    if (is_ep)
    {
        int cap_sq = to + (us == 0 ? 8 : -8);
        uint64_t cap_bb = 1ULL << cap_sq;

        if (check_count == 1 && !(to_bb & check_mask) && !(cap_bb & check_mask))
            return;

        uint64_t occ_after_ep = (occ & ~from_bb & ~cap_bb) | to_bb;
        if (squareAttacked_custom(board, king_sq, them, occ_after_ep))
            return;
    }
    else
    {
        if (!(to_bb & check_mask))
            return;
    }

    if (flag)
        ADD_FLAG_MOVE(list, flag, from, to);
    else
        ADD_MOVE(list, from, to);
}

static void pawnMovesWhiteLegal(Position *board, MoveList *list,
                                uint64_t check_mask, uint64_t pinned_pieces,
                                const uint64_t pinner_ray[64], int check_count,
                                int king_sq, int them, uint64_t occ)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[WHITE];
    if (!pawns)
        return;

    uint64_t empty = ~board->color[OCCUPANCY];
    uint64_t enemies = board->color[BLACK];

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;
    const uint64_t RANK_7 = 0x000000000000FF00ULL;
    const uint64_t RANK_3 = 0x0000FF0000000000ULL;

    uint64_t norm_pawns = pawns & ~RANK_7;
    uint64_t prom_pawns = pawns & RANK_7;

    uint64_t single_push = (norm_pawns >> 8) & empty;
    uint64_t double_push = ((single_push & RANK_3) >> 8) & empty;
    uint64_t capture_l = ((norm_pawns & ~FILE_A) >> 9) & enemies;
    uint64_t capture_r = ((norm_pawns & ~FILE_H) >> 7) & enemies;

    while (single_push)
    {
        int to = pop_lsb(&single_push);
        int from = to + 8;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to + 16;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to + 9;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to + 7;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    if (board->epsquare != -1)
    {
        int ep = board->epsquare;
        int cap_sq = ep + 8;

        if ((board->pieces[PAWNNUMBER] & board->color[BLACK]) &
            (1ULL << cap_sq))
        {
            uint64_t ep_bb = 1ULL << ep;
            uint64_t ep_l = ((norm_pawns & ~FILE_A) >> 9) & ep_bb;
            uint64_t ep_r = ((norm_pawns & ~FILE_H) >> 7) & ep_bb;

            if (ep_l)
                tryAddLegalPawnMove(board, list, 0, them, ep + 9, ep, 0,
                                    check_mask, pinned_pieces, pinner_ray,
                                    check_count, king_sq, occ, true);

            if (ep_r)
                tryAddLegalPawnMove(board, list, 0, them, ep + 7, ep, 0,
                                    check_mask, pinned_pieces, pinner_ray,
                                    check_count, king_sq, occ, true);
        }
    }

    if (prom_pawns)
    {
        uint64_t prom_single = (prom_pawns >> 8) & empty;
        uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) >> 9) & enemies;
        uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) >> 7) & enemies;

        while (prom_single)
        {
            int to = pop_lsb(&prom_single);
            int from = to + 8;
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to + 9;
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to + 7;
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }
    }
}

static void pawnMovesBlackLegal(Position *board, MoveList *list,
                                uint64_t check_mask, uint64_t pinned_pieces,
                                const uint64_t pinner_ray[64], int check_count,
                                int king_sq, int them, uint64_t occ)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[BLACK];
    if (!pawns)
        return;

    uint64_t empty = ~board->color[OCCUPANCY];
    uint64_t enemies = board->color[WHITE];

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;
    const uint64_t RANK_2 = 0x00FF000000000000ULL;
    const uint64_t RANK_6 = 0x0000000000FF0000ULL;

    uint64_t norm_pawns = pawns & ~RANK_2;
    uint64_t prom_pawns = pawns & RANK_2;

    uint64_t single_push = (norm_pawns << 8) & empty;
    uint64_t double_push = ((single_push & RANK_6) << 8) & empty;
    uint64_t capture_l = ((norm_pawns & ~FILE_A) << 7) & enemies;
    uint64_t capture_r = ((norm_pawns & ~FILE_H) << 9) & enemies;

    while (single_push)
    {
        int to = pop_lsb(&single_push);
        int from = to - 8;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to - 16;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to - 7;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to - 9;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq,
                            occ, false);
    }

    if (board->epsquare != -1)
    {
        int ep = board->epsquare;
        int cap_sq = ep - 8;

        if ((board->pieces[PAWNNUMBER] & board->color[WHITE]) &
            (1ULL << cap_sq))
        {
            uint64_t ep_bb = 1ULL << ep;
            uint64_t ep_l = ((norm_pawns & ~FILE_A) << 7) & ep_bb;
            uint64_t ep_r = ((norm_pawns & ~FILE_H) << 9) & ep_bb;

            if (ep_l)
                tryAddLegalPawnMove(board, list, 1, them, ep - 7, ep, 0,
                                    check_mask, pinned_pieces, pinner_ray,
                                    check_count, king_sq, occ, true);

            if (ep_r)
                tryAddLegalPawnMove(board, list, 1, them, ep - 9, ep, 0,
                                    check_mask, pinned_pieces, pinner_ray,
                                    check_count, king_sq, occ, true);
        }
    }

    if (prom_pawns)
    {
        uint64_t prom_single = (prom_pawns << 8) & empty;
        uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) << 7) & enemies;
        uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) << 9) & enemies;

        while (prom_single)
        {
            int to = pop_lsb(&prom_single);
            int from = to - 8;
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to - 7;
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to - 9;
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask,
                                pinned_pieces, pinner_ray, check_count, king_sq,
                                occ, false);
        }
    }
}

static void horseMovesLegal(Position *board, MoveList *list, bool color,
                            uint64_t check_mask, uint64_t pinned_pieces)
{
    uint64_t own = board->color[color];

    // Pinned knights cannot move legally.
    uint64_t knights = board->pieces[HORSENUMBER] & own & ~pinned_pieces;

    while (knights)
    {
        int from = pop_lsb(&knights);
        uint64_t attacks = knighttable[from] & ~own & check_mask;

        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }
}

static void bishopMovesLegal(Position *board, MoveList *list, bool color,
                             uint64_t check_mask, uint64_t pinned_pieces,
                             const uint64_t pinner_ray[64])
{
    uint64_t own = board->color[color];
    uint64_t occ = board->color[OCCUPANCY];
    uint64_t sliding =
        (board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]) & own;

    while (sliding)
    {
        int from = pop_lsb(&sliding);
        uint64_t attacks = getBishopAttacks(from, occ) & ~own & check_mask;

        if ((1ULL << from) & pinned_pieces)
            attacks &= pinner_ray[from];

        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }
}

static void rookMovesLegal(Position *board, MoveList *list, bool color,
                           uint64_t check_mask, uint64_t pinned_pieces,
                           const uint64_t pinner_ray[64])
{
    uint64_t own = board->color[color];
    uint64_t occ = board->color[OCCUPANCY];
    uint64_t sliding =
        (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) & own;

    while (sliding)
    {
        int from = pop_lsb(&sliding);
        uint64_t attacks = getRookAttacks(from, occ) & ~own & check_mask;

        if ((1ULL << from) & pinned_pieces)
            attacks &= pinner_ray[from];

        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }
}

static void kingMovesLegal(Position *board, MoveList *list, bool color,
                           int check_count, const AttackSet *enemyAttacks)
{
    int us = color;
    int them = !color;

    uint64_t kings = board->pieces[KINGNUMBER] & board->color[us];
    if (!kings)
        return;

    int from = __builtin_ctzll(kings);
    uint64_t occupancy = board->color[OCCUPANCY];
    uint64_t from_bb = 1ULL << from;
    uint64_t own_without_king = board->color[us] & ~from_bb;

    uint64_t attacks = kingtable[from] & ~own_without_king;

    while (attacks)
    {
        int to = pop_lsb(&attacks);
        uint64_t to_bb = 1ULL << to;
        uint64_t occ_after_king_move = (occupancy & ~from_bb) | to_bb;

        if (!squareAttacked_fast(to, them, occ_after_king_move, enemyAttacks))
            ADD_MOVE(list, from, to);
    }

    if (check_count == 0)
    {
        if (us == 0 && from == E1)
        {
            if (board->castling & (1U << WHITE_KINGSIDE))
            {
                if (!(occupancy & ((1ULL << F1) | (1ULL << G1))))
                {
                    uint64_t occ_g = (occupancy & ~from_bb) | (1ULL << G1);
                    if (!squareAttacked_fast(F1, them, occupancy,
                                             enemyAttacks) &&
                        !squareAttacked_fast(G1, them, occ_g, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 1U, from, G1);
                    }
                }
            }

            if (board->castling & (1U << WHITE_QUEENSIDE))
            {
                if (!(occupancy &
                      ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))))
                {
                    uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C1);
                    if (!squareAttacked_fast(D1, them, occupancy,
                                             enemyAttacks) &&
                        !squareAttacked_fast(C1, them, occ_c, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 2U, from, C1);
                    }
                }
            }
        }
        else if (us == 1 && from == E8)
        {
            if (board->castling & (1U << BLACK_KINGSIDE))
            {
                if (!(occupancy & ((1ULL << F8) | (1ULL << G8))))
                {
                    uint64_t occ_g = (occupancy & ~from_bb) | (1ULL << G8);
                    if (!squareAttacked_fast(F8, them, occupancy,
                                             enemyAttacks) &&
                        !squareAttacked_fast(G8, them, occ_g, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 4U, from, G8);
                    }
                }
            }

            if (board->castling & (1U << BLACK_QUEENSIDE))
            {
                if (!(occupancy &
                      ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))))
                {
                    uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C8);
                    if (!squareAttacked_fast(D8, them, occupancy,
                                             enemyAttacks) &&
                        !squareAttacked_fast(C8, them, occ_c, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 3U, from, C8);
                    }
                }
            }
        }
    }
}

static uint64_t BISHOP_PSEUDO[64];
static uint64_t ROOK_PSEUDO[64];

void init_pseudo_attacks(void)
{
    for (int sq = 0; sq < 64; sq++)
    {
        BISHOP_PSEUDO[sq] = getBishopAttacks(sq, 0);
        ROOK_PSEUDO[sq] = getRookAttacks(sq, 0);
    }
}

static void legalMoveGenWhite(Position *board, MoveList *list);
static void legalMoveGenBlack(Position *board, MoveList *list);

#define DEFINE_LEGALMOVEGEN(COLOR, US, THEM)                                    \
    static void legalMoveGen##COLOR(Position *board, MoveList *list)            \
    {                                                                           \
        board->color[OCCUPANCY] = board->color[WHITE] | board->color[BLACK];    \
                                                                                \
        const int us   = US;                                                    \
        const int them = THEM;                                                  \
                                                                                \
        uint64_t king_bb = board->pieces[KINGNUMBER] & board->color[us];        \
        if (!king_bb)                                                           \
            return;                                                             \
                                                                                \
        int king_sq       = __builtin_ctzll(king_bb);                           \
        uint64_t occ      = board->color[OCCUPANCY];                            \
        uint64_t them_bb  = board->color[them];                                 \
        uint64_t us_bb    = board->color[us];                                   \
                                                                                \
        uint64_t checkers =                                                     \
            (((THEM == 0 ? white_pawn_attacks[king_sq]                          \
                         : black_pawn_attacks[king_sq]) &                       \
              board->pieces[PAWNNUMBER]) |                                      \
             (knighttable[king_sq] & board->pieces[HORSENUMBER])) &             \
            them_bb;                                                            \
                                                                                \
        uint64_t bq = board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]; \
        uint64_t rq = board->pieces[ROOKNUMBER]   | board->pieces[QUEENNUMBER]; \
                                                                                \
        uint64_t sliders = ((BISHOP_PSEUDO[king_sq] & bq) |                     \
                            (ROOK_PSEUDO[king_sq]   & rq)) &                    \
                           them_bb;                                             \
                                                                                \
        uint64_t pinned_pieces = 0;                                             \
        uint64_t pinner_ray[64];                                                \
                                                                                \
        while (sliders)                                                         \
        {                                                                       \
            int s = pop_lsb(&sliders);                                          \
            uint64_t ray     = ray_between_table[king_sq * 64 + s];             \
            uint64_t between = ray & occ;                                       \
                                                                                \
            if (!between)                                                       \
            {                                                                   \
                checkers |= 1ULL << s;                                          \
            }                                                                   \
            else if ((between & (between - 1)) == 0 && (between & us_bb))       \
            {                                                                   \
                int psq = __builtin_ctzll(between);                             \
                pinned_pieces |= between;                                       \
                pinner_ray[psq] = ray | (1ULL << s);                            \
            }                                                                   \
        }                                                                       \
                                                                                \
        int check_count = __builtin_popcountll(checkers);                       \
                                                                                \
        uint64_t check_mask = 0xFFFFFFFFFFFFFFFFULL;                            \
        if (check_count == 1)                                                   \
        {                                                                       \
            int checker_sq = __builtin_ctzll(checkers);                         \
            check_mask = ray_between_table[king_sq * 64 + checker_sq] |         \
                         (1ULL << checker_sq);                                  \
        }                                                                       \
        else if (check_count >= 2)                                              \
        {                                                                       \
            check_mask = 0ULL;                                                  \
        }                                                                       \
                                                                                \
        AttackSet enemyAttacks = buildAttackSet(board, them);                   \
                                                                                \
        kingMovesLegal(board, list, us, check_count, &enemyAttacks);            \
                                                                                \
        if (check_count >= 2)                                                   \
            return;                                                             \
                                                                                \
        if (check_count == 0 && pinned_pieces == 0 && board->epsquare == -1)    \
        {                                                                       \
            pawnMoves##COLOR(board, list);                                      \
            horseMovesLegal(board, list, us, 0xFFFFFFFFFFFFFFFFULL, 0);         \
            bishopMoves(board, list, us);                                       \
            rookMoves(board, list, us);                                         \
            return;                                                             \
        }                                                                       \
                                                                                \
        pawnMoves##COLOR##Legal(board, list, check_mask, pinned_pieces,         \
                                pinner_ray, check_count, king_sq, them, occ);   \
                                                                                \
        horseMovesLegal(board, list, us, check_mask, pinned_pieces);            \
        bishopMovesLegal(board, list, us, check_mask, pinned_pieces,            \
                         pinner_ray);                                           \
        rookMovesLegal(board, list, us, check_mask, pinned_pieces, pinner_ray); \
    }

DEFINE_LEGALMOVEGEN(White, 0, 1)
DEFINE_LEGALMOVEGEN(Black, 1, 0)

void legalMoveGen(Position *board, MoveList *list)
{
    if (board->turn == 0)
        legalMoveGenWhite(board, list);
    else
        legalMoveGenBlack(board, list);
}


#define DEFINE_MAKEMOVE(COLOR, c)                                              \
    static inline void makeMove##COLOR(Position *restrict board,               \
                                       MoveList *restrict list, int move)      \
    {                                                                          \
        int to = list->movelist[move] & 0x3F;                                  \
        int from = (list->movelist[move] >> 6) & 0x3F;                         \
        int flag = (list->movelist[move] >> 12) & 0xF;                         \
                                                                               \
        uint64_t frombb = 1ULL << from;                                        \
        uint64_t tobb = 1ULL << to;                                            \
                                                                               \
        int old_epsquare = board->epsquare;                                    \
                                                                               \
        int piece = board->mailbox[from];                                      \
        int moving_piece = piece;                                              \
        int victim = board->mailbox[to];                                       \
                                                                               \
        if (piece == 6)                                                        \
            return;                                                            \
                                                                               \
        /* quiet move (no capture, no promotion, no ep) */                     \
        if (flag == 0 && victim == 6 &&                                        \
            !(moving_piece == 0 && to == old_epsquare &&                       \
              old_epsquare != -1))                                             \
        {                                                                      \
            board->epsquare = -1;                                              \
                                                                               \
            board->castling &= castling_mask[from] & castling_mask[to];        \
            board->pieces[moving_piece] &= ~frombb;                            \
            board->color[c] &= ~frombb;                                        \
            board->mailbox[from] = 6;                                          \
                                                                               \
            board->pieces[moving_piece] |= tobb;                               \
            board->color[c] |= tobb;                                           \
            board->mailbox[to] = piece;                                        \
                                                                               \
            if (moving_piece == 0)                                             \
            {                                                                  \
                int from_rank = from >> 3;                                     \
                int to_rank = to >> 3;                                         \
                if (abs1(from_rank - to_rank) == 2)                            \
                    board->epsquare = from + ((c == 0) ? -8 : 8);              \
            }                                                                  \
                                                                               \
            board->turn = !c;                                                  \
            return;                                                            \
        }                                                                      \
                                                                               \
        /* general move (captures, promotions, castling, ep) */                \
        board->epsquare = -1;                                                  \
                                                                               \
        board->castling &= castling_mask[from] & castling_mask[to];            \
                                                                               \
        switch (flag)                                                          \
        {                                                                      \
        case 1: /* white king side */                                          \
            board->pieces[3] &= ~(1ULL << H1);                                 \
            board->color[c] &= ~(1ULL << H1);                                  \
            board->mailbox[H1] = 6;                                            \
            board->pieces[3] |= (1ULL << F1);                                  \
            board->color[c] |= (1ULL << F1);                                   \
            board->mailbox[F1] = 3;                                            \
            board->castling &= ~(1U << WHITE_KINGSIDE);                        \
            break;                                                             \
        case 2: /* white queen side */                                         \
            board->pieces[3] &= ~(1ULL << A1);                                 \
            board->color[c] &= ~(1ULL << A1);                                  \
            board->mailbox[A1] = 6;                                            \
            board->pieces[3] |= (1ULL << D1);                                  \
            board->color[c] |= (1ULL << D1);                                   \
            board->mailbox[D1] = 3;                                            \
            board->castling &= ~(1U << WHITE_QUEENSIDE);                       \
            break;                                                             \
        case 4: /* black king side */                                          \
            board->pieces[3] &= ~(1ULL << H8);                                 \
            board->color[c] &= ~(1ULL << H8);                                  \
            board->mailbox[H8] = 6;                                            \
            board->pieces[3] |= (1ULL << F8);                                  \
            board->color[c] |= (1ULL << F8);                                   \
            board->mailbox[F8] = 3;                                            \
            board->castling &= ~(1U << BLACK_KINGSIDE);                        \
            break;                                                             \
        case 3: /* black queen side */                                         \
            board->pieces[3] &= ~(1ULL << A8);                                 \
            board->color[c] &= ~(1ULL << A8);                                  \
            board->mailbox[A8] = 6;                                            \
            board->pieces[3] |= (1ULL << D8);                                  \
            board->color[c] |= (1ULL << D8);                                   \
            board->mailbox[D8] = 3;                                            \
            board->castling &= ~(1U << BLACK_QUEENSIDE);                       \
            break;                                                             \
        case 5:                                                                \
            piece = 1;                                                         \
            break; /* PR_BISHOP */                                             \
        case 6:                                                                \
            piece = 2;                                                         \
            break; /* PR_KNIGHT */                                             \
        case 7:                                                                \
            piece = 3;                                                         \
            break; /* PR_ROOK   */                                             \
        case 8:                                                                \
            piece = 4;                                                         \
            break; /* PR_QUEEN  */                                             \
        }                                                                      \
                                                                               \
        board->pieces[moving_piece] &= ~frombb;                                \
        board->color[c] &= ~frombb;                                            \
        board->mailbox[from] = 6;                                              \
                                                                               \
        if (moving_piece == 0 && to == old_epsquare && old_epsquare != -1)     \
        {                                                                      \
            int captured_sq = to + ((c == 0) ? 8 : -8);                        \
            uint64_t capBB = 1ULL << captured_sq;                              \
            board->pieces[PAWNNUMBER] &= ~capBB;                               \
            board->color[!c] &= ~capBB;                                        \
            board->mailbox[captured_sq] = 6;                                   \
        }                                                                      \
                                                                               \
        if (victim != 6)                                                       \
            board->pieces[victim] &= ~tobb;                                    \
        board->color[!c] &= ~tobb;                                             \
                                                                               \
        board->pieces[piece] |= tobb;                                          \
        board->color[c] |= tobb;                                               \
        board->mailbox[to] = piece;                                            \
                                                                               \
        if (moving_piece == 0)                                                 \
        {                                                                      \
            int from_y = from >> 3;                                            \
            int to_y = to >> 3;                                                \
            if (abs1(to_y - from_y) == 2)                                      \
                board->epsquare = from + ((c == 0) ? -8 : 8);                  \
        }                                                                      \
                                                                               \
        board->turn = !c;                                                      \
    }

DEFINE_MAKEMOVE(White, 0)
DEFINE_MAKEMOVE(Black, 1)


void makeMove(Position *board, MoveList *list, int move)
{
    if (board->turn == 0)
        makeMoveWhite(board, list, move);
    else
        makeMoveBlack(board, list, move);
}

void moveint(Position *board, uint16_t move)
{
    MoveList tmp;
    tmp.movelist[0] = move;
    if (board->turn == 0)
        makeMoveWhite(board, &tmp, 0);
    else
        makeMoveBlack(board, &tmp, 0);
}

static void perft_print_move(uint16_t move, uint64_t move_nodes)
{
    int from = (move >> 6) & 0x3F;
    int to   =  move        & 0x3F;
    int flag = (move >> 12) & 0xF;

    int x1 = from % 8;
    int y1 = 8 - (from / 8);
    int x2 = to   % 8;
    int y2 = 8 - (to   / 8);

    char promotion = 0;
    if      (flag == 5) promotion = 'b';
    else if (flag == 6) promotion = 'n';
    else if (flag == 7) promotion = 'r';
    else if (flag == 8) promotion = 'q';

    if (promotion)
        printf("%c%i%c%i%c - %" PRIu64 "\n",
               'a' + x1, y1, 'a' + x2, y2, promotion, move_nodes);
    else
        printf("%c%i%c%i - %" PRIu64 "\n",
               'a' + x1, y1, 'a' + x2, y2, move_nodes);
}

static uint64_t perftWhite(Position *board, int depth, int divide);
static uint64_t perftBlack(Position *board, int depth, int divide);

#define DEFINE_PERFT(COLOR, OTHER_COLOR)                                        \
    static uint64_t perft##COLOR(Position *board, int depth, int divide)        \
    {                                                                           \
        if (depth == 0)                                                         \
            return 1;                                                           \
                                                                                \
        MoveList move_list;                                                     \
        move_list.offset = 0;                                                   \
        legalMoveGen(board, &move_list);                                        \
                                                                                \
        uint64_t nodes = 0;                                                     \
        for (unsigned int i = 0; i < move_list.offset; i++)                     \
        {                                                                       \
            Position copy = *board;                                             \
            makeMove##COLOR(&copy, &move_list, i);                              \
                                                                                \
            uint64_t move_nodes =                                               \
                perft##OTHER_COLOR(&copy, depth - 1, divide);                   \
            nodes += move_nodes;                                                \
                                                                                \
            if (depth == divide && divide != 0)                                 \
                perft_print_move(move_list.movelist[i], move_nodes);            \
        }                                                                       \
        return nodes;                                                           \
    }

DEFINE_PERFT(White, Black)
DEFINE_PERFT(Black, White)

uint64_t perft(Position *board, int depth, int divide)
{
    return (board->turn == 0) ? perftWhite(board, depth, divide)
                              : perftBlack(board, depth, divide);
}

static uint64_t perftbulkWhite(Position *board, int depth);
static uint64_t perftbulkBlack(Position *board, int depth);

#define DEFINE_PERFTBULK(COLOR, OTHER_COLOR)                                    \
    static uint64_t perftbulk##COLOR(Position *board, int depth)                \
    {                                                                           \
        if (depth == 0)                                                         \
            return 1;                                                           \
                                                                                \
        MoveList move_list;                                                     \
        move_list.offset = 0;                                                   \
        legalMoveGen(board, &move_list);                                        \
                                                                                \
        if (depth == 1)                                                         \
            return move_list.offset;                                            \
                                                                                \
        uint64_t nodes = 0;                                                     \
        for (unsigned int i = 0; i < move_list.offset; i++)                     \
        {                                                                       \
            Position copy = *board;                                             \
            makeMove##COLOR(&copy, &move_list, i);                              \
            nodes += perftbulk##OTHER_COLOR(&copy, depth - 1);                  \
        }                                                                       \
        return nodes;                                                           \
    }

DEFINE_PERFTBULK(White, Black)
DEFINE_PERFTBULK(Black, White)

uint64_t perftbulk(Position *board, int depth)
{
    return (board->turn == 0) ? perftbulkWhite(board, depth)
                              : perftbulkBlack(board, depth);
}


#define PC(x) ((int)__builtin_popcountll(x))

static inline int countPawnMoves(const Position *board, uint64_t pawns,
                                 int us, uint64_t mask)
{
    if (!pawns)
        return 0;

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;

    uint64_t empty   = ~board->color[OCCUPANCY];
    uint64_t enemies = board->color[!us];

    uint64_t single, dbl, cl, cr, ps, pl, pr;

    if (us == 0)
    {
        const uint64_t RANK_7 = 0x000000000000FF00ULL;
        const uint64_t RANK_3 = 0x0000FF0000000000ULL;

        uint64_t norm = pawns & ~RANK_7;
        uint64_t prom = pawns & RANK_7;

        single = (norm >> 8) & empty;
        dbl    = ((single & RANK_3) >> 8) & empty;
        cl     = ((norm & ~FILE_A) >> 9) & enemies;
        cr     = ((norm & ~FILE_H) >> 7) & enemies;

        ps = (prom >> 8) & empty;
        pl = ((prom & ~FILE_A) >> 9) & enemies;
        pr = ((prom & ~FILE_H) >> 7) & enemies;
    }
    else
    {
        const uint64_t RANK_2 = 0x00FF000000000000ULL;
        const uint64_t RANK_6 = 0x0000000000FF0000ULL;

        uint64_t norm = pawns & ~RANK_2;
        uint64_t prom = pawns & RANK_2;

        single = (norm << 8) & empty;
        dbl    = ((single & RANK_6) << 8) & empty;
        cl     = ((norm & ~FILE_A) << 7) & enemies;
        cr     = ((norm & ~FILE_H) << 9) & enemies;

        ps = (prom << 8) & empty;
        pl = ((prom & ~FILE_A) << 7) & enemies;
        pr = ((prom & ~FILE_H) << 9) & enemies;
    }

    return PC(single & mask) + PC(dbl & mask) + PC(cl & mask) + PC(cr & mask) +
           4 * (PC(ps & mask) + PC(pl & mask) + PC(pr & mask));
}

static inline bool epLegal(Position *board, int us, int them, int from, int to,
                           uint64_t check_mask, uint64_t pinned_pieces,
                           const uint64_t pinner_ray[64], int check_count,
                           int king_sq, uint64_t occ)
{
    uint64_t from_bb = 1ULL << from;
    uint64_t to_bb   = 1ULL << to;

    if ((from_bb & pinned_pieces) && !(to_bb & pinner_ray[from]))
        return false;

    int cap_sq       = to + (us == 0 ? 8 : -8);
    uint64_t cap_bb  = 1ULL << cap_sq;

    if (check_count == 1 && !(to_bb & check_mask) && !(cap_bb & check_mask))
        return false;

    uint64_t occ_after = (occ & ~from_bb & ~cap_bb) | to_bb;
    return !squareAttacked_custom(board, king_sq, them, occ_after);
}

static inline int countEpMoves(Position *board, int us, int them,
                               uint64_t check_mask, uint64_t pinned_pieces,
                               const uint64_t pinner_ray[64], int check_count,
                               int king_sq, uint64_t occ)
{
    if (board->epsquare == -1)
        return 0;

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;

    int ep     = board->epsquare;
    int cap_sq = ep + (us == 0 ? 8 : -8);

    if (!((board->pieces[PAWNNUMBER] & board->color[them]) & (1ULL << cap_sq)))
        return 0;

    uint64_t ep_bb = 1ULL << ep;
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[us];
    int n = 0;

    if (us == 0)
    {
        if (((pawns & ~FILE_A) >> 9) & ep_bb)
            n += epLegal(board, us, them, ep + 9, ep, check_mask, pinned_pieces,
                         pinner_ray, check_count, king_sq, occ);
        if (((pawns & ~FILE_H) >> 7) & ep_bb)
            n += epLegal(board, us, them, ep + 7, ep, check_mask, pinned_pieces,
                         pinner_ray, check_count, king_sq, occ);
    }
    else
    {
        if (((pawns & ~FILE_A) << 7) & ep_bb)
            n += epLegal(board, us, them, ep - 7, ep, check_mask, pinned_pieces,
                         pinner_ray, check_count, king_sq, occ);
        if (((pawns & ~FILE_H) << 9) & ep_bb)
            n += epLegal(board, us, them, ep - 9, ep, check_mask, pinned_pieces,
                         pinner_ray, check_count, king_sq, occ);
    }
    return n;
}

static inline int countKingMovesLegal(Position *board, int us, int check_count,
                                      const AttackSet *enemyAttacks)
{
    int them = !us;

    uint64_t kings = board->pieces[KINGNUMBER] & board->color[us];
    if (!kings)
        return 0;

    int from            = __builtin_ctzll(kings);
    uint64_t occupancy  = board->color[OCCUPANCY];
    uint64_t from_bb    = 1ULL << from;
    uint64_t own_no_king = board->color[us] & ~from_bb;

    uint64_t attacks = kingtable[from] & ~own_no_king;
    int n = 0;

    while (attacks)
    {
        int to = pop_lsb(&attacks);
        uint64_t occ_after = (occupancy & ~from_bb) | (1ULL << to);
        n += !squareAttacked_fast(to, them, occ_after, enemyAttacks);
    }

    if (check_count == 0)
    {
        if (us == 0 && from == E1)
        {
            if ((board->castling & (1U << WHITE_KINGSIDE)) &&
                !(occupancy & ((1ULL << F1) | (1ULL << G1))))
            {
                uint64_t occ_g = (occupancy & ~from_bb) | (1ULL << G1);
                n += !squareAttacked_fast(F1, them, occupancy, enemyAttacks) &&
                     !squareAttacked_fast(G1, them, occ_g, enemyAttacks);
            }
            if ((board->castling & (1U << WHITE_QUEENSIDE)) &&
                !(occupancy & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))))
            {
                uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C1);
                n += !squareAttacked_fast(D1, them, occupancy, enemyAttacks) &&
                     !squareAttacked_fast(C1, them, occ_c, enemyAttacks);
            }
        }
        else if (us == 1 && from == E8)
        {
            if ((board->castling & (1U << BLACK_KINGSIDE)) &&
                !(occupancy & ((1ULL << F8) | (1ULL << G8))))
            {
                uint64_t occ_g = (occupancy & ~from_bb) | (1ULL << G8);
                n += !squareAttacked_fast(F8, them, occupancy, enemyAttacks) &&
                     !squareAttacked_fast(G8, them, occ_g, enemyAttacks);
            }
            if ((board->castling & (1U << BLACK_QUEENSIDE)) &&
                !(occupancy & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))))
            {
                uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C8);
                n += !squareAttacked_fast(D8, them, occupancy, enemyAttacks) &&
                     !squareAttacked_fast(C8, them, occ_c, enemyAttacks);
            }
        }
    }
    return n;
}

static inline int countKnightMoves(Position *board, int us, uint64_t check_mask,
                                   uint64_t pinned_pieces)
{
    uint64_t own     = board->color[us];
    uint64_t knights = board->pieces[HORSENUMBER] & own & ~pinned_pieces;
    int n = 0;

    while (knights)
    {
        int from = pop_lsb(&knights);
        n += PC(knighttable[from] & ~own & check_mask);
    }
    return n;
}

static inline int countSliderMoves(Position *board, int us, uint64_t check_mask,
                                   uint64_t pinned_pieces,
                                   const uint64_t pinner_ray[64])
{
    uint64_t own = board->color[us];
    uint64_t occ = board->color[OCCUPANCY];
    int n = 0;

    uint64_t bq = (board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]) & own;
    while (bq)
    {
        int from = pop_lsb(&bq);
        uint64_t a = getBishopAttacks(from, occ) & ~own & check_mask;
        if ((1ULL << from) & pinned_pieces)
            a &= pinner_ray[from];
        n += PC(a);
    }

    uint64_t rq = (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) & own;
    while (rq)
    {
        int from = pop_lsb(&rq);
        uint64_t a = getRookAttacks(from, occ) & ~own & check_mask;
        if ((1ULL << from) & pinned_pieces)
            a &= pinner_ray[from];
        n += PC(a);
    }
    return n;
}

#define DEFINE_COUNTMOVES(COLOR, US, THEM)                                      \
    static int countMoves##COLOR(Position *board)                               \
    {                                                                           \
        board->color[OCCUPANCY] = board->color[WHITE] | board->color[BLACK];    \
                                                                                \
        const int us   = US;                                                    \
        const int them = THEM;                                                  \
                                                                                \
        uint64_t king_bb = board->pieces[KINGNUMBER] & board->color[us];        \
        if (!king_bb)                                                           \
            return 0;                                                           \
                                                                                \
        int king_sq      = __builtin_ctzll(king_bb);                            \
        uint64_t occ     = board->color[OCCUPANCY];                             \
        uint64_t them_bb = board->color[them];                                  \
        uint64_t us_bb   = board->color[us];                                    \
                                                                                \
        uint64_t checkers =                                                     \
            (((THEM == 0 ? white_pawn_attacks[king_sq]                          \
                         : black_pawn_attacks[king_sq]) &                       \
              board->pieces[PAWNNUMBER]) |                                      \
             (knighttable[king_sq] & board->pieces[HORSENUMBER])) &             \
            them_bb;                                                            \
                                                                                \
        uint64_t bq = board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]; \
        uint64_t rq = board->pieces[ROOKNUMBER]   | board->pieces[QUEENNUMBER]; \
                                                                                \
        uint64_t sliders = ((BISHOP_PSEUDO[king_sq] & bq) |                     \
                            (ROOK_PSEUDO[king_sq]   & rq)) & them_bb;           \
                                                                                \
        uint64_t pinned_pieces = 0;                                             \
        uint64_t pinner_ray[64];                                                \
                                                                                \
        while (sliders)                                                         \
        {                                                                       \
            int s            = pop_lsb(&sliders);                               \
            uint64_t ray     = ray_between_table[king_sq * 64 + s];             \
            uint64_t between = ray & occ;                                       \
                                                                                \
            if (!between)                                                       \
                checkers |= 1ULL << s;                                          \
            else if ((between & (between - 1)) == 0 && (between & us_bb))       \
            {                                                                   \
                int psq = __builtin_ctzll(between);                             \
                pinned_pieces |= between;                                       \
                pinner_ray[psq] = ray | (1ULL << s);                            \
            }                                                                   \
        }                                                                       \
                                                                                \
        int check_count = PC(checkers);                                         \
                                                                                \
        uint64_t check_mask = ~0ULL;                                            \
        if (check_count == 1)                                                   \
        {                                                                       \
            int checker_sq = __builtin_ctzll(checkers);                         \
            check_mask = ray_between_table[king_sq * 64 + checker_sq] |         \
                         (1ULL << checker_sq);                                  \
        }                                                                       \
        else if (check_count >= 2)                                              \
            check_mask = 0ULL;                                                  \
                                                                                \
        AttackSet enemyAttacks = buildAttackSet(board, them);                   \
                                                                                \
        int n = countKingMovesLegal(board, us, check_count, &enemyAttacks);     \
                                                                                \
        if (check_count >= 2)                                                   \
            return n;                                                           \
                                                                                \
        uint64_t pawns = board->pieces[PAWNNUMBER] & us_bb;                     \
        n += countPawnMoves(board, pawns & ~pinned_pieces, us, check_mask);     \
                                                                                \
        uint64_t pp = pawns & pinned_pieces;                                    \
        while (pp)                                                              \
        {                                                                       \
            int sq = pop_lsb(&pp);                                              \
            n += countPawnMoves(board, 1ULL << sq, us,                          \
                                check_mask & pinner_ray[sq]);                   \
        }                                                                       \
                                                                                \
        n += countEpMoves(board, us, them, check_mask, pinned_pieces,           \
                          pinner_ray, check_count, king_sq, occ);               \
                                                                                \
        n += countKnightMoves(board, us, check_mask, pinned_pieces);            \
        n += countSliderMoves(board, us, check_mask, pinned_pieces,             \
                              pinner_ray);                                      \
        return n;                                                               \
    }

DEFINE_COUNTMOVES(White, 0, 1)
DEFINE_COUNTMOVES(Black, 1, 0)

int countLegalMoves(Position *board)
{
    return board->turn == 0 ? countMovesWhite(board) : countMovesBlack(board);
}

static uint64_t perftpopcntWhite(Position *board, int depth);
static uint64_t perftpopcntBlack(Position *board, int depth);

#define DEFINE_PERFTPOPCNT(COLOR, OTHER_COLOR)                                  \
    static uint64_t perftpopcnt##COLOR(Position *board, int depth)              \
    {                                                                           \
        if (depth == 0)                                                         \
            return 1;                                                           \
                                                                                \
        if (depth == 1)                                                         \
            return (uint64_t)countMoves##COLOR(board);                          \
                                                                                \
        MoveList move_list;                                                     \
        move_list.offset = 0;                                                   \
        legalMoveGen##COLOR(board, &move_list);                                 \
                                                                                \
        uint64_t nodes = 0;                                                     \
        for (unsigned int i = 0; i < move_list.offset; i++)                     \
        {                                                                       \
            Position copy = *board;                                             \
            makeMove##COLOR(&copy, &move_list, i);                              \
            nodes += perftpopcnt##OTHER_COLOR(&copy, depth - 1);                \
        }                                                                       \
        return nodes;                                                           \
    }

DEFINE_PERFTPOPCNT(White, Black)
DEFINE_PERFTPOPCNT(Black, White)

uint64_t perftpopcnt(Position *board, int depth)
{
    return (board->turn == 0) ? perftpopcntWhite(board, depth)
                              : perftpopcntBlack(board, depth);
}