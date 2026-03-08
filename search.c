#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"

#define MAX_DEPTH 14

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop)
{
    searchOutput output = {0};
    stop->nodes += 1;
    if ((stop->nodes & 2047) == 0)
    {
        if (get_time_ms() - stop->start_time >= stop->max_time)
        {
            stop->stop = 1;
        }
    }
    if (stop->max_nodes != 0)
    {
        if (stop->nodes >= stop->max_nodes)
            stop->stop = 1;
    }

    if (stop->stop)
    {
        output.score = eval(board);
        return output;
    }

    if (depth <= 0)
    {
        output.score = eval(board);
        return output;
    }

    MoveList move_list;
    move_list.offset = 0;

    legalMoveGen(board, &move_list);

    if (move_list.offset == 0)
    {
        uint64_t king_bb = board->pieces[5] & board->color[board->turn];
        if (!king_bb)
        {
            output.score = -32000 + ply;
            return output;
        }
        int king_pos = __builtin_ctzll(king_bb);
        if (squareAttacked(board, king_pos, !board->turn))
            output.score = -32000 + ply;
        else
            output.score = 0;
        return output;
    }

    int best_score = -32000;
    Position copy;

    for (int i = 0; i < move_list.offset; i++)
    {
        copy = *board;
        makeMove(&copy, &move_list, i);

        int score = -search(&copy, depth - 1, ply + 1, -beta, -alpha, stop).score;

        if (score > best_score)
            best_score = score;

        if (score > alpha)
        {
            if (ply == 0)
                output.move = move_list.movelist[i];
            alpha = score;
        }

        if (score >= beta)
            break;
    }

    output.score = best_score;
    return output;
}

uint16_t iterative_deepening(Position *board, stopConditions *stop)
{
    uint16_t best_move_so_far = 0;

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        searchOutput out = search(board, depth, 0, -32000, 32000, stop);

        if (out.move != 0)
            best_move_so_far = out.move;

        if (stop->stop)
            break;
    }

    return best_move_so_far;
}