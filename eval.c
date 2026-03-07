#include "eval.h"
#include "lmath.h"
#include "stdio.h"
#include "play.h"

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

int eval(Position *board)
{
    Bitboard danger = pawnMask(board, !board->turn) |
                    bishopMask(board, !board->turn) |
                    horseMask(board, !board->turn) |
                    rookMask(board, !board->turn);
    
    int totalw = 0;
    int totalb = 0;
    int squarecontrol = 0;

    squarecontrol +=  -__builtin_popcount(danger);


    int direction = (board->turn == 0) ? 1 : -1;

    uint64_t pieces = board->color[board->turn];

    int totalcentrality = 0;
    while (pieces)
    {
        int ind = pop_lsb(&pieces);
        totalcentrality += penaltymap[ind]*5;
    }

    for (int piece = 0; piece < 6; piece++)
    {
        int v = bitVal[piece];
        totalw += v * __builtin_popcount(board->color[0] & board->pieces[piece]);
        totalb += v * __builtin_popcount(board->color[1] & board->pieces[piece]);
    }
    return (totalw - totalb + totalcentrality + squarecontrol)*direction;
}