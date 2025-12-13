#include "eval.h"
#include "lmath.h"
#include "stdio.h"

char assessSquare(char ind, BitboardSet board)
{
    char peiceVal[8] = {
        1,  // PAWNVAL
        3,  // BISHOPVAL
        3,  // KNIGHTVAL
        5,  // ROOKVAL
        9,  // QUEENVAL
        10, // FRIENDLYPIECEVAL
        11, // OUTBOUNDVAL
        12  // KINGVAL
    };

    int val = 0;
    if (is_set(board.color[0], ind))
    {
        if (is_set(board.pieces[0], ind))
        {
            return peiceVal[0];
        }
        if (is_set(board.pieces[1], ind))
        {
            return peiceVal[1];
        }
        if (is_set(board.pieces[2], ind))
        {
            return peiceVal[2];
        }
        if (is_set(board.pieces[3], ind))
        {
            return peiceVal[3];
        }
        if (is_set(board.pieces[4], ind))
        {
            return peiceVal[4];
        }
        if (is_set(board.pieces[5], ind))
        {
            return peiceVal[5];
        }
    }
    else
    {
        if (is_set(board.pieces[0], ind))
        {
            return -peiceVal[0];
        }
        if (is_set(board.pieces[1], ind))
        {
            return -peiceVal[1];
        }
        if (is_set(board.pieces[2], ind))
        {
            return -peiceVal[2];
        }
        if (is_set(board.pieces[3], ind))
        {
            return -peiceVal[3];
        }
        if (is_set(board.pieces[4], ind))
        {
            return -peiceVal[4];
        }
        if (is_set(board.pieces[5], ind))
        {
            return -peiceVal[5];
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
        total -= v * __builtin_popcount(board.color[1] & board.pieces[piece]);
    }
    return total;
}