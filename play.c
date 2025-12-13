#include "play.h"
#include "lmath.h"
#include "eval.h"


// x+y*8
PawnMoves pawn(BitboardSet board,char ind){
    assessSquare(ind,board); 
}

MoveArray Horse(BitboardSet board ,char ind)
{
    MoveArray output;
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
    for (char i = 0; i < 27; i++)
    {
        output.data[i] = values[i];
    }
    return output;
}

void makeMove(BitboardSet board,char turn)
{

}
