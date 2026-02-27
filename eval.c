#include "eval.h"
#include "lmath.h"
#include "stdio.h"

#define ILLEGALMOVE 42 // Answer to the universe

typedef enum
{
    PAWNVAL = 1,
    BISHOPVAL = 3,
    KNIGHTVAL = 3,
    ROOKVAL = 5,
    QUEENVAL = 9,
    KINGVAL = 11,
} pieceVal;

int assessSquare(int ind, Position *board)
{

    if (ind > 63 || ind < 0)
    {
        return ILLEGALMOVE; // Random Value for invalid move
    }

    int val = 0;
    if ((board->color[0] >> ind) & 1)
    {
        if ((board->pieces[0] >> ind) & 1)
        {
            return PAWNVAL;
        }
        if ((board->pieces[1] >> ind) & 1)
        {
            return BISHOPVAL;
        }
        if ((board->pieces[2] >> ind) & 1)
        {
            return KNIGHTVAL;
        }
        if ((board->pieces[3] >> ind) & 1)
        {
            return ROOKVAL;
        }
        if ((board->pieces[4] >> ind) & 1)
        {
            return QUEENVAL;
        }
        if ((board->pieces[5] >> ind) & 1)
        {
            return KINGVAL;
        }
    }
    else
    {
        if ((board->pieces[0] >> ind) & 1)
        {
            return -PAWNVAL;
        }
        if ((board->pieces[1] >> ind) & 1)
        {
            return -BISHOPVAL;
        }
        if ((board->pieces[2] >> ind) & 1)
        {
            return -KNIGHTVAL;
        }
        if ((board->pieces[3] >> ind) & 1)
        {
            return -ROOKVAL;
        }
        if ((board->pieces[4] >> ind) & 1)
        {
            return -QUEENVAL;
        }
        if ((board->pieces[5] >> ind) & 1)
        {
            return -KINGVAL;
        }
    }
    return val;
}

short eval(Position board)
{
    short totalw = 0;
    short totalb = 0;

    for (int piece = 0; piece < 6; piece++)
    {
        short v = bitVal[piece];
        totalw += v * __builtin_popcount(board.color[0] & board.pieces[piece]);
        totalb += v * __builtin_popcount(board.color[1] & board.pieces[piece]);
    }
    return totalw - totalb;
}