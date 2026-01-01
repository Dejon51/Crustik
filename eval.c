#include "eval.h"
#include "lmath.h"
#include "stdio.h"

#define ILLEGALMOVE 42 // Answer to the universe

typedef enum {
    PAWNVAL = 1,  
    BISHOPVAL = 3, 
    KNIGHTVAL = 3, 
    ROOKVAL = 5, 
    QUEENVAL = 9,
    KINGVAL = 11, 
} pieceVal;


int assessSquare(int ind, BitboardSet *board)
{


    if (ind > 63 || ind < 0)
    {
        return ILLEGALMOVE; // Random Value for invalid move
    }
    
    int val = 0;
    if (is_set(board->color[0], ind))
    {
        if (is_set(board->pieces[0], ind))
        {
            return PAWNVAL;
        }
        if (is_set(board->pieces[1], ind))
        {
            return BISHOPVAL;
        }
        if (is_set(board->pieces[2], ind))
        {
            return KNIGHTVAL;
        }
        if (is_set(board->pieces[3], ind))
        {
            return ROOKVAL;
        }
        if (is_set(board->pieces[4], ind))
        {
            return QUEENVAL;
        }
        if (is_set(board->pieces[5], ind))
        {
            return KINGVAL;
        }
    }
    else
    {
        if (is_set(board->pieces[0], ind))
        {
            return -PAWNVAL;
        }
        if (is_set(board->pieces[1], ind))
        {
            return -BISHOPVAL;
        }
        if (is_set(board->pieces[2], ind))
        {
            return -KNIGHTVAL;
        }
        if (is_set(board->pieces[3], ind))
        {
            return -ROOKVAL;
        }
        if (is_set(board->pieces[4], ind))
        {
            return -QUEENVAL;
        }
        if (is_set(board->pieces[5], ind))
        {
            return -KINGVAL;
        }
    }
    return val;
}



short eval(BitboardSet board)
{
    short totalw = 0;
    short totalb = 0;

    for (int piece = 0; piece < 6; piece++)
    {
        short v = bitVal[piece];
        totalw += v * __builtin_popcount(board.color[0] & board.pieces[piece]);
        totalb += v * __builtin_popcount(board.color[1] & board.pieces[piece]);
    }
    return totalw-totalb;
}