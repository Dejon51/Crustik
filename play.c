#include <stdbool.h>
#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

void print_bytes(long value)
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
            printf("1");
        }
        else
        {
            printf("0");
        }
        // Right shift the mask to check the next bit
        mask >>= 1;

        // Optional: Add a space every 8 bits for readability (byte separation)
        if ((i + 1) % bits_in_byte == 0 && (i + 1) != total_bits)
        {
            printf(" ");
        }
    }
    printf("\n");
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
    Bitboard king = board->pieces[5];

    Bitboard kingscolor = king & board->color[color];

    return kingtable[__builtin_ctzll(kingscolor)];
}

void pawnMoves(Position *board, MoveList *list, bool color)
{

    int direction = (color == 0) ? -1 : 1;
    uint64_t pawns = board->pieces[0] & board->color[color];

    while (pawns)
    {
        int ind = pop_lsb(&pawns);
        int x = ind % 8;
        int y = ind / 8;
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
                else if (i == 1)
                {
                    list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                }
                else if (i == 2 && (y == 1 || y == 6))
                {
                    if (!((board->color[color] >> x + (y + direction * offsety[i - 1]) * 8) & 1) &&
                        !((board->color[!color] >> x + (y + direction * offsety[i - 1]) * 8) & 1))
                    {
                        list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    }
                }
                else if (board->epsquare == to && board->epsquare != -1)
                {
                    int to_file = to % 8;
                    int from_rank = ind / 8;
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

        int x = ind % 8;
        int y = ind / 8;

        static const struct
        {
            int dx;
            int dy;
        } offsets[] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};

        for (int i = 0; i < 8; i++)
        {
            int target = (x + offsets[i].dx) + (y + offsets[i].dy) * 8;
            int nx = x + offsets[i].dx;
            int ny = y + offsets[i].dy;
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
            {
                continue;
            }
            else
            {
                if (!((board->color[color] >> target) & 1))
                {

                    list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
                }
            }
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

    uint64_t king = board->pieces[5] & board->color[color];
    while (king)
    {
        int ind = pop_lsb(&king);
        int x = ind % 8;
        int y = ind / 8;

        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

        for (int i = 0; i < 8; i++)
        {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
            {
                continue;
            }
            else
            {
                if (!(board->color[color] >> (nx + ny * 8) & 1))
                {

                    list->movelist[list->offset++] = ((ind & 63) << 6) | (nx + ny * 8 & 63);
                }
            }
        }
    }
}

bool iskingcheck(Position *board, int ind, bool color)
{
    Bitboard danger = pawnMask(board, color) |
                      bishopMask(board, color) |
                      horseMask(board, color) |
                      rookMask(board, color) |
                      kingMask(board, color);
    return (danger & (1ULL << ind)) != 0;
}
// x+y*8

void legalMoveGen(Position *board, MoveList *list, bool turn)
{
    MoveList pseudo = {0};

    pawnMoves(board, &pseudo, turn);
    bishopMoves(board, &pseudo, turn);
    horseMoves(board, &pseudo, turn);
    rookMoves(board, &pseudo, turn);
    kingMoves(board, &pseudo, turn);

    for (int i = 0; i < pseudo.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &pseudo, i);

        uint64_t our_king = copy.pieces[5] & copy.color[turn];
        int king_pos = pop_lsb(&our_king);

        if (!iskingcheck(&copy, king_pos, !turn))
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

    uint64_t frombb = 1ULL << from;
    uint64_t tobb = 1ULL << to;

    int old_epsquare = board->epsquare;
    board->epsquare = -1;

    // get moving piece
    int piece = 6;
    board->mailbox[to] = 6;
    for (int i = 0; i < 6; i++)
        piece = board->mailbox[from];

    if (piece == 6)
        return; // return nothing if piecetype is empty

    // remove piece from from
    board->pieces[piece] &= ~frombb;
    board->color[board->turn] &= ~frombb;
    board->mailbox[from] = 6;

    // en passant stuff i dont wanna touch again
    if (piece == 0 && to == old_epsquare)
    {
        int captured_sq = to - 8 * direction;
        uint64_t capBB = 1ULL << captured_sq;

        board->pieces[0] &= ~capBB;
        board->color[!board->turn] &= ~capBB;
        board->mailbox[captured_sq] = 6;
    }

    // clear destination piece
    for (int i = 0; i < 6; i++)
        board->pieces[i] &= ~tobb;

    board->color[!board->turn] &= ~tobb;

    // replace destination piece with other piece causing capture
    board->pieces[piece] |= tobb;
    board->color[board->turn] |= tobb;
    board->mailbox[to] = piece;

    // handle double pawn pushes
    if (piece == 0)
    {
        int from_y = from / 8;
        int to_y = to / 8;

        if (abs1(to_y - from_y) == 2)
            board->epsquare = from + 8 * direction;
    }

    board->turn ^= 1;
}
void unMakeMove(Position *board, MoveList *move_list, int ind)
{
}

uint64_t perft(Position *board, int depth)
{
    if (depth == 0)
        return 1;

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list, board->turn);
    uint64_t nodes = 0;
    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &move_list, i);
        nodes += perft(&copy, depth - 1);
        // if (depth == 3)
        // {
        //     int from = (move_list.movelist[i] >> 6 & 0x3F);

        //     int to = (move_list.movelist[i] & 0x3F);
        //     int x1 = from % 8;
        //     int y1 = from / 8;
        //     int x2 = to % 8;
        //     int y2 = to / 8;
        //     printf("%c%i%c%i %li\n", 'a' + (x1), y1 + 1, 'a' + (x2), y2 + 1, nodes);
        // }
    }

    return nodes;
}