#include "play.h"
#include "lmath.h"
#include "eval.h"
#include <stdbool.h>

#define ILLEGALMOVE 42

#define WHITE -1

// x+y*8
PawnMoves pawn(BitboardSet board,char color,int ind){
    int x = ind % 8;
    int y = (ind - x) / 8;
    PawnMoves output = {0};
    int dir = (color == WHITE) ? 1 : -1;
    output.data[0] = assessSquare(x-1 + (y + color) * 8,board);    //    B
    output.data[1] = assessSquare(x + (y + color) * 8,board);      // A  P  C
    output.data[2] = assessSquare(x+1 + (y + color) * 8,board);
    return output;
}

HorseMoves Horse(BitboardSet board, int ind)
{
    const int x = ind % 8;
    const int y = (ind - x) / 8;

    static const struct { int dx; int dy; } offsets[] = {
        { 2,  1}, { 1,  2}, {-1,  2}, {-2,  1},
        {-2, -1}, {-1, -2}, { 1, -2}, { 2, -1}
    };

    HorseMoves output = {0};

    for (int i = 0; i < 8; i++)
    {   
        int target = (x + offsets[i].dx) + (y + offsets[i].dy) * 8;
        int nx = x + offsets[i].dx;
        int ny = y + offsets[i].dy;
        if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8){
            output.data[i] = ILLEGALMOVE;
        }
        else{
            output.data[i] = assessSquare(target, board);
        }
    }

    return output;
}

BishopMoves Bishop(BitboardSet board, int ind) {
    BishopMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {1, 1, -1, -1};
    int dy[] = {1, -1, -1, 1};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++) {
        bool movego = 0; // reset for each direction

        for (int i = 1; i < 8; i++) {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            // Check bounds first
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
                output.data[offset[dir] + i] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0) {
                movego = 1;
            }

            output.data[offset[dir] + i] = movego ? ILLEGALMOVE : squareval;
        }
    }

    return output;
}

RookMoves Rook(BitboardSet board,int ind){
    RookMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Direction vectors: NE, SE, SW, NW
    int dx[] = {0, 1, 0, -1};
    int dy[] = {1, 0, -1, 0};
    int offset[] = {0, 7, 14, 21};

    for (int dir = 0; dir < 4; dir++) {
        bool movego = 0; // reset for each direction

        for (int i = 1; i < 8; i++) {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            // Check bounds first
            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
                output.data[offset[dir] + i] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0) {
                movego = 1;
            }

            output.data[offset[dir] + i] = movego ? ILLEGALMOVE : squareval;
        }
    }

    return output;
}

QueenMoves Queen(BitboardSet board, int ind) {
    QueenMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    // Directions:
    // N, E, S, W, NE, SE, SW, NW
    int dx[] = { 0,  1,  0, -1,  1,  1, -1, -1 };
    int dy[] = { 1,  0, -1,  0,  1, -1, -1,  1 };

    for (int dir = 0; dir < 8; dir++) {
        bool movego = 0;

        for (int i = 1; i < 8; i++) {
            int nx = x + dx[dir] * i;
            int ny = y + dy[dir] * i;

            int index = dir * 7 + i;

            if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
                output.data[index] = ILLEGALMOVE;
                continue;
            }

            int squareval = assessSquare(nx + ny * 8, board);

            if (squareval != 0) {
                movego = 1;
            }

            output.data[index] = movego ? ILLEGALMOVE : squareval;
        }
    }

    return output;
}

KingMoves King(BitboardSet board, int ind) {
    KingMoves output = {0};

    int x = ind % 8;
    int y = ind / 8;

    int dx[] = { -1,  0,  1, -1, 1, -1, 0, 1 };
    int dy[] = { -1, -1, -1,  0, 0,  1, 1, 1 };

    for (int i = 0; i < 8; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];

        if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) {
            output.data[i] = ILLEGALMOVE;
            continue;
        }

        output.data[i] = assessSquare(nx + ny * 8, board);
    }

    return output;
}

void makeMove(BitboardSet board,bool turn)
{

}
