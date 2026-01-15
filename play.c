#include <stdbool.h>
#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"

/*public PackedMoveList getLegalMoves() {
    PackedMoveList moveList = MoveGenerator.generatePseudoLegalMoves(this);

    for (int i = 0; i < moveList->size(); i++) {
        long move = moveList->get(i);
        makeMove(move);
        if (isKingInCheck(!whiteTurn)) {
            moveList->remove(i);
            i--;
        }
        undoMove();

    }

    return moveList;
}*/
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
            pawnmask |= (1ULL << (x - 1 + (y + color) * 8));
            pawnmask |= (1ULL << (x + 1 + (y + color) * 8));
        }
    }

    return pawnmask;
}

Bitboard horseMask(Position *board, bool color)
{
    Bitboard horsemask = 0ULL;
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[2], ind))

        {

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
    }
    return horsemask;
}

Bitboard bishopMask(Position *board, bool color)
{
    Bitboard bishopmask = 0ULL;

    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[1], ind))
        {
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

                        // Occupied
                        if ((board->color[0] >> nx + ny * 8) & 1ULL || (board->color[1] >> nx + ny * 8) & 1ULL)
                        {
                            break;
                        }

                        bishopmask |= (1ULL << (nx + ny * 8));
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

    for (int ind = 0; ind < 64; ind++)
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[3], ind))
        {
            {
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
                            if ((board->color[0] >> nx + ny * 8) & 1ULL || (board->color[1] >> nx + ny * 8) & 1ULL)
                            {
                                break;
                            }

                            rookmask |= (1ULL << (nx + ny * 8));
                        }
                    }
                }
            }
        }
    return rookmask;
}

Bitboard queenMask(Position *board, bool color)
{
    Bitboard queenmask = 0ULL;

    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[4], ind))
        {

            int x = ind % 8;
            int y = ind / 8;

            // Directions:
            // N, E, S, W, NE, SE, SW, NW
            int dx[] = {0, 1, 0, -1, 1, 1, -1, -1};
            int dy[] = {1, 0, -1, 0, 1, -1, -1, 1};

            for (int dir = 0; dir < 8; dir++)
            {
                bool movego = 0;

                for (int i = 1; i < 8; i++)
                {
                    int nx = x + dx[dir] * i;
                    int ny = y + dy[dir] * i;

                    int index = dir * 7 + i;

                    if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                    {
                        break;
                    }
                    else
                    {
                        if ((board->color[0] >> nx + ny * 8) & 1ULL || (board->color[1] >> nx + ny * 8) & 1ULL)
                        {
                            break;
                        }

                        queenmask |= (1ULL << (nx + ny * 8));
                    }
                }
            }
        }
    }
    return queenmask;
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

                printf("from:%i to:%i\n", ind, to);

                if ((((board->color[1] >> to) & 1ULL) ||
                     ((board->color[0] >> to) & 1ULL)) &&
                    (i == 2 || i == 1))
                {
                    printf("Im not taking this");
                    continue;
                }
                else if (i == 1)
                {
                    list->movelist[list->offset] = ((ind & 63) << 6) | (to & 63);
                    list->offset++;
                }
                else if (i == 2 && (y == 1 || y == 6))
                {
                    if (!is_set(board->color[color], offsetx[i] + (y + direction * offsety[i - 1]) * 8) &&
                        !is_set(board->color[!color], offsetx[i] + (y + direction * offsety[i - 1]) * 8))
                    {
                        list->movelist[list->offset] = ((ind & 63) << 6) | (to & 63);
                        list->offset++;
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
                    list->movelist[list->offset] = ((ind & 63) << 6) | (to & 63);
                    list->offset++;
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
                    list->movelist[list->offset] = ((ind & 63) << 6) | (target & 63);
                    list->offset++;
                }
            }
        }
    }
}

