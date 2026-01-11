#include <stdbool.h>
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

Bitboard pawnMask(const Position *board, bool color)
{
    Bitboard pawnmask = 0ULL;
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[0], ind))
        {
            int x = ind % 8;
            int y = (ind - x) / 8;
            pawnmask |= (1ULL << (x - 1 + (y + color) * 8));
            pawnmask |= (1ULL << (x + 1 + (y + color) * 8));
        }
    }

    return pawnmask;
}

Bitboard horseMask(const Position *board, bool color)
{
    Bitboard horsemask = 0ULL;
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[2], ind))

        {

            const int x = ind % 8;
            const int y = (ind - x) / 8;

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

Bitboard bishopMask(const Position *board, bool color)
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

Bitboard rookMask(const Position *board, bool color)
{
    Bitboard rookmask = 0ULL;

    for (int ind = 0; ind < 64; ind++)
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[3], ind))
    {        {
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

Bitboard queenMask(const Position *board, bool color)
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

Bitboard kingMask(const Position *board, bool color)
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

void pawnMoves(const Position *board, MoveList *list, bool color)
{
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[0], ind))
        {
            int x = ind % 8;
            int y = (ind - x) / 8;
            int pos[4] = {x - 1 + (y + color) * 8,
                          x + (y + color) * 8,
                          x + (y + color * 2) * 8,
                          x + 1 + (y + color) * 8};
            for (int i = 0; i < 4; i++)
            {
                if (!is_set(board->color[color], pos[i]))
                {
                    list->movelist[list->offset] = ((ind & 63) << 6) | (pos[0] & 63);
                    list->offset++;
                }
            }
        }
    }

}

void horseMoves(const Position *board,MoveList *list, bool color)
{
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[2], ind))
        {

            const int x = ind % 8;
            const int y = (ind - x) / 8;

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
}

void bishopMoves(const Position *board,MoveList *list, bool color)
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

void rookMoves(const Position *board,MoveList *list, bool color)
{
    for (int ind = 0; ind < 64; ind++)
    {
        if ((color == is_set(board->color[0], ind) && !is_set(board->color[1], ind)) && is_set(board->pieces[3], ind))
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

void queenMoves(const Position *board,MoveList *list, bool color)
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

void kingMoves(const Position *board,MoveList *list, bool color)
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

bool iskingcheck(const Position *board, int ind, bool color)
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

MoveList* legalMoveGen(const Position *board,MoveList *list, bool turn)
{
    pawnMoves(board,list,turn);
    bishopMoves(board,list,turn);
    horseMoves(board,list,turn);
    rookMoves(board,list,turn);
    queenMoves(board,list,turn);
    kingMoves(board,list,turn);
    return list;
}
void makeMove(Position *board,MoveList *list, int move, bool turn){
    int from = (list->movelist[move] & 0x3F);
    int to = ((list->movelist[move] >> 6) & 0x3F);
    for (int i = 0; i < 6; i++)
    {
        if (board->pieces[i] & (1ULL << from)){
            board->pieces[i] &= ~(1ULL << from);
            board->pieces[i] |= (1ULL << to);
            break;
        }
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
