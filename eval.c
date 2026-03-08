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
    Bitboard danger_bitboard = pawnMask(board, !board->turn) |
                      bishopMask(board, !board->turn) |
                      horseMask(board, !board->turn) |
                      rookMask(board, !board->turn);
    Bitboard attack_bitboard = pawnMask(board, board->turn) |
                      bishopMask(board, board->turn) |
                      horseMask(board, board->turn) |
                      rookMask(board, board->turn);

    int squarecontrol = __builtin_popcount(attack_bitboard)-__builtin_popcount(danger_bitboard);

    int totalcentrality = 0;
    uint64_t pieces = board->color[board->turn];
    while (pieces)
    {
        int ind = pop_lsb(&pieces);
        totalcentrality += penaltymap[ind] * 2;
    }

    int attacks = 0;
    int endangered = 0;

    int totalstm = 0;
    int totalopp = 0;
    for (int piece = 0; piece < 6; piece++)
    {
        int v = bitVal[piece];
        totalstm += v * __builtin_popcount(board->color[board->turn] & board->pieces[piece]);
        totalopp += v * __builtin_popcount(board->color[!board->turn] & board->pieces[piece]);
        attacks += (v/5)*__builtin_popcount((board->color[!board->turn] & board->pieces[piece]) & attack_bitboard);
        endangered += (v/2)*__builtin_popcount((board->color[board->turn] & board->pieces[piece]) & danger_bitboard);
    }
    return (totalstm - totalopp + totalcentrality + squarecontrol + attacks - endangered);
}