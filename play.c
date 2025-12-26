#include "play.h"
#include "lmath.h"
#include "eval.h"

#define WHITE -1

// x+y*8
PawnMoves pawn(BitboardSet board,char color,char ind){
    char x = ind % 8;
    char y = (ind - x) / 8;
    PawnMoves output = {0};
    int dir = (color == WHITE) ? 1 : -1;
    output.data[0] = assessSquare(x-1 + (y + color) * 8,board);    //    B
    output.data[1] = assessSquare(x + (y + color) * 8,board);      // A  P  C
    output.data[2] = assessSquare(x+1 + (y + color) * 8,board);
    return output;
}

HorseMoves Horse(BitboardSet board ,char ind)
{
    char x = ind % 8;
    char y = (ind - x) / 8;
    HorseMoves output = {0};
    char values[8] = {
        assessSquare(x + 2 + (y + 1) * 8, board),
        assessSquare(x + 1 + (y + 2) * 8, board),
        assessSquare(x - 1 + (y + 2) * 8, board),
        assessSquare(x - 2 + (y + 1) * 8, board),
        assessSquare(x - 2 + (y - 1) * 8, board),
        assessSquare(x - 1 + (y - 2) * 8, board),
        assessSquare(x + 1 + (y - 2) * 8, board),
        assessSquare(x + 2 + (y - 1) * 8, board),
    };
    for (char i = 0; i < 8; i++)
    {
        output.data[i] = values[i];
    }
    return output;
}

BishopMoves Bishop(BitboardSet board,char ind){
    char x = ind % 8;
    char y = (ind - x) / 8;
}


void makeMove(BitboardSet board,char turn)
{

}
