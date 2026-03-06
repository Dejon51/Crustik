#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"

searchOutput search(Position *board, int depth, int ply, int alpha, int beta)
{
    searchOutput output = {0};

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

        int score = -search(&copy, depth - 1, ply + 1, -beta, -alpha).score;

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