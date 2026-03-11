#include "eval.h"
#include "lmath.h"
#include "stdio.h"
#include "play.h"
#include <stdio.h>

// #define ILLEGALMOVE 42 // Answer to the universe

static uint8_t penaltymap[64] = {1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 5, 5, 4, 3, 2, 3, 4, 5, 6, 6, 5, 4, 3, 4, 5, 6, 7, 7, 6, 5, 4, 4, 5, 6, 7, 7, 6, 5, 4, 3, 4, 5, 6, 6, 5, 4, 3, 2, 3, 4, 5, 5, 4, 3, 2, 1, 2, 3, 4, 4, 3, 2, 1};

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

int eval(Position *board, int nodes)
{
    int totalstm = 0;
    int totalopp = 0;
    for (int piece = 0; piece < 6; piece++)
    {
        int v = bitVal[piece];
        totalstm += v * __builtin_popcount(board->color[board->turn] & board->pieces[piece]);
        totalopp += v * __builtin_popcount(board->color[!board->turn] & board->pieces[piece]);
    }

    int base_eval = totalstm - totalopp;

    int noise = nodes % 128 - 64;
    return base_eval + noise;
}

int quiesce(Position *board, int alpha, int beta, int nodes)
{
    int static_eval = eval(board, nodes);

    if (static_eval >= beta)
        return static_eval;

    if (static_eval > alpha)
        alpha = static_eval;

    MoveList move_list = {};
    captureMoves(board, &move_list, board->turn);

    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        moveint(&copy, move_list.movelist[i]);

        uint64_t our_king = copy.pieces[5] & copy.color[board->turn];
        if (!our_king || squareAttacked(&copy, __builtin_ctzll(our_king), !board->turn))
            continue;

        int score = -quiesce(&copy, -beta, -alpha, nodes);

        if (score >= beta)
            return score;

        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

// int Quiesce( int alpha, int beta ) {
//     int static_eval = eval();

//     int best_value = static_eval;
//     if( best_value >= beta )
//         return best_value;
//     if( best_value > alpha )
//         alpha = best_value;

//     until( every_capture_has_been_examined )  {
//         MakeCapture();
//         score = -Quiesce( -beta, -alpha );
//         TakeBackMove();

//         if( score >= beta )
//             return score;
//         if( score > best_value )
//             best_value = score;
//         if( score > alpha )
//             alpha = score;
//     }

//     return best_value;
// }