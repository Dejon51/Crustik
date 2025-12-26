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


char assessSquare(char ind, BitboardSet board)
{


    if (ind > 63 || ind < 0)
    {
        return ILLEGALMOVE; // Random Value for invalid move
    }
    
    int val = 0;
    if (is_set(board.color[0], ind))
    {
        if (is_set(board.pieces[0], ind))
        {
            return PAWNVAL;
        }
        if (is_set(board.pieces[1], ind))
        {
            return BISHOPVAL;
        }
        if (is_set(board.pieces[2], ind))
        {
            return KNIGHTVAL;
        }
        if (is_set(board.pieces[3], ind))
        {
            return ROOKVAL;
        }
        if (is_set(board.pieces[4], ind))
        {
            return QUEENVAL;
        }
        if (is_set(board.pieces[5], ind))
        {
            return KINGVAL;
        }
    }
    else
    {
        if (is_set(board.pieces[0], ind))
        {
            return -PAWNVAL;
        }
        if (is_set(board.pieces[1], ind))
        {
            return -BISHOPVAL;
        }
        if (is_set(board.pieces[2], ind))
        {
            return -KNIGHTVAL;
        }
        if (is_set(board.pieces[3], ind))
        {
            return -ROOKVAL;
        }
        if (is_set(board.pieces[4], ind))
        {
            return -QUEENVAL;
        }
        if (is_set(board.pieces[5], ind))
        {
            return -KINGVAL;
        }
    }
    return val;
}

short threatAccess()
{
    for (char x = 0; x < 8; x++)
    {
        for (char y = 0; y < 8; y++)
        {
        }
    }
}

short eval(BitboardSet board)
{
    short bitVal[6] = {1,3,3,5,9,0};
    short total = 0;
    for (int piece = 0; piece < 6; piece++)
    {
        short v = bitVal[piece];
        total += v * __builtin_popcount(board.color[0] & board.pieces[piece]);
    }
    return total;
}