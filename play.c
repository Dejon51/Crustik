#include "play.h"
#include "lmath.h"
#include "eval.h"
#include <stdbool.h>

/*public PackedMoveList getLegalMoves() {
    PackedMoveList moveList = MoveGenerator.generatePseudoLegalMoves(this);

    for (int i = 0; i < moveList.size(); i++) {
        long move = moveList.get(i);
        makeMove(move);
        if (isKingInCheck(!whiteTurn)) {
            moveList.remove(i);
            i--;
        }
        undoMove();

    }

    return moveList;
}*/

bool iskingcheck(BitboardSet *board, int ind)
{
}

Bitboard pawnMask(BitboardSet board, Bitboard *colorbitboard, bool color)
{
    Bitboard pawnmask = 0;
    for (int ind = 0; ind < 64; ind++)
    {
        if (color == is_set(colorbitboard, ind))
        {
            int x = ind % 8;
            int y = (ind - x) / 8;
            PawnMoves output = {0};
            int dir = (color == WHITE) ? 1 : -1;
            pawnmask = set_bit(pawnmask, x - 1 + (y + color) * 8, 1); //    C
            pawnmask = set_bit(pawnmask, x + (y + color) * 8, 1);     //    B
            pawnmask = set_bit(pawnmask, x + (y + color * 2) * 8, 1); // A  P  D
            pawnmask = set_bit(pawnmask, x + 1 + (y + color) * 8, 1);
        }
    }

    return pawnmask;
}

Bitboard horseMask(BitboardSet *board,, bool color)
{
    Bitboard horsemask = 0;
    for (int ind = 0; ind < 64; ind++)
    {
        if (color == is_set(whitebitboard, ind) && !is_set(blackbitboard, ind))
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
                    horsemask = set_bit(board, target, 1);
                }
            }
        }
    }
    return horsemask;
}

Bitboard bishopMask(BitboardSet *board,Bitboard *whitebitboard,Bitboard *blackbitboard, bool color)
{
    Bitboard bishopmask = {0};

    for (int ind = 0; ind < 64; ind++)
    {
        if (color == is_set(whitebitboard, ind) && !is_set(blackbitboard, ind))
        {
    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {1, 1, -1, -1};
    int dy[] = {1, -1, -1, 1};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++)
    {
        bool movego = 0; // reset for each direction

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

                int squareval = assessSquare(nx + ny * 8, board);

                if (squareval != 0)
                {
                    movego = 1;
                }

                int index = offset[dir] + i;

                if (movego) break;
            }
        }
    }
        }}
    return bishopmask;
}

Bitboard rookMask(BitboardSet *board, int ind)
{
    RookMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {0, 1, 0, -1};
    int dy[] = {1, 0, -1, 0};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++)
    {
        bool movego = 0; // reset for each direction

        for (int i = 1; i < 8; i++)
        {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            // Check bounds first
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
            {
                output.data[offset[dir] + i] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0)
            {
                movego = 1;
            }

            output.data[offset[dir] + i] = movego ? ILLEGALMOVE : squareval;
            output.move[i] = pack6(ind, nx + ny * 8);
        }
    }

    return output;
}

Bitboard queenMask(BitboardSet *board, int ind)
{
    QueenMoves output = {0};

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
                continue;
            }
            else
            {

                int squareval = assessSquare(nx + ny * 8, board);

                if (squareval != 0)
                {
                    movego = 1;
                }

                output.move[i] = pack6(ind, nx + ny * 8);
            }
        }
    }

    return output;
}

Bitboard kingMask(BitboardSet *board, int ind)
{
    KingMoves output = {0};

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
            output.move[i] = pack6(ind, nx + ny * 8);
        }
    }

    return output;
}

// x+y*8
PawnMoves pawnEval(BitboardSet *board, char color, int ind)
{
    int x = ind % 8;
    int y = (ind - x) / 8;
    PawnMoves output = {0};
    int dir = (color == WHITE) ? 1 : -1;
    output.data[0] = assessSquare(x - 1 + (y + color) * 8, board); //    C
    output.move[0] = pack6(ind, x - 1 + (y + color) * 8);
    output.data[1] = assessSquare(x + (y + color) * 8, board); //    B
    output.move[1] = pack6(ind, x + (y + color) * 8);
    output.data[2] = assessSquare(x + (y + color * 2) * 8, board); // A  P  D
    output.move[2] = pack6(ind, x + (y + color * 2) * 8);
    output.data[3] = assessSquare(x + 1 + (y + color) * 8, board);
    output.move[3] = pack6(ind, x + 1 + (y + color) * 8);

    return output;
}

HorseMoves horseEval(BitboardSet *board, int ind)
{
    const int x = ind % 8;
    const int y = (ind - x) / 8;

    static const struct
    {
        int dx;
        int dy;
    } offsets[] = {
        {2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};

    HorseMoves output = {0};

    for (int i = 0; i < 8; i++)
    {
        int target = (x + offsets[i].dx) + (y + offsets[i].dy) * 8;
        int nx = x + offsets[i].dx;
        int ny = y + offsets[i].dy;
        if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
        {
            output.data[i] = ILLEGALMOVE;
        }
        else
        {
            output.data[i] = assessSquare(target, board);
            output.move[i] = pack6(ind, target);
        }
    }

    return output;
}

BishopMoves bishopEval(BitboardSet *board, int ind)
{
    BishopMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {1, 1, -1, -1};
    int dy[] = {1, -1, -1, 1};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++)
    {
        bool movego = 0; // reset for each direction

        for (int i = 1; i < 8; i++)
        {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            // Check bounds first
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
            {
                output.data[offset[dir] + i] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0)
            {
                movego = 1;
            }

            output.data[offset[dir] + i] = movego ? ILLEGALMOVE : squareval;
            output.move[i] = pack6(ind, nx + ny * 8);
        }
    }

    return output;
}

RookMoves rookEval(BitboardSet *board, int ind)
{
    RookMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {0, 1, 0, -1};
    int dy[] = {1, 0, -1, 0};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++)
    {
        bool movego = 0; // reset for each direction

        for (int i = 1; i < 8; i++)
        {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            // Check bounds first
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
            {
                output.data[offset[dir] + i] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0)
            {
                movego = 1;
            }

            output.data[offset[dir] + i] = movego ? ILLEGALMOVE : squareval;
            output.move[i] = pack6(ind, nx + ny * 8);
        }
    }

    return output;
}

QueenMoves queenEval(BitboardSet *board, int ind)
{
    QueenMoves output = {0};

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
                output.data[index] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0)
            {
                movego = 1;
            }

            output.data[index] = movego ? ILLEGALMOVE : squareval;
            output.move[i] = pack6(ind, nx + ny * 8);
        }
    }

    return output;
}

KingMoves kingEval(BitboardSet *board, int ind)
{
    KingMoves output = {0};

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
            output.data[i] = ILLEGALMOVE;
            continue;
        }

        output.data[i] = assessSquare(nx + ny * 8, board);
        output.move[i] = pack6(ind, nx + ny * 8);
    }

    return output;
}

void legalMoveGen(BitboardSet *board, int move, bool *turn)
{
    for (int i = 0; i < 63; i++)
    {
        PawnMoves pawnmoves = pawn(board, i, turn);
        HorseMoves horsemoves = horse(board, i);
        BishopMoves bishopmoves = bishop(board, i);
        RookMoves rookmoves = rook(board, i);
        QueenMoves queenmoves = queen(board, i);
        KingMoves kingmoves = king(board, i);
    }
}
void makeMove(BitboardSet *board, int move, bool *turn)
{
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
