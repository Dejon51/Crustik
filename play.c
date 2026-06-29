#include <stdbool.h>
#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"
#include "zobrist.h"
#include "uci.h"
#include "inttypes.h"

typedef struct {
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
    a.bishopsQueens = (b->pieces[BISHOPNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces;
    a.rooksQueens = (b->pieces[ROOKNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces;
    a.king = b->pieces[KINGNUMBER] & enemyPieces;

    return a;
}

static inline bool squareAttacked_fast(int sq, int enemy, uint64_t occ, const AttackSet *a)
{
    if ((enemy == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) & a->pawns)
        return true;

    if (knighttable[sq] & a->knights)
        return true;

    if (kingtable[sq] & a->king)
        return true;

    if (getbishopAttacks(sq, occ) & a->bishopsQueens)
        return true;

    if (getrookAttacks(sq, occ) & a->rooksQueens)
        return true;

    return false;
}

#define ADD_MOVE(list, from, to) \
    ((list)->movelist[(list)->offset++] = ((uint16_t)(((from) << 6) | (to))))

#define ADD_FLAG_MOVE(list, flag, from, to) \
    ((list)->movelist[(list)->offset++] = ((uint16_t)(((flag) << 12) | ((from) << 6) | (to))))

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

    if (getbishopAttacks(sq, occ) &
        ((b->pieces[BISHOPNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    if (getrookAttacks(sq, occ) &
        ((b->pieces[ROOKNUMBER] | b->pieces[QUEENNUMBER]) & enemyPieces))
        return true;

    return false;
}

Bitboard pawnMask(Position *board, bool color)
{
    Bitboard pawns = board->pieces[0] & board->color[color];

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
    Bitboard horsemask = 0ULL;
    uint64_t knights = board->pieces[2] & board->color[color];
    while (knights)
    {
        int ind = pop_lsb(&knights);
        horsemask |= knighttable[ind];
    }
    return horsemask;
}

Bitboard bishopMask(Position *board, bool color)
{
    Bitboard bishopmask = 0ULL;

    uint64_t sliding = (board->pieces[1] | board->pieces[4]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getbishopAttacks(ind, board->color[2]);

        attacks &= ~board->color[color];

        bishopmask |= attacks;
    }

    return bishopmask;
}

Bitboard rookMask(Position *board, bool color)
{
    Bitboard rookmask = 0ULL;

    uint64_t sliding = (board->pieces[3] | board->pieces[4]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getrookAttacks(ind, board->color[2]);

        attacks &= ~board->color[color];

        rookmask |= attacks;
    }

    return rookmask;
}

Bitboard kingMask(Position *board, bool color)
{
    Bitboard kingscolor = board->pieces[5] & board->color[color];

    if (!kingscolor)
        return 0ULL;

    return kingtable[__builtin_ctzll(kingscolor)];
}

// Optimized: Separate white pawn moves (no color branching)
void pawnMovesWhite(Position *board, MoveList *list)
{
    uint64_t pawns = board->pieces[0] & board->color[0];
    if (!pawns)
        return;

    uint64_t empty = ~(board->color[2]);
    uint64_t enemies = board->color[1];

    if (board->epsquare != -1)
    {
        int cap_sq = board->epsquare + 8;
        if ((board->pieces[0] & board->color[1]) & (1ULL << cap_sq))
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
    uint64_t pawns = board->pieces[0] & board->color[1];
    if (!pawns)
        return;

    uint64_t empty = ~(board->color[2]);
    uint64_t enemies = board->color[0];

    if (board->epsquare != -1)
    {
        int cap_sq = board->epsquare - 8;
        if ((board->pieces[0] & board->color[0]) & (1ULL << cap_sq))
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

void horseMoves(Position *board, MoveList *list, bool color)
{
    uint64_t knights = board->pieces[2] & board->color[color];

    while (knights)
    {
        int ind = pop_lsb(&knights);

        uint64_t attacks = knighttable[ind];

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = (ind << 6) | target;
        }
    }
}

void bishopMoves(Position *board, MoveList *list, bool color)
{
    uint64_t sliding = (board->pieces[1] | board->pieces[4]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getbishopAttacks(ind, board->color[2]);

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
    uint64_t sliding = (board->pieces[3] | board->pieces[4]) & board->color[color];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getrookAttacks(ind, board->color[2]);

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = (ind << 6) | target;
        }
    }
}

bool squareAttacked_custom(Position *b, int sq, int enemy, uint64_t custom_occ)
{
    uint64_t enemyPieces = b->color[enemy] & custom_occ;

    if ((enemy == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
        (b->pieces[0] & enemyPieces))
        return true;

    if (knighttable[sq] & (b->pieces[2] & enemyPieces))
        return true;

    if (kingtable[sq] & (b->pieces[5] & enemyPieces))
        return true;

    if (getbishopAttacks(sq, custom_occ) &
        ((b->pieces[1] | b->pieces[4]) & enemyPieces))
        return true;

    if (getrookAttacks(sq, custom_occ) &
        ((b->pieces[3] | b->pieces[4]) & enemyPieces))
        return true;

    return false;
}

void kingMoves(Position *board, MoveList *list, bool color, int check_count)
{
    int us = color;
    int them = !color;
    uint64_t kings = board->pieces[5] & board->color[us];

    int from = __builtin_ctzll(kings);
    uint64_t occupancy = board->color[2];

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
                        list->movelist[list->offset++] = (1U << 12) | (from << 6) | G1;
                    }
                }
            }
            // Queenside (White)
            if (board->castling & (1U << WHITE_QUEENSIDE))
            {
                if (!(occupancy & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))))
                {
                    if (!squareAttacked_custom(board, D1, them, occupancy))
                    {
                        list->movelist[list->offset++] = (2U << 12) | (from << 6) | C1;
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
                        list->movelist[list->offset++] = (4U << 12) | (from << 6) | G8;
                    }
                }
            }
            // Queenside (Black)
            if (board->castling & (1U << BLACK_QUEENSIDE))
            {
                if (!(occupancy & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))))
                {
                    if (!squareAttacked_custom(board, D8, them, occupancy))
                    {
                        list->movelist[list->offset++] = (3U << 12) | (from << 6) | C8;
                    }
                }
            }
        }
    }
}

