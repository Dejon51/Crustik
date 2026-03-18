#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"

#define MAX_DEPTH 14

MoveList ordermoves(Position *board, MoveList move_list) {
    MoveList scored_list = move_list;
    if (move_list.offset == 0)
        return scored_list;

    int scores[218] = {0};
    Position copy;

    for (int i = 0; i < move_list.offset; i++) {
        copy = *board;
        moveint(&copy, move_list.movelist[i]);
        scores[i] = eval(&copy, 8192); 
    }

    for (int i = 0; i < move_list.offset - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < move_list.offset; j++) {
            if (scores[j] > scores[max_idx]) {
                max_idx = j;
            }
        }
        int temp_score = scores[i];
        scores[i] = scores[max_idx];
        scores[max_idx] = temp_score;

        uint16_t temp_move = scored_list.movelist[i];
        scored_list.movelist[i] = scored_list.movelist[max_idx];
        scored_list.movelist[max_idx] = temp_move;
    }

    return scored_list;
}

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop)
{
    searchOutput output = {0};
    stop->nodes += 1;
    if (stop->start_time != 0)
    {
        if ((stop->nodes & 2047) == 0)
        {
            if (get_time_ms() - stop->start_time >= stop->max_time)
            {
                stop->stop = 1;
            }
        }
    }

    if (stop->max_nodes != 0)
    {
        if (stop->nodes >= stop->max_nodes)
            stop->stop = 1;
    }

    if (stop->stop)
    {
        output.score = eval(board, stop->nodes);
        return output;
    }

    if (depth <= 0)
    {
        output.score = output.score = quiesce(board, alpha, beta, stop->nodes);
        return output;
    }

    MoveList move_list;
    move_list.offset = 0;

    legalMoveGen(board, &move_list);

    // move_list = ordermoves(board, move_list);
    
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

        int16_t score = -search(&copy, depth - 1, ply + 1, -beta, -alpha, stop).score;

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