void bishopMoves(Position *board, MoveList *list, bool color)
{
    uint64_t bishop = board->pieces[1] & board->color[color];
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

                    // Occupied
                    if ((board->color[1] >> nx + ny * 8) & 1ULL || (board->color[0] >> nx + ny * 8) & 1ULL)
                    {
                        break;
                    }

                    if (!is_set(board->color[color], nx + ny * 8))
                    {
                        printf("pawne");
                        list->movelist[list->offset] = ((ind & 63) << 6) | (nx + ny * 8 & 63);
                        list->offset++;
                    }
                }
            }
        }
    }
}

void rookMoves(Position *board, MoveList *list, bool color)
{
    uint64_t rook = board->pieces[3] & board->color[color];
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
                    if ((board->color[1] >> nx + ny * 8) & 1ULL || (board->color[0] >> nx + ny * 8) & 1ULL)
                    {
                        break;
                    }

                    if (!is_set(board->color[color], nx + ny * 8))
                    {
                        list->movelist[list->offset] = ((ind & 63) << 6) | (nx + ny * 8 & 63);
                        list->offset++;
                    }
                }
            }
        }
    }
}

void queenMoves(Position *board, MoveList *list, bool color)
{
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[4], ind))
        {
            Bitboard queenmask = 0ULL;

            int x = ind % 8;
            int y = ind / 8;

            // Directions:
            // N, E, S, W, NE, SE, SW, NW
            int dx[] = {0, 1, 0, -1, 1, 1, -1, -1};
            int dy[] = {1, 0, -1, 0, 1, -1, -1, 1};

            for (int dir = 0; dir < 8; dir++)
            {
                bool movego = 0;

                for (int i = 1; i < 8; i++)
                {
                    int nx = x + dx[dir] * i;
                    int ny = y + dy[dir] * i;

                    int index = dir * 7 + i;

                    if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                    {
                        break;
                    }
                    else
                    {
                        if ((board->color[1] >> nx + ny * 8) & 1ULL || (board->color[0] >> nx + ny * 8) & 1ULL)
                        {
                            break;
                        }

                        if (!is_set(board->color[color], nx + ny * 8))
                        {
                            list->movelist[list->offset] = ((ind & 63) << 6) | (nx + ny * 8 & 63);
                            list->offset++;
                        }
                    }
                }
            }
        }
    }
}

void kingMoves(Position *board, MoveList *list, bool color)
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
                    if (!is_set(board->color[color], nx + ny * 8))
                    {
                        list->movelist[list->offset] = ((ind & 63) << 6) | (nx + ny * 8 & 63);
                        list->offset++;
                    }
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
                      queenMask(board, color) |
                      kingMask(board, color);
    return (danger & (1ULL << ind)) != 0;
}

// x+y*8

void legalMoveGen(Position *board, MoveList *list, bool turn)
{
    pawnMoves(board, list, turn);
    bishopMoves(board, list, turn);
    horseMoves(board, list, turn);
    rookMoves(board, list, turn);
    queenMoves(board, list, turn);
    kingMoves(board, list, turn);
}

void makeMove(Position *board, MoveList *list, int move)
{
    int to = (list->movelist[move] & 0x3F);
    int from = ((list->movelist[move] >> 6) & 0x3F);
    if (board->pieces[0] & (1ULL << from))
    {
        printf("Makemove PAWN: %i,%i\n", from, to);
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

        print_bytes(board->pieces[0]);
    }
    else if (board->pieces[1] & (1ULL << from))
    {
        printf("Makemove: %i,%i\n", from, to);
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
        printf("Makemove: %i,%i\n", from, to);
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
        printf("Makemove: %i,%i\n", from, to);
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
        printf("Makemove: %i,%i\n", from, to);
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
        printf("Makemove: %i,%i\n", from, to);
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
}

// u64 Perft(int depth)
// {
//   MOVE move_list[256];
//   int n_moves, i;
//   u64 nodes = 0;

//   if (depth == 0)
//     return 1ULL;

//   n_moves = GenerateLegalMoves(move_list);
//   for (i = 0; i < n_moves; i++) {
//     MakeMove(move_list[i]);
//     nodes += Perft(depth - 1);
//     UndoMove(move_list[i]);
//   }
//   return nodes;
// }