uint64_t get_checkers(Position *board, int sq, int enemy_color)
{
    uint64_t occ = board->color[2];
    uint64_t checkers = 0;

    checkers |= (enemy_color == 0 ? white_pawn_attacks[sq] : black_pawn_attacks[sq]) &
                (board->pieces[0] & board->color[enemy_color]);
    checkers |= knighttable[sq] & (board->pieces[2] & board->color[enemy_color]);
    checkers |= getbishopAttacks(sq, occ) & ((board->pieces[1] | board->pieces[4]) & board->color[enemy_color]);
    checkers |= getrookAttacks(sq, occ) & ((board->pieces[3] | board->pieces[4]) & board->color[enemy_color]);

    return checkers;
}

uint64_t get_potential_pinners(Position *board, int king_sq, int enemy_color)
{
    uint64_t pinners = 0;
    pinners |= getbishopAttacks(king_sq, 0) & ((board->pieces[1] | board->pieces[4]) & board->color[enemy_color]);
    pinners |= getrookAttacks(king_sq, 0) & ((board->pieces[3] | board->pieces[4]) & board->color[enemy_color]);
    return pinners;
}


static inline void tryAddLegalMove(MoveList *list, int from, int to, int flag,
                                   uint64_t check_mask,
                                   uint64_t pinned_pieces,
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

static inline void tryAddLegalPawnMove(Position *board, MoveList *list,
                                       int us, int them,
                                       int from, int to, int flag,
                                       uint64_t check_mask,
                                       uint64_t pinned_pieces,
                                       const uint64_t pinner_ray[64],
                                       int check_count,
                                       int king_sq,
                                       uint64_t occ,
                                       bool is_ep)
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
                                uint64_t check_mask,
                                uint64_t pinned_pieces,
                                const uint64_t pinner_ray[64],
                                int check_count,
                                int king_sq,
                                int them,
                                uint64_t occ)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[0];
    if (!pawns)
        return;

    uint64_t empty = ~board->color[2];
    uint64_t enemies = board->color[1];

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
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to + 16;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to + 9;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to + 7;
        tryAddLegalPawnMove(board, list, 0, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    if (board->epsquare != -1)
    {
        int ep = board->epsquare;
        int cap_sq = ep + 8;

        if ((board->pieces[PAWNNUMBER] & board->color[1]) & (1ULL << cap_sq))
        {
            uint64_t ep_bb = 1ULL << ep;
            uint64_t ep_l = ((norm_pawns & ~FILE_A) >> 9) & ep_bb;
            uint64_t ep_r = ((norm_pawns & ~FILE_H) >> 7) & ep_bb;

            if (ep_l)
                tryAddLegalPawnMove(board, list, 0, them, ep + 9, ep, 0, check_mask,
                                    pinned_pieces, pinner_ray, check_count, king_sq, occ, true);

            if (ep_r)
                tryAddLegalPawnMove(board, list, 0, them, ep + 7, ep, 0, check_mask,
                                    pinned_pieces, pinner_ray, check_count, king_sq, occ, true);
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
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to + 9;
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to + 7;
            tryAddLegalPawnMove(board, list, 0, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 0, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }
    }
}

static void pawnMovesBlackLegal(Position *board, MoveList *list,
                                uint64_t check_mask,
                                uint64_t pinned_pieces,
                                const uint64_t pinner_ray[64],
                                int check_count,
                                int king_sq,
                                int them,
                                uint64_t occ)
{
    uint64_t pawns = board->pieces[PAWNNUMBER] & board->color[1];
    if (!pawns)
        return;

    uint64_t empty = ~board->color[2];
    uint64_t enemies = board->color[0];

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
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (double_push)
    {
        int to = pop_lsb(&double_push);
        int from = to - 16;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (capture_l)
    {
        int to = pop_lsb(&capture_l);
        int from = to - 7;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    while (capture_r)
    {
        int to = pop_lsb(&capture_r);
        int from = to - 9;
        tryAddLegalPawnMove(board, list, 1, them, from, to, 0, check_mask,
                            pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
    }

    if (board->epsquare != -1)
    {
        int ep = board->epsquare;
        int cap_sq = ep - 8;

        if ((board->pieces[PAWNNUMBER] & board->color[0]) & (1ULL << cap_sq))
        {
            uint64_t ep_bb = 1ULL << ep;
            uint64_t ep_l = ((norm_pawns & ~FILE_A) << 7) & ep_bb;
            uint64_t ep_r = ((norm_pawns & ~FILE_H) << 9) & ep_bb;

            if (ep_l)
                tryAddLegalPawnMove(board, list, 1, them, ep - 7, ep, 0, check_mask,
                                    pinned_pieces, pinner_ray, check_count, king_sq, occ, true);

            if (ep_r)
                tryAddLegalPawnMove(board, list, 1, them, ep - 9, ep, 0, check_mask,
                                    pinned_pieces, pinner_ray, check_count, king_sq, occ, true);
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
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }

        while (prom_capture_l)
        {
            int to = pop_lsb(&prom_capture_l);
            int from = to - 7;
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }

        while (prom_capture_r)
        {
            int to = pop_lsb(&prom_capture_r);
            int from = to - 9;
            tryAddLegalPawnMove(board, list, 1, them, from, to, 5, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 6, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 7, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
            tryAddLegalPawnMove(board, list, 1, them, from, to, 8, check_mask, pinned_pieces, pinner_ray, check_count, king_sq, occ, false);
        }
    }
}

static void horseMovesLegal(Position *board, MoveList *list, bool color,
                            uint64_t check_mask,
                            uint64_t pinned_pieces)
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
                             uint64_t check_mask,
                             uint64_t pinned_pieces,
                             const uint64_t pinner_ray[64])
{
    uint64_t own = board->color[color];
    uint64_t occ = board->color[2];
    uint64_t sliding = (board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]) & own;

    while (sliding)
    {
        int from = pop_lsb(&sliding);
        uint64_t attacks = getbishopAttacks(from, occ) & ~own & check_mask;

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
                           uint64_t check_mask,
                           uint64_t pinned_pieces,
                           const uint64_t pinner_ray[64])
{
    uint64_t own = board->color[color];
    uint64_t occ = board->color[2];
    uint64_t sliding = (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) & own;

    while (sliding)
    {
        int from = pop_lsb(&sliding);
        uint64_t attacks = getrookAttacks(from, occ) & ~own & check_mask;

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
                           int check_count,
                           const AttackSet *enemyAttacks)
{
    int us = color;
    int them = !color;

    uint64_t kings = board->pieces[KINGNUMBER] & board->color[us];
    if (!kings)
        return;

    int from = __builtin_ctzll(kings);
    uint64_t occupancy = board->color[2];
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
                    if (!squareAttacked_fast(F1, them, occupancy, enemyAttacks) &&
                        !squareAttacked_fast(G1, them, occ_g, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 1U, from, G1);
                    }
                }
            }

            if (board->castling & (1U << WHITE_QUEENSIDE))
            {
                if (!(occupancy & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))))
                {
                    uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C1);
                    if (!squareAttacked_fast(D1, them, occupancy, enemyAttacks) &&
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
                    if (!squareAttacked_fast(F8, them, occupancy, enemyAttacks) &&
                        !squareAttacked_fast(G8, them, occ_g, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 4U, from, G8);
                    }
                }
            }

            if (board->castling & (1U << BLACK_QUEENSIDE))
            {
                if (!(occupancy & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))))
                {
                    uint64_t occ_c = (occupancy & ~from_bb) | (1ULL << C8);
                    if (!squareAttacked_fast(D8, them, occupancy, enemyAttacks) &&
                        !squareAttacked_fast(C8, them, occ_c, enemyAttacks))
                    {
                        ADD_FLAG_MOVE(list, 3U, from, C8);
                    }
                }
            }
        }
    }
}

void legalMoveGen(Position *board, MoveList *list)
{
    board->color[2] = board->color[0] | board->color[1];

    int us = board->turn;
    int them = !us;

    uint64_t king_bb = board->pieces[KINGNUMBER] & board->color[us];
    if (!king_bb)
        return;

    int king_sq = __builtin_ctzll(king_bb);
    uint64_t occ = board->color[2];

    uint64_t checkers = get_checkers(board, king_sq, them);
    int check_count = __builtin_popcountll(checkers);

    uint64_t check_mask = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t pinned_pieces = 0;
    uint64_t pinner_ray[64];

    if (check_count == 1)
    {
        int checker_sq = __builtin_ctzll(checkers);
        check_mask = ray_between_table[king_sq * 64 + checker_sq] | (1ULL << checker_sq);
    }
    else if (check_count >= 2)
    {
        check_mask = 0ULL;
    }

    uint64_t potential_pinners = get_potential_pinners(board, king_sq, them);
    while (potential_pinners)
    {
        int pinner_sq = pop_lsb(&potential_pinners);
        uint64_t ray = ray_between_table[king_sq * 64 + pinner_sq];
        uint64_t overlap = ray & occ;

        if (overlap && (overlap & (overlap - 1)) == 0)
        {
            if (overlap & board->color[us])
            {
                int pinned_sq = __builtin_ctzll(overlap);
                pinned_pieces |= (1ULL << pinned_sq);
                pinner_ray[pinned_sq] = ray | (1ULL << pinner_sq);
            }
        }
    }

    AttackSet enemyAttacks = buildAttackSet(board, them);

    kingMovesLegal(board, list, us, check_count, &enemyAttacks);

    if (check_count >= 2)
        return;

    if (us == 0)
        pawnMovesWhiteLegal(board, list, check_mask, pinned_pieces, pinner_ray,
                            check_count, king_sq, them, occ);
    else
        pawnMovesBlackLegal(board, list, check_mask, pinned_pieces, pinner_ray,
                            check_count, king_sq, them, occ);

    horseMovesLegal(board, list, us, check_mask, pinned_pieces);
    bishopMovesLegal(board, list, us, check_mask, pinned_pieces, pinner_ray);
    rookMovesLegal(board, list, us, check_mask, pinned_pieces, pinner_ray);
}

void makeMove(Position *board, MoveList *list, int move)
{
    int direction = (board->turn == 0) ? -1 : 1;

    int to = list->movelist[move] & 0x3F;
    int from = (list->movelist[move] >> 6) & 0x3F;
    int flag = (list->movelist[move] >> 12) & 0xF;

    uint64_t frombb = 1ULL << from;
    uint64_t tobb = 1ULL << to;

    int old_epsquare = board->epsquare;
    int old_castling = board->castling;

    // Remove old Castling and EP from hash
    board->hash ^= zobrist_table[769 + old_castling];
    if (old_epsquare != -1)
        board->hash ^= zobrist_table[785 + (old_epsquare & 7)];

    board->epsquare = -1;

    int piece = board->mailbox[from];
    int moving_piece = piece;
    int victim = board->mailbox[to];

    if (piece == 6)
        return;

    // Remove moving piece from 'from' square in hash
    board->hash ^= zobrist_table[(board->turn * 384) + (moving_piece * 64) + from];

    // If regular capture, remove victim from hash
    if (victim != 6)
    {
        board->hash ^= zobrist_table[(!board->turn * 384) + (victim * 64) + to];
    }

    // Check if this is a capture or pawn move (resets halfmove clock)
    bool is_capture = (board->mailbox[to] != 6);
    bool is_pawn_move = (moving_piece == 0);

    // Check for en passant capture (also resets halfmove clock)
    bool is_ep_capture = (moving_piece == 0 && to == old_epsquare && old_epsquare != -1);

    // Update halfmove clock
    if (is_pawn_move || is_capture || is_ep_capture)
    {
        board->halfmoves = 0;
    }
    else
    {
        board->halfmoves++;
    }

    // king moved remove both castling rights
    if (piece == KINGNUMBER)
    {
        switch (board->turn)
        {
        case 0:
            board->castling &= ~((1U << WHITE_KINGSIDE) | (1U << WHITE_QUEENSIDE));
            break;
        case 1:
            board->castling &= ~((1U << BLACK_KINGSIDE) | (1U << BLACK_QUEENSIDE));
            break;
        }
    }

    // rook moved remove that sides castling
    if (piece == 3)
    {
        if (from == H1)
            board->castling &= ~(1U << WHITE_KINGSIDE);
        if (from == A1)
            board->castling &= ~(1U << WHITE_QUEENSIDE);
        if (from == H8)
            board->castling &= ~(1U << BLACK_KINGSIDE);
        if (from == A8)
            board->castling &= ~(1U << BLACK_QUEENSIDE);
    }

    // castling move handling
    switch (flag)
    {
    case 1: // white king side
        board->pieces[3] &= ~(1ULL << H1);
        board->color[board->turn] &= ~(1ULL << H1);
        board->mailbox[H1] = 6;

        board->pieces[3] |= (1ULL << F1);
        board->color[board->turn] |= (1ULL << F1);
        board->mailbox[F1] = 3;

        board->castling &= ~(1U << WHITE_KINGSIDE);

        // Update Rook hash for castle
        board->hash ^= zobrist_table[(0 * 384) + (3 * 64) + H1];
        board->hash ^= zobrist_table[(0 * 384) + (3 * 64) + F1];
        break;

    case 2: // white queen side
        board->pieces[3] &= ~(1ULL << A1);
        board->color[board->turn] &= ~(1ULL << A1);
        board->mailbox[A1] = 6;

        board->pieces[3] |= (1ULL << D1);
        board->color[board->turn] |= (1ULL << D1);
        board->mailbox[D1] = 3;

        board->castling &= ~(1U << WHITE_QUEENSIDE);

        // Update Rook hash for castle
        board->hash ^= zobrist_table[(0 * 384) + (3 * 64) + A1];
        board->hash ^= zobrist_table[(0 * 384) + (3 * 64) + D1];
        break;

    case 4: // black king side
        board->pieces[3] &= ~(1ULL << H8);
        board->color[board->turn] &= ~(1ULL << H8);
        board->mailbox[H8] = 6;

        board->pieces[3] |= (1ULL << F8);
        board->color[board->turn] |= (1ULL << F8);
        board->mailbox[F8] = 3;

        board->castling &= ~(1U << BLACK_KINGSIDE);

        // Update Rook hash for castle
        board->hash ^= zobrist_table[(1 * 384) + (3 * 64) + H8];
        board->hash ^= zobrist_table[(1 * 384) + (3 * 64) + F8];
        break;

    case 3: // black queen side
        board->pieces[3] &= ~(1ULL << A8);
        board->color[board->turn] &= ~(1ULL << A8);
        board->mailbox[A8] = 6;

        board->pieces[3] |= (1ULL << D8);
        board->color[board->turn] |= (1ULL << D8);
        board->mailbox[D8] = 3;

        board->castling &= ~(1U << BLACK_QUEENSIDE);

        // Update Rook hash for castle
        board->hash ^= zobrist_table[(1 * 384) + (3 * 64) + A8];
        board->hash ^= zobrist_table[(1 * 384) + (3 * 64) + D8];
        break;

    case 5:
        piece = 1;
        break;
    case 6:
        piece = 2;
        break;
    case 7:
        piece = 3;
        break;
    case 8:
        piece = 4;
        break;

    default:
        break;
    }

    // remove piece from origin (use moving_piece, not piece)
    board->pieces[moving_piece] &= ~frombb;
    board->color[board->turn] &= ~frombb;
    board->mailbox[from] = 6;

    // en passant capture
    if (moving_piece == 0 && to == old_epsquare && old_epsquare != -1)
    {
        int captured_sq = to - (direction * 8);
        uint64_t capBB = 1ULL << captured_sq;

        board->pieces[0] &= ~capBB;
        board->color[!board->turn] &= ~capBB;
        board->mailbox[captured_sq] = 6;

        // Remove captured pawn from hash
        board->hash ^= zobrist_table[(!board->turn * 384) + (0 * 64) + captured_sq];
    }

    if (to == H1)
        board->castling &= ~(1U << WHITE_KINGSIDE);
    if (to == A1)
        board->castling &= ~(1U << WHITE_QUEENSIDE);
    if (to == H8)
        board->castling &= ~(1U << BLACK_KINGSIDE);
    if (to == A8)
        board->castling &= ~(1U << BLACK_QUEENSIDE);

    // clear destination square
    if (victim != 6) board->pieces[victim] &= ~tobb;
 

    board->color[!board->turn] &= ~tobb;

    // place moving piece (use piece, which is promoted type if applicable)
    board->pieces[piece] |= tobb;
    board->color[board->turn] |= tobb;
    board->mailbox[to] = piece;

    // Add piece to 'to' square in hash
    board->hash ^= zobrist_table[(board->turn * 384) + (piece * 64) + to];

    if (moving_piece == 0)
    {
        int from_y = from >> 3;
        int to_y = to >> 3;

        if (abs1(to_y - from_y) == 2)
            board->epsquare = from + (direction * 8);
    }

    // XOR IN new EP and Castling rights
    if (board->epsquare != -1)
        board->hash ^= zobrist_table[785 + (board->epsquare & 7)];
    board->hash ^= zobrist_table[769 + board->castling];

    // Toggle turn in hash
    board->hash ^= zobrist_table[768];

    // Update fullmove counter (increments after black's move)
    if (board->turn == 1)
    {
        board->fullmoves++;
    }

    board->turn ^= 1;
}

void moveint(Position *board, uint16_t move)
{
    MoveList tmp;
    tmp.movelist[0] = move;
    makeMove(board, &tmp, 0);
}

void captureMoves(Position *board, MoveList *list, bool color)
{
    uint64_t own = board->color[color];
    uint64_t enemies = board->color[!color];
    uint64_t occ = board->color[2];

    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;

    uint64_t pawns = board->pieces[PAWNNUMBER] & own;

    if (color == 0)
    {
        uint64_t cap_l = ((pawns & ~FILE_A) >> 9) & enemies;
        uint64_t cap_r = ((pawns & ~FILE_H) >> 7) & enemies;

        while (cap_l)
        {
            int to = pop_lsb(&cap_l);
            int from = to + 9;
            ADD_MOVE(list, from, to);
        }

        while (cap_r)
        {
            int to = pop_lsb(&cap_r);
            int from = to + 7;
            ADD_MOVE(list, from, to);
        }
    }
    else
    {
        uint64_t cap_l = ((pawns & ~FILE_A) << 7) & enemies;
        uint64_t cap_r = ((pawns & ~FILE_H) << 9) & enemies;

        while (cap_l)
        {
            int to = pop_lsb(&cap_l);
            int from = to - 7;
            ADD_MOVE(list, from, to);
        }

        while (cap_r)
        {
            int to = pop_lsb(&cap_r);
            int from = to - 9;
            ADD_MOVE(list, from, to);
        }
    }

    uint64_t knights = board->pieces[HORSENUMBER] & own;
    while (knights)
    {
        int from = pop_lsb(&knights);
        uint64_t attacks = knighttable[from] & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }

    uint64_t bishops = (board->pieces[BISHOPNUMBER] | board->pieces[QUEENNUMBER]) & own;
    while (bishops)
    {
        int from = pop_lsb(&bishops);
        uint64_t attacks = getbishopAttacks(from, occ) & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }

    uint64_t rooks = (board->pieces[ROOKNUMBER] | board->pieces[QUEENNUMBER]) & own;
    while (rooks)
    {
        int from = pop_lsb(&rooks);
        uint64_t attacks = getrookAttacks(from, occ) & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }

    uint64_t kings = board->pieces[KINGNUMBER] & own;
    if (kings)
    {
        int from = __builtin_ctzll(kings);
        uint64_t attacks = kingtable[from] & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            ADD_MOVE(list, from, to);
        }
    }
}

