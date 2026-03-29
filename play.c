#include <stdbool.h>
#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"
#include "uci.h"

void print_bytes(uint64_t value)
{
    // Determine the number of bits in a long without limits.h
    // The C standard guarantees that a byte has at least 8 bits,
    // so we can use a constant 8 for common systems.
    // A more robust way to find CHAR_BIT without limits.h is complex,
    // but for the sake of simplicity and common environments, we assume 8.
    const int bits_in_byte = 8;
    int total_bits = sizeof(long) * bits_in_byte;

    // Use an unsigned long to avoid issues with right-shifting signed numbers.
    unsigned long mask = 1UL << (total_bits - 1);

    for (int i = 0; i < total_bits; i++)
    {
        // Use bitwise AND to check the current bit
        if (value & mask)
        {
            printf("1 ");
        }
        else
        {
            printf("0 ");
        }
        // Right shift the mask to check the next bit
        mask >>= 1;

        // Optional: Add a space every 8 bits for readability (byte separation)
        if ((i + 1) % bits_in_byte == 0 && (i + 1) != total_bits)
        {
            printf("\n");
        }
    }
    printf("\n");
}

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

    uint64_t occupancy = board->color[0] | board->color[1];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getbishopAttacks(ind, occupancy);

        attacks &= ~board->color[color];

        bishopmask |= attacks;
    }

    return bishopmask;
}

Bitboard rookMask(Position *board, bool color)
{
    Bitboard rookmask = 0ULL;

    uint64_t sliding = (board->pieces[3] | board->pieces[4]) & board->color[color];

    uint64_t occupancy = board->color[0] | board->color[1];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getrookAttacks(ind, occupancy);

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

void pawnMoves(Position *board, MoveList *list, bool color)
{
    uint64_t pawns = board->pieces[0] & board->color[color];
    if (!pawns) return;

    uint64_t empty = ~(board->color[0] | board->color[1]);
    uint64_t enemies = board->color[!color];

    if (board->epsquare != -1) {
        int cap_sq = board->epsquare + (color == 0 ? 8 : -8);
        if ((board->pieces[0] & board->color[!color]) & (1ULL << cap_sq)) {
            enemies |= (1ULL << board->epsquare);
        }
    }

    // Bitboard constants for bounds checking and rank masks
    const uint64_t FILE_A = 0x0101010101010101ULL;
    const uint64_t FILE_H = 0x8080808080808080ULL;
    const uint64_t RANK_7 = 0x000000000000FF00ULL;
    const uint64_t RANK_6 = 0x0000000000FF0000ULL;
    const uint64_t RANK_3 = 0x0000FF0000000000ULL;
    const uint64_t RANK_2 = 0x00FF000000000000ULL;

    if (color == 0) 
    {
        uint64_t norm_pawns = pawns & ~RANK_7;
        uint64_t prom_pawns = pawns & RANK_7;

        uint64_t single_push = (norm_pawns >> 8) & empty;
        uint64_t double_push = ((single_push & RANK_3) >> 8) & empty;
        uint64_t capture_l   = ((norm_pawns & ~FILE_A) >> 9) & enemies;
        uint64_t capture_r   = ((norm_pawns & ~FILE_H) >> 7) & enemies;

        while (single_push) {
            int to = pop_lsb(&single_push);
            int from = to + 8;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (double_push) {
            int to = pop_lsb(&double_push);
            int from = to + 16;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (capture_l) {
            int to = pop_lsb(&capture_l);
            int from = to + 9;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (capture_r) {
            int to = pop_lsb(&capture_r);
            int from = to + 7;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }

        // Promotions
        if (prom_pawns) {
            uint64_t prom_single    = (prom_pawns >> 8) & empty;
            uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) >> 9) & enemies;
            uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) >> 7) & enemies;

            while (prom_single) {
                int to = pop_lsb(&prom_single);
                int from = to + 8;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
            
            while (prom_capture_l) {
                int to = pop_lsb(&prom_capture_l);
                int from = to + 9;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
            
            while (prom_capture_r) {
                int to = pop_lsb(&prom_capture_r);
                int from = to + 7;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
        }
    }
    else 
    {
        uint64_t norm_pawns = pawns & ~RANK_2;
        uint64_t prom_pawns = pawns & RANK_2;

        uint64_t single_push = (norm_pawns << 8) & empty;
        uint64_t double_push = ((single_push & RANK_6) << 8) & empty;
        uint64_t capture_l   = ((norm_pawns & ~FILE_A) << 7) & enemies;
        uint64_t capture_r   = ((norm_pawns & ~FILE_H) << 9) & enemies;

        while (single_push) {
            int to = pop_lsb(&single_push);
            int from = to - 8;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (double_push) {
            int to = pop_lsb(&double_push);
            int from = to - 16;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (capture_l) {
            int to = pop_lsb(&capture_l);
            int from = to - 7;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
        
        while (capture_r) {
            int to = pop_lsb(&capture_r);
            int from = to - 9;
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }

        if (prom_pawns) {
            uint64_t prom_single    = (prom_pawns << 8) & empty;
            uint64_t prom_capture_l = ((prom_pawns & ~FILE_A) << 7) & enemies;
            uint64_t prom_capture_r = ((prom_pawns & ~FILE_H) << 9) & enemies;

            while (prom_single) {
                int to = pop_lsb(&prom_single);
                int from = to - 8;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
            
            while (prom_capture_l) {
                int to = pop_lsb(&prom_capture_l);
                int from = to - 7;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
            
            while (prom_capture_r) {
                int to = pop_lsb(&prom_capture_r);
                int from = to - 9;
                list->movelist[list->offset++] = (5U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (6U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (7U << 12) | ((from & 63) << 6) | (to & 63);
                list->movelist[list->offset++] = (8U << 12) | ((from & 63) << 6) | (to & 63);
            }
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
            list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
        }
    }
}

void bishopMoves(Position *board, MoveList *list, bool color)
{
    uint64_t sliding = (board->pieces[1] | board->pieces[4]) & board->color[color];

    uint64_t occupancy = board->color[0] | board->color[1];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getbishopAttacks(ind, occupancy);

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
        }
    }
}

void rookMoves(Position *board, MoveList *list, bool color)
{
    uint64_t sliding = (board->pieces[3] | board->pieces[4]) & board->color[color];

    uint64_t occupancy = board->color[0] | board->color[1];

    while (sliding)
    {
        int ind = pop_lsb(&sliding);

        uint64_t attacks = getrookAttacks(ind, occupancy);

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
        }
    }
}

bool squareAttacked_custom(Position *b, int sq, int enemy, uint64_t custom_occ)
{
    uint64_t enemyPieces = b->color[enemy];

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
    if (!kings)
        return;

    int from = __builtin_ctzll(kings);
    uint64_t occupancy = board->color[0] | board->color[1];

    uint64_t attacks = kingtable[from];
    attacks &= ~board->color[us];

    while (attacks)
    {
        int to = pop_lsb(&attacks);
        list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
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
} // x+y*8

uint64_t get_checkers(Position *board, int sq, int enemy_color)
{
    uint64_t occ = board->color[0] | board->color[1];
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

void legalMoveGen(Position *board, MoveList *list)
{
    int us = board->turn;
    int them = !us;
    uint64_t king_bb = board->pieces[5] & board->color[us];
    if (!king_bb)
        return;

    int king_sq = __builtin_ctzll(king_bb);
    uint64_t occ = board->color[0] | board->color[1];

    uint64_t checkers = get_checkers(board, king_sq, them);
    int check_count = __builtin_popcountll(checkers);

    uint64_t check_mask = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t pinned_pieces = 0;
    uint64_t pinner_ray[64] = {0};

    if (check_count == 1)
    {
        int checker_sq = __builtin_ctzll(checkers);

        check_mask = ray_between_table[king_sq * 64 + checker_sq] | (1ULL << checker_sq);
    }
    else if (check_count >= 2)
    {
        check_mask = 0; // Only King moves allowed
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

    MoveList pseudo = {0};
    kingMoves(board, &pseudo, us, check_count);

    if (check_count < 2)
    {
        pawnMoves(board, &pseudo, us);
        horseMoves(board, &pseudo, us);
        bishopMoves(board, &pseudo, us);
        rookMoves(board, &pseudo, us);
    }

    for (int i = 0; i < pseudo.offset; i++)
    {
        uint16_t move = pseudo.movelist[i];
        int from = (move >> 6) & 0x3F;
        int to = move & 0x3F;
        int flag = (move >> 12) & 0xF;
        uint64_t from_bb = 1ULL << from;
        uint64_t to_bb = 1ULL << to;

        if (from == king_sq)
        {
            uint64_t occ_without_king = occ & ~from_bb;
            if (!squareAttacked_custom(board, to, them, (occ_without_king | to_bb)))
            {
                list->movelist[list->offset++] = move;
            }
            continue;
        }

        if (from_bb & pinned_pieces)
        {
            if (!(to_bb & pinner_ray[from]))
                continue;
        }

        if (!(to_bb & check_mask))
            continue;

        if (board->mailbox[from] == 0 && (to == board->epsquare && board->epsquare != -1))
        {
            int cap_sq = to + (us == 0 ? 8 : -8);
            uint64_t occ_after_ep = occ & ~from_bb & ~(1ULL << cap_sq) | to_bb;
            if (squareAttacked_custom(board, king_sq, them, occ_after_ep))
                continue;
        }

        // Move is verified legal!
        list->movelist[list->offset++] = move;
    }
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
    board->epsquare = -1;

    int piece = board->mailbox[from];
    int moving_piece = piece;

    if (piece == 6)
        return;

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
        break;

    case 2: // white queen side
        board->pieces[3] &= ~(1ULL << A1);
        board->color[board->turn] &= ~(1ULL << A1);
        board->mailbox[A1] = 6;

        board->pieces[3] |= (1ULL << D1);
        board->color[board->turn] |= (1ULL << D1);
        board->mailbox[D1] = 3;

        board->castling &= ~(1U << WHITE_QUEENSIDE);
        break;

    case 4: // black king side
        board->pieces[3] &= ~(1ULL << H8);
        board->color[board->turn] &= ~(1ULL << H8);
        board->mailbox[H8] = 6;

        board->pieces[3] |= (1ULL << F8);
        board->color[board->turn] |= (1ULL << F8);
        board->mailbox[F8] = 3;

        board->castling &= ~(1U << BLACK_KINGSIDE);
        break;

    case 3: // black queen side
        board->pieces[3] &= ~(1ULL << A8);
        board->color[board->turn] &= ~(1ULL << A8);
        board->mailbox[A8] = 6;

        board->pieces[3] |= (1ULL << D8);
        board->color[board->turn] |= (1ULL << D8);
        board->mailbox[D8] = 3;

        board->castling &= ~(1U << BLACK_QUEENSIDE);
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
        int captured_sq = to - (direction << 3);
        uint64_t capBB = 1ULL << captured_sq;

        board->pieces[0] &= ~capBB;
        board->color[!board->turn] &= ~capBB;
        board->mailbox[captured_sq] = 6;
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
    for (int i = 0; i < 6; i++)
        board->pieces[i] &= ~tobb;

    board->color[!board->turn] &= ~tobb;

    // place moving piece (use piece, which is promoted type if applicable)
    board->pieces[piece] |= tobb;
    board->color[board->turn] |= tobb;
    board->mailbox[to] = piece;

    if (moving_piece == 0)
    {
        int from_y = from >> 3;
        int to_y = to >> 3;

        if (abs1(to_y - from_y) == 2)
            board->epsquare = from + (direction << 3);
    }

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
    Bitboard enemies = board->color[!color];
    Bitboard occ = board->color[0] | board->color[1];

    Bitboard pawn_caps = pawnMask(board, color) & enemies;
    while (pawn_caps)
    {
        int to = pop_lsb(&pawn_caps);
        Bitboard pawns = board->pieces[0] & board->color[color];
        while (pawns)
        {
            int from = pop_lsb(&pawns);
            Bitboard single = (color ? white_pawn_attacks[from] : black_pawn_attacks[from]);
            if (single & (1ULL << to))
                list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
    }

    Bitboard knights = board->pieces[2] & board->color[color];
    while (knights)
    {
        int from = pop_lsb(&knights);
        Bitboard attacks = knighttable[from] & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
    }

    Bitboard bishops = (board->pieces[1] | board->pieces[4]) & board->color[color];
    while (bishops)
    {
        int from = pop_lsb(&bishops);
        Bitboard attacks = getbishopAttacks(from, occ) & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
    }

    Bitboard rooks = (board->pieces[3] | board->pieces[4]) & board->color[color];
    while (rooks)
    {
        int from = pop_lsb(&rooks);
        Bitboard attacks = getrookAttacks(from, occ) & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
    }

    Bitboard kings = board->pieces[5] & board->color[color];
    if (kings)
    {
        int from = __builtin_ctzll(kings);
        Bitboard attacks = kingtable[from] & enemies;
        while (attacks)
        {
            int to = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((from & 63) << 6) | (to & 63);
        }
    }
}
// void unmakeMove(Position *board, MoveList *list, int move)
// {
//     int direction = (board->turn == 0) ? -1 : 1;

//     int to = list->movelist[move] & 0x3F;
//     int from = (list->movelist[move] >> 6) & 0x3F;
//     int flag = (list->movelist[move] >> 12) & 0xF;
//     // printf("From %i To %i Index %i\n",from,to,list->offset);
//     uint64_t frombb = 1ULL << from;
//     uint64_t tobb = 1ULL << to;

//     // get moving piece
//     int piece = 6;
//     piece = board->mailbox[to];
//     board->mailbox[to] = 6;

//     // remove piece from from
//     board->pieces[piece] &= ~tobb;
//     board->color[!board->turn] &= ~tobb;
//     board->mailbox[to] = piece;

//     // clear destination piece
//     for (int i = 0; i < 6; i++)
//         board->pieces[i] &= ~frombb;

//     board->color[board->turn] &= ~tobb;

//     // replace destination piece with other piece causing capture
//     board->pieces[piece] |= frombb;
//     board->color[!board->turn] |= frombb;
//     board->mailbox[from] = 6;

//     board->turn ^= 1;
// }

uint64_t perft(Position *board, int depth)
{
    if (depth == 0)
        return 1;

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);
    // if (depth==1)
    // {
    //     return move_list.offset;
    // }
    
    uint64_t nodes = 0;
    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        uint64_t move_nodes = perft(&copy, depth - 1);
        nodes += move_nodes;

    //     if (depth == 5)
    //     {
    //         int from = (move_list.movelist[i] >> 6) & 0x3F;
    //         int to = move_list.movelist[i] & 0x3F;
    //         int flag = (move_list.movelist[i] >> 12) & 0xF;

    //         int x1 = from % 8;
    //         int y1 = 8 - (from / 8);
    //         int x2 = to % 8;
    //         int y2 = 8 - (to / 8);

    //         char promotion = 0;
    //         if (flag == 5)
    //             promotion = 'b';
    //         else if (flag == 6)
    //             promotion = 'n';
    //         else if (flag == 7)
    //             promotion = 'r';
    //         else if (flag == 8)
    //             promotion = 'q';

    //         if (promotion)
    //             printf("%c%i%c%i%c - %llu\n", 'a' + x1, y1, 'a' + x2, y2, promotion, move_nodes);
    //         else
    //             printf("%c%i%c%i - %llu\n", 'a' + x1, y1, 'a' + x2, y2, move_nodes);
    //     }
    // }
    }
    return nodes;
}