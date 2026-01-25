#include <stdbool.h>
#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"

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
    Bitboard pawnmask = 0ULL;
    int direction = (color == 0) ? -1 : 1;
    uint64_t pawns = board->pieces[0] & board->color[color];
    while (pawns)
    {
        int ind = pop_lsb(&pawns);
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[0], ind))
        {
            int x = ind % 8;
            int y = ind / 8;
            if (x - 1 >= 0 && y + direction >= 0 && y + direction < 8)
                pawnmask |= (1ULL << (x - 1 + (y + direction) * 8));
            if (x + 1 < 8 && y + direction >= 0 && y + direction < 8)
                pawnmask |= (1ULL << (x + 1 + (y + direction) * 8));
        }
    }

    return pawnmask;
}

Bitboard horseMask(Position *board, bool color)
{
    Bitboard horsemask = 0ULL;
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
                horsemask |= (1ULL << target);
            }
        }
    }
    return horsemask;
}

Bitboard bishopMask(Position *board, bool color)
{
    Bitboard bishopmask = 0ULL;

    uint64_t bishop = (board->pieces[1] & board->pieces[4]) & board->color[color];
    while (bishop)
    {
        int ind = pop_lsb(&bishop);

        int x = ind % 8;
        int y = ind / 8;

        // Direction vectors: NE, SE, SW, NW
        int dx[] = {1, 1, -1, -1};
        int dy[] = {1, -1, -1, 1};
        int offset[] = {0, 7, 14, 21};

        for (int dir = 0; dir < 4; dir++)
        {
            for (int i = 1; i < 8; i++)
            {
                int nx = x + dx[dir] * i;
                int ny = y + dy[dir] * i;

                // Check bounds first
                if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                {
                    break;
                }
                else
                {
                    bishopmask |= (1ULL << (nx + ny * 8));

                    // Occupied
                    if ((board->color[0] >> nx + ny * 8) & 1ULL || (board->color[1] >> nx + ny * 8) & 1ULL)
                    {
                        break;
                    }
                }
            }
        }
    }
    return bishopmask;
}

Bitboard rookMask(Position *board, bool color)
{
    Bitboard rookmask = 0ULL;
    uint64_t rook = (board->pieces[3] & board->pieces[4]) & board->color[color];
    while (rook)
    {
        int ind = pop_lsb(&rook);

        int x = ind % 8;
        int y = ind / 8;

        // Direction vectors: NE, SE, SW, NW
        int dx[] = {0, 1, 0, -1};
        int dy[] = {1, 0, -1, 0};
        int offset[] = {0, 7, 14, 21};

        for (int dir = 0; dir < 4; dir++)
        {
            for (int i = 1; i < 8; i++)
            {
                int nx = x + dx[dir] * i;
                int ny = y + dy[dir] * i;

                // Check bounds first
                if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                {
                    break;
                }
                else
                {
                    rookmask |= (1ULL << (nx + ny * 8));

                    if ((board->color[0] >> nx + ny * 8) & 1ULL || (board->color[1] >> nx + ny * 8) & 1ULL)
                    {
                        break;
                    }
                }
            }
        }
    }
    return rookmask;
}

Bitboard kingMask(Position *board, bool color)
{

    Bitboard kingmask = 0ULL;
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[5], ind))
        {
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
                    kingmask |= (1ULL << (nx + ny * 8));
                }
            }
        }
    }
    return kingmask;
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
                    if (!is_set(board->color[color], x + (y + direction * offsety[i - 1]) * 8) &&
                        !is_set(board->color[!color], x + (y + direction * offsety[i - 1]) * 8))
                    {
                        list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    }
                }
                else if (board->epsquare == to && board->epsquare != -1)
                {
                    int to_file = to % 8;
                    int from_rank = ind / 8;
                    int captured_sq = to_file + from_rank * 8;
                    if (is_set(board->pieces[0], captured_sq) && is_set(board->color[!color], captured_sq))
                    {
                        list->movelist[list->offset++] = ((ind & 63) << 6) | (to & 63);
                    }
                }
                else if (is_set(board->color[color], to))
                {
                    continue;
                }
                else if (!is_set(board->color[color], to) &&
                         !is_set(board->color[!color], to))
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
                if (!is_set(board->color[color], target))
                {

                    list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);
                }
            }
        }
    }
}

void bishopMoves(Position *board, MoveList *list, bool color)
{

    uint64_t bishop = (board->pieces[1] | board->pieces[4]) & board->color[color];
    while (bishop)
    {
        int ind = pop_lsb(&bishop);

        int x = ind % 8;
        int y = ind / 8;

        // Direction vectors: NE, SE, SW, NW
        int dx[] = {1, 1, -1, -1};
        int dy[] = {1, -1, -1, 1};
        int offset[] = {0, 7, 14, 21};

        for (int dir = 0; dir < 4; dir++)
        {
            for (int i = 1; i < 8; i++)
            {
                int nx = x + dx[dir] * i;
                int ny = y + dy[dir] * i;

                // Check bounds first
                if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                {
                    break;
                }
                int target = nx + ny * 8;
                if (is_set(board->color[color], target))
                {
                    break;
                }

                // Add the move
                list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);

                if (is_set(board->color[!color], target))
                {
                    break;
                }
            }
        }
    }
}