uint64_t perft(Position *board, int depth, int divide)
{
    if (depth == 0)
        return 1;

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);

    uint64_t nodes = 0;
    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board; // Stack allocation
        makeMove(&copy, &move_list, i);

        uint64_t move_nodes = perft(&copy, depth - 1, divide);
        nodes += move_nodes;

        if (depth == divide && divide != 0)
        {
            int from = (move_list.movelist[i] >> 6) & 0x3F;
            int to = move_list.movelist[i] & 0x3F;
            int flag = (move_list.movelist[i] >> 12) & 0xF;

            int x1 = from % 8;
            int y1 = 8 - (from / 8);
            int x2 = to % 8;
            int y2 = 8 - (to / 8);

            char promotion = 0;
            if (flag == 5)
                promotion = 'b';
            else if (flag == 6)
                promotion = 'n';
            else if (flag == 7)
                promotion = 'r';
            else if (flag == 8)
                promotion = 'q';

            if (promotion)
            {
                printf("%c%i%c%i%c - %" PRIu64 "\n",
                       'a' + x1, y1,
                       'a' + x2, y2,
                       promotion,
                       move_nodes);
            }
            else
            {
                printf("%c%i%c%i - %" PRIu64 "\n",
                       'a' + x1, y1,
                       'a' + x2, y2,
                       move_nodes);
            }
        }
    }
    return nodes;
}


uint64_t perftbulk(Position *board, int depth)
{
    if (depth == 0)
        return 1;

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);

    if (depth == 1)
    {
        return move_list.offset;
    }

    uint64_t nodes = 0;
    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board; // Stack allocation
        makeMove(&copy, &move_list, i);

        uint64_t move_nodes = perftbulk(&copy, depth - 1);
        nodes += move_nodes;
    }
    return nodes;
}
