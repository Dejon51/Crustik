#include <stdio.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"


searchOutput search(Position *board, int depth,int ply,int alpha,int beta)
{
    searchOutput output = {};
    if (depth <= 0)
    {
        output.score = eval(board);
        return output;
    }
    int best_score = -2000;
    

    // Play each move
    MoveList move_list;
    move_list.offset = 0;
    Position copy;
    legalMoveGen(&copy, &move_list, &copy.turn);

    for (int i = 0; i < move_list.offset; i++)
    {
        copy = *board;
        makeMove(&copy, &move_list,i);
        int score = -search(&copy,depth -1,ply+1,-beta,-alpha).score;
        if (score > best_score){
            best_score = score;
        }

        if (score > alpha) {
            if (ply == 0) {
                output.move = i;
            }
            alpha = score;
        }

        if (score >= beta) {
            break;
        }
    }
    output.score = best_score;
    return output;
}

/*
fn search(&mut self, pos: &Position, depth: i32, ply: i32, mut alpha: i32, beta: i32) -> i32 {
    if depth <= 0 {
        return eval(pos); // or qsearch
    }

    let mut best_score = -SCORE_INF;

    let moves = generate_moves(pos);
    for mv in moves {
        let new_pos = pos.make_move(mv);
        let score = -self.search(&new_pos, depth - 1, ply + 1, -beta, -alpha);

        if score > best_score {
            best_score = score;
        }

        if score > alpha {
            if ply == 0 {
                self.best_move = mv;
            }
            alpha = score;
        }

        if score >= beta {
            break;
        }
    }

    best_score
}
*/