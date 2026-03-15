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

    int direction = (color == 0) ? -1 : 1;
    uint64_t pawns = board->pieces[0] & board->color[color];

    while (pawns)
    {
        int ind = pop_lsb(&pawns);
        int x = ind & 7;
        int y = ind >> 3;
        int offsetx[4] = {-1, 0, 0, 1};
        int offsety[4] = {1, 1, 2, 1};
        for (int i = 0; i < 4; i++)
        {
            if (x + offsetx[i] < 0 || x + offsetx[i] >= 8 ||
                y + direction * offsety[i] < 0 || y + direction * offsety[i] >= 8)
            {
                continue;
            }
            else
            {
                int to = x + offsetx[i] + (y + direction * offsety[i]) * 8;

                if ((((board->color[1] >> to) & 1ULL) ||
                     ((board->color[0] >> to) & 1ULL)) &&
                    (i == 2 || i == 1))
                {
                    continue;
                }
                else if ((i == 0 || i == 3) && ((color == 0 && y == 1) || (color == 1 && y == 6)) && ((board->color[!color] >> to) & 1))
                {
                    // capture promotiontion
                    list->movelist[list->offset++] = (5U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (6U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (7U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (8U << 12) | ((ind & 63) << 6) | (to & 63);
                }
                else if (i == 1 && ((color == 0 && y == 1) || (color == 1 && y == 6)))
                {
                    // push promotiontion
                    list->movelist[list->offset++] = (5U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (6U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (7U << 12) | ((ind & 63) << 6) | (to & 63);
                    list->movelist[list->offset++] = (8U << 12) | ((ind & 63) << 6) | (to & 63);
                }
                else if (i == 1)
                {
                    list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                }
                else if (i == 2 && ((color == 0 && y == 6) || (color == 1 && y == 1)))
                {
                    if (!((board->color[color] >> (x + (y + direction * offsety[i - 1]) * 8)) & 1) &&
                        !((board->color[!color] >> (x + (y + direction * offsety[i - 1]) * 8)) & 1))
                    {
                        list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    }
                }
                else if (board->epsquare == to && board->epsquare != -1)
                {
                    int to_file = to & 7;
                    int from_rank = ind >> 3;
                    int captured_sq = to_file + from_rank * 8;
                    if (((board->pieces[0] >> captured_sq) & 1) && ((board->color[!color] >> captured_sq) & 1))
                    {
                        list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    }
                }
                else if (((board->color[color] >> to) & 1))
                {
                    continue;
                }
                else if (!((board->color[color] >> to) & 1) &&
                         !((board->color[!color] >> to) & 1))
                {
                    continue;
                }
                else
                {
                    list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    continue;
                }
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

void kingMoves(Position *board, MoveList *list, bool color)
{
    Bitboard danger = pawnMask(board, !color) |
                      bishopMask(board, !color) |
                      horseMask(board, !color) |
                      rookMask(board, !color) |
                      kingMask(board, !color);
    uint64_t kings = board->pieces[5] & board->color[color];
    uint64_t occupancy = board->color[0] | board->color[1];
    uint64_t nocastle = occupancy | danger;

    while (kings)
    {
        int ind = pop_lsb(&kings);
        // printf("Danger: %llu occupancy: %llu",occupancy,danger)
        if (color == 0 && ind == E1) // white king
        {
            // kingside
            if ((board->castling & (1U << WHITE_KINGSIDE)) &&
                (nocastle & 0x6000000000000000ULL) == 0ULL)
            {
                list->movelist[list->offset++] = (1U << 12) | ((ind & 63) << 6) | (G1 & 63);
            }
            // queenside
            if ((board->castling & (1U << WHITE_QUEENSIDE)) &&
                (nocastle & 0xc00000000000000ULL) == 0ULL && (occupancy & 0x200000000000000ULL) == 0ULL)
            {
                list->movelist[list->offset++] = (2U << 12) | ((ind & 63) << 6) | (C1 & 63);
            }
        }
        else if (color == 1 && ind == E8) // black king
        {
            // kingside
            if ((board->castling & (1U << BLACK_KINGSIDE)) &&
                (nocastle & 0x0000000000000060ULL) == 0ULL)
            {
                list->movelist[list->offset++] = (4U << 12) | ((ind & 63) << 6) | (G8 & 63);
            }
            // queenside

            if ((board->castling & (1U << BLACK_QUEENSIDE)) &&
                (nocastle & 0xCULL) == 0ULL && (occupancy & 0x2ULL) == 0ULL)
            {
                list->movelist[list->offset++] = (3U << 12) | ((ind & 63) << 6) | (C8 & 63);
            }
        }
        // printf("%i %i %i\n",ind,E1,color);

        uint64_t attacks = kingtable[ind];

        attacks &= ~board->color[color];

        while (attacks)
        {
            int target = pop_lsb(&attacks);
            list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
        }
    }
}

// x+y*8

void legalMoveGen(Position *board, MoveList *list)
{
    MoveList pseudo = {0};
    bool turn = board->turn;
    pawnMoves(board, &pseudo, turn);
    bishopMoves(board, &pseudo, turn);
    horseMoves(board, &pseudo, turn);
    rookMoves(board, &pseudo, turn);
    kingMoves(board, &pseudo, turn);

    uint64_t king_bb = board->pieces[5] & board->color[turn];
    int current_king_pos = pop_lsb(&king_bb);
    bool in_check = squareAttacked(board, current_king_pos, !turn);

    for (int i = 0; i < pseudo.offset; i++)
    {
        int flag = (pseudo.movelist[i] >> 12) & 0xF;
        int to = pseudo.movelist[i] & 0x3F;

        if ((board->pieces[KINGNUMBER] >> to) & 1)
            continue;

        if (in_check && flag >= 1 && flag <= 4)
            continue;

        Position copy = *board;
        makeMove(&copy, &pseudo, i);

        uint64_t our_king = copy.pieces[5] & copy.color[turn];
        if (!our_king)
            continue; // safety net
        int king_pos = pop_lsb(&our_king);

        if (!squareAttacked(&copy, king_pos, !turn))
        {
            list->movelist[list->offset++] = pseudo.movelist[i];
        }
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
    if (moving_piece == 0 && to == old_epsquare)
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

    board->turn ^= 1;
}

void moveint(Position *board, uint16_t move) {
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
    uint64_t nodes = 0;
    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        uint64_t move_nodes = perft(&copy, depth - 1);
        nodes += move_nodes;

        // if (depth == 5)
        // {
        //     int from = (move_list.movelist[i] >> 6) & 0x3F;
        //     int to = move_list.movelist[i] & 0x3F;
        //     int flag = (move_list.movelist[i] >> 12) & 0xF;

        //     int x1 = from % 8;
        //     int y1 = 8 - (from / 8);
        //     int x2 = to % 8;
        //     int y2 = 8 - (to / 8);

        //     char promotion = 0;
        //     if (flag == 5)
        //         promotion = 'b';
        //     else if (flag == 6)
        //         promotion = 'n';
        //     else if (flag == 7)
        //         promotion = 'r';
        //     else if (flag == 8)
        //         promotion = 'q';

        //     if (promotion)
        //         printf("%c%i%c%i%c - %llu\n", 'a' + x1, y1, 'a' + x2, y2, promotion, move_nodes);
        //     else
        //         printf("%c%i%c%i - %llu\n", 'a' + x1, y1, 'a' + x2, y2, move_nodes);
        // }
    }
    return nodes;
}