void rookMoves(Position *board, MoveList *list, bool color)
{

    uint64_t rook = (board->pieces[3] | board->pieces[4]) & board->color[color];
    while (rook)
    {
        int ind = pop_lsb(&rook);

        int x = ind % 8;
        int y = ind / 8;

        // Direction vectors: NE, SE, SW, NW
        int dx[] = {0, 1, 0, -1};
        int dy[] = {1, 0, -1, 0};
        int offset[] = {0, 7, 14, 21};

        for (int dir = 0; dir < 4; dir++)
        {
            for (int i = 1; i < 8; i++)
            {
                int nx = x + dx[dir] * i;
                int ny = y + dy[dir] * i;

                if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                {
                    break;
                }
                int target = nx + ny * 8;
                if (is_set(board->color[color], target))
                {
                    break;
                }

                // Add the move
                list->movelist[list->offset++] = ((ind & 63) << 6) | (target & 63);

                if (is_set(board->color[!color], target))
                {
                    break;
                }
            }
        }
    }
}

void kingMoves(Position *board, KList *list, bool color)
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
                if (!is_set(board->color[color], nx + ny * 8))
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
    KList klist = {0};

    // Generate all pseudo-legal moves
    pawnMoves(board, &pseudo, turn);
    bishopMoves(board, &pseudo, turn);
    horseMoves(board, &pseudo, turn);
    rookMoves(board, &pseudo, turn);
    kingMoves(board, &klist, turn);

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

    // Filter king moves
    for (int i = 0; i < klist.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &klist, i);

        int to = klist.movelist[i] & 0x3F;

        if (!iskingcheck(&copy, to, !turn))
        {
            list->movelist[list->offset++] = klist.movelist[i];
        }
    }
}

void makeMove(Position *board, MoveList *list, int move)
{
    int direction = (board->turn == 0) ? -1 : 1;
    int to = (list->movelist[move] & 0x3F);
    int from = ((list->movelist[move] >> 6) & 0x3F);

    // Store old epsquare before resetting
    int old_epsquare = board->epsquare;
    board->epsquare = -1;

    if (board->pieces[0] & (1ULL << from))
    {
        if (to == old_epsquare)
        {
            int captured_sq = to - 8 * direction;
            board->color[!board->turn] &= ~(1ULL << captured_sq);
            board->pieces[0] &= ~(1ULL << captured_sq);
        }

        int from_y = from / 8;
        int to_y = to / 8;
        if (abs(to_y - from_y) == 2)
        {
            board->epsquare = from + 8 * direction;
        }

        board->pieces[0] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] |= (1ULL << to);
        board->pieces[1] &= ~(1ULL << to);
        board->pieces[2] &= ~(1ULL << to);
        board->pieces[3] &= ~(1ULL << to);
        board->pieces[4] &= ~(1ULL << to);
        board->pieces[5] &= ~(1ULL << to);
        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }
    else if (board->pieces[1] & (1ULL << from))
    {
        board->pieces[1] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] &= ~(1ULL << to);
        board->pieces[1] |= (1ULL << to);
        board->pieces[2] &= ~(1ULL << to);
        board->pieces[3] &= ~(1ULL << to);
        board->pieces[4] &= ~(1ULL << to);
        board->pieces[5] &= ~(1ULL << to);
        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }
    else if (board->pieces[2] & (1ULL << from))
    {
        board->pieces[2] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] &= ~(1ULL << to);
        board->pieces[1] &= ~(1ULL << to);
        board->pieces[2] |= (1ULL << to);
        board->pieces[3] &= ~(1ULL << to);
        board->pieces[4] &= ~(1ULL << to);
        board->pieces[5] &= ~(1ULL << to);

        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }
    else if (board->pieces[3] & (1ULL << from))
    {
        board->pieces[3] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] &= ~(1ULL << to);
        board->pieces[1] &= ~(1ULL << to);
        board->pieces[2] &= ~(1ULL << to);
        board->pieces[3] |= (1ULL << to);
        board->pieces[4] &= ~(1ULL << to);
        board->pieces[5] &= ~(1ULL << to);
        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }
    else if (board->pieces[4] & (1ULL << from))
    {
        board->pieces[4] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] &= ~(1ULL << to);
        board->pieces[1] &= ~(1ULL << to);
        board->pieces[2] &= ~(1ULL << to);
        board->pieces[3] &= ~(1ULL << to);
        board->pieces[4] |= (1ULL << to);
        board->pieces[5] &= ~(1ULL << to);
        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }
    else if (board->pieces[5] & (1ULL << from))
    {
        board->pieces[5] &= ~(1ULL << from);
        board->color[board->turn] &= ~(1ULL << from);
        board->pieces[0] &= ~(1ULL << to);
        board->pieces[1] &= ~(1ULL << to);
        board->pieces[2] &= ~(1ULL << to);
        board->pieces[3] &= ~(1ULL << to);
        board->pieces[4] &= ~(1ULL << to);
        board->pieces[5] |= (1ULL << to);
        board->color[board->turn] |= (1ULL << to);
        board->color[!board->turn] &= ~(1ULL << to);
    }

    board->turn = !board->turn;
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