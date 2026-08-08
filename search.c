#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "tt.h"
#include "zobrist.h"

#define MATE_SCORE 32000
#define MAX_DEPTH 200
#define MAX_GAME_PLY 2048
#define MAX_LMR_MOVES 50

#define MAX_HISTORY 16384

static int butterfly_hist[2][64][64];
static uint16_t killer_moves[MAX_GAME_PLY][2];

void reset_history(void)
{
    memset(butterfly_hist, 0, sizeof butterfly_hist);
    memset(killer_moves, 0, sizeof killer_moves);
}

int lmr_table[MAX_DEPTH + 1][MAX_LMR_MOVES + 1];

void init_lmr()
{
    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        for (int move = 1; move <= MAX_LMR_MOVES; move++)
        {
            int r = (int)(log((double)depth) *
                           log((double)move) / 2.0);

            if (r < 1)
                r = 1;

            if (r > depth - 2)
                r = depth - 2;

            if (r > 8)
                r = 8;

            lmr_table[depth][move] = r;
        }
    }
}

static void move_to_uci(uint16_t move, char *buf)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int flag = (move >> 12) & 0xF;

    buf[0] = 'a' + (from & 7);
    buf[1] = '0' + (8 - (from >> 3));
    buf[2] = 'a' + (to & 7);
    buf[3] = '0' + (8 - (to >> 3));

    switch (flag)
    {
    case 5:
        buf[4] = 'b';
        buf[5] = '\0';
        return;
    case 6:
        buf[4] = 'n';
        buf[5] = '\0';
        return;
    case 7:
        buf[4] = 'r';
        buf[5] = '\0';
        return;
    case 8:
        buf[4] = 'q';
        buf[5] = '\0';
        return;
    }

    buf[4] = '\0';
}

static int score_from_tt(int score, int ply)
{
    if (score > 31000)
        return score - ply;
    if (score < -31000)
        return score + ply;
    return score;
}

static int score_to_tt(int score, int ply)
{
    if (score > 31000)
        return score + ply;
    if (score < -31000)
        return score - ply;
    return score;
}

static int piece_value_lva(int piece)
{
    switch (piece)
    {
    case 0:
        return 100; // pawn
    case 1:
        return 330; // bishop
    case 2:
        return 320; // horse/knight
    case 3:
        return 500; // rook
    case 4:
        return 900; // queen
    case 5:
        return 20000; // king
    }
    return 0;
}

static void make_null_move(Position *board)
{
    if (board->epsquare != -1)
    {
        board->hash ^= zobrist_table[785 + (board->epsquare & 7)];
        board->epsquare = -1;
    }

    board->turn ^= 1;
    board->hash ^= zobrist_table[768];
}

static int is_mate_score(int score)
{
    return score > 31000 || score < -31000;
}


static inline int lmr_reduction(int depth, int move_number)
{
    if (move_number > MAX_LMR_MOVES)
        move_number = MAX_LMR_MOVES;

    return lmr_table[depth][move_number];
}
// 145 elo moveordering
MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move)
{
    MoveList ordered = *move_list;
    int scores[256] = {0};

    const int TT_SCORE = 100000000;
    const int CAPTURE_BASE = 90000000; // above killers
    const int KILLER_BASE = 80000000;  // above history

    for (unsigned int i = 0; i < ordered.offset; i++)
    {
        uint16_t move = ordered.movelist[i];

        if (move == tt_move)
        {
            scores[i] = TT_SCORE;
            continue;
        }

        int from = move_from(move);
        int to = move_to(move);
        int victim = piece_on_square(board, to);
        int attacker = piece_on_square(board, from);

        if (victim != -1 && attacker != -1)
        {
            int mvv_lva = piece_value_lva(victim) * 10 - piece_value_lva(attacker);
            scores[i] = CAPTURE_BASE + mvv_lva;
            continue;
        }

        bool is_killer = false;
        if (ply < MAX_GAME_PLY)
        {
            if (move == killer_moves[ply][0] || move == killer_moves[ply][1])
            {
                // slight edge to the first killer
                int bonus = (move == killer_moves[ply][0]) ? 1 : 0;
                scores[i] = KILLER_BASE + bonus;
                is_killer = true;
            }
        }

        if (!is_killer)
        {
            scores[i] = butterfly_hist[board->turn][from][to];
        }
    }

    for (unsigned int i = 0; i < ordered.offset; i++)
    {
        unsigned int best = i;
        for (unsigned int j = i + 1; j < ordered.offset; j++)
        {
            if (scores[j] > scores[best])
                best = j;
        }
        if (best != i)
        {
            uint16_t tmp_move = ordered.movelist[i];
            ordered.movelist[i] = ordered.movelist[best];
            ordered.movelist[best] = tmp_move;

            int tmp_score = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp_score;
        }
    }

    return ordered;
}

// 361 elo qsearch
int quiesce(Position *board, int alpha, int beta, int ply, stopConditions *stop)
{
    stop->nodes++;

    if (ply > stop->seldepth)
        stop->seldepth = ply;
    if (stop->start_time && (stop->nodes & 2047) == 0 &&
        get_time_ms() - stop->start_time >= stop->max_time)
        stop->stop = 1;

    if (stop->max_nodes && stop->nodes >= stop->max_nodes)
        stop->stop = 1;


    int static_eval = eval(board);

    if (static_eval >= beta)
        return static_eval;

    if (static_eval > alpha)
        alpha = static_eval;

    MoveList move_list = {0};

    qsearchMoves(board, &move_list, board->turn);
    move_list = ordermoves(board, &move_list, ply, 0); // 18 elo qsearch ordering moves

    int best_score = static_eval;

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        uint64_t king_bb = copy.pieces[5] & copy.color[board->turn];
        if (!king_bb || squareAttacked(&copy, __builtin_ctzll(king_bb), !board->turn))
            continue;

        int score = -quiesce(&copy, -beta, -alpha, ply + 1, stop);

        if (score > best_score)
            best_score = score;

        if (score >= beta)
            return score;

        if (score > alpha)
            alpha = score;
    }

    return best_score;
}

searchOutput search(Position *board, int depth, int ply, int alpha, int beta,
                    stopConditions *stop, PVLine *pv)
{
    searchOutput output = {0};
    int alpha_orig = alpha;
    uint16_t tt_move = 0;

    stop->nodes++;

    if (pv)
        pv->length = 0;

    if (ply > stop->seldepth)
        stop->seldepth = ply;

    if (stop->start_time && (stop->nodes & 2047) == 0 &&
        get_time_ms() - stop->start_time >= stop->max_time)
        stop->stop = 1;

    if (stop->max_nodes && stop->nodes >= stop->max_nodes)
        stop->stop = 1;

    if (stop->stop)
        return output;
    if (depth <= 0)
        return (searchOutput){.score = quiesce(board, alpha, beta, ply, stop), .move = 0};
    TTEntry *entry = tt_probe(board->hash);
    if (entry)
    {
        tt_move = entry->move;

        if (entry->depth >= depth)
        {
            int tt_score = score_from_tt(entry->score, ply);

            if (entry->flag == TT_EXACT)
                return (searchOutput){.score = tt_score, .move = tt_move};
            if (entry->flag == TT_ALPHA && tt_score <= alpha)
                return (searchOutput){.score = tt_score, .move = tt_move};
            if (entry->flag == TT_BETA && tt_score >= beta)
                return (searchOutput){.score = tt_score, .move = tt_move};
        }
    }
    int in_check = king_in_check(board, board->turn);
    bool root_node = (ply == 0);

    if (in_check)
        depth++;

    if (depth >= 4 && tt_move == 0 && !in_check)
    {
        depth--;
    }

    int static_eval = 0;

    if (!in_check)
    {

        static_eval = eval(board);
        // RFP 60 elo
        if (!root_node &&
            depth <= 6 &&
            !is_mate_score(beta))
        {
            int margin = 100 * depth;

            if (static_eval - margin >= beta)
            {
                return (searchOutput){
                    .score = (static_eval + beta) / 2,
                    .move = 0};
            }
        }
        if (depth >= 3 && !root_node && static_eval >= beta)
        {
            int R = 3;

            Position copy = *board;
            make_null_move(&copy);

            int score = -search(&copy, depth - R - 1,
                                ply + 1, -beta, -beta + 1,
                                stop, NULL)
                             .score;

            if (stop->stop)
                return (searchOutput){0};

            if (score >= beta)
                return (searchOutput){.score = beta, .move = 0};
        }
    }

    MoveList move_list = {0};
    legalMoveGen(board, &move_list);
    move_list = ordermoves(board, &move_list, ply, tt_move);

    if (move_list.offset == 0)
    {
        output.score = in_check ? -MATE_SCORE + ply : 0;
        output.move = 0;
        return output;
    }

    int best_score = -MATE_SCORE;
    uint16_t best_move = 0;

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        uint16_t move = move_list.movelist[i];

        if (ply == 0 && stop->print_info)
        {
            char mv[6];
            move_to_uci(move, mv);
            printf("info depth %d currmove %s currmovenumber %d\n",
                   depth, mv, i + 1);
            fflush(stdout);
        }

        bool is_capture = is_capture_move(board, move);
        bool is_killer =
            move == killer_moves[ply][0] ||
            move == killer_moves[ply][1];
        bool is_promotion = is_promotion_move(move);

        if (!root_node &&
            !in_check &&
            depth <= 3 &&
            !is_capture &&
            !is_killer &&
            (int)i >= 20)
        {
            continue;
        }

        if (depth <= 1 && !in_check && !is_mate_score(alpha) && !is_mate_score(beta))
        {
            int futility_margin = 120;
            if (static_eval + futility_margin <= alpha)
            {

                if (!is_capture && !is_promotion)
                {
                    continue;
                }
            }
        }
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        PVLine child_pv = {0};
        int score;

        if (i == 0 || depth <= 2)
        {
            score = -search(&copy, depth - 1, ply + 1,
                            -beta, -alpha, stop, &child_pv)
                         .score;
        }
        else
        {
            int reduction = 0;
            if (!root_node && !in_check && depth >= 3 && i >= 4 &&
                !is_capture && !is_promotion && move != tt_move)
            {
                reduction = lmr_reduction(depth, i + 1);
            }

            if (reduction > 0)
            {
                // Reduced depth search with null window
                score = -search(&copy, depth - 1 - reduction, ply + 1,
                                -alpha - 1, -alpha, stop, NULL)
                             .score;

                if (!stop->stop && score > alpha)
                {
                    // Re-search with full depth but still null window
                    score = -search(&copy, depth - 1, ply + 1,
                                    -alpha - 1, -alpha, stop, NULL)
                                 .score;
                }
            }
            else
            {
                // Normal null-window search
                score = -search(&copy, depth - 1, ply + 1,
                                -alpha - 1, -alpha, stop, NULL)
                             .score;
            }

            // If score is inside the window, do a full-window re-search with PV
            if (!stop->stop && score > alpha && score < beta)
            {
                child_pv.length = 0;
                score = -search(&copy, depth - 1, ply + 1,
                                -beta, -alpha, stop, &child_pv)
                             .score;
            }
        }

        if (stop->stop)
            break;

        if (score > best_score)
        {
            best_score = score;
            best_move = move;
        }

        if (score > alpha)
        {
            alpha = score;

            if (pv && child_pv.length + 1 <= MAX_PV_LENGTH)
            {
                pv->moves[0] = move;
                memcpy(pv->moves + 1, child_pv.moves,
                       child_pv.length * sizeof(uint16_t));
                pv->length = child_pv.length + 1;
            }
        }
        else
        {
            int from = (move >> 6) & 0x3F;
            int to = move & 0x3F;

            if (!is_capture && !is_promotion)
            {
                int malus = -clamp_int(160 * depth - 200, 0, MAX_HISTORY);

                butterfly_hist[board->turn][from][to] +=
                    malus -
                    butterfly_hist[board->turn][from][to] *
                        abs(malus) / MAX_HISTORY;
            }
        }

        if (alpha >= beta)
        {
            int from = move_from(move);
            int to = move_to(move);

            if (!is_capture && !is_promotion)
            {
                // Butterfly history 68 Elo
                int clampedBonus = clamp_int(320 * depth - 400, 0, MAX_HISTORY);
                butterfly_hist[board->turn][from][to] += clampedBonus - butterfly_hist[board->turn][from][to] * abs(clampedBonus) / MAX_HISTORY;

                // Killer moves
                if (ply < MAX_GAME_PLY && killer_moves[ply][0] != move)
                {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }
            }

            break;
        }
    }

    if (!stop->stop)
    {
        int flag;
        if (best_score <= alpha_orig)
            flag = TT_ALPHA;
        else if (best_score >= beta)
            flag = TT_BETA;
        else
            flag = TT_EXACT;

        tt_store(board->hash, score_to_tt(best_score, ply), best_move, depth, flag);
    }

    output.score = best_score;
    output.move = best_move;
    return output;
}

uint16_t iterative_deepening(Position *board, stopConditions *stop)
{
    uint16_t best_move_so_far = 0;
    PVLine best_pv = {0};
    long long search_start = get_time_ms();

    int prev_score = 0;
    int aspiration_delta = 25;
    const int ASPIRATION_MAX_DELTA = 500;

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        /* -------- SOFT LIMIT CHECKS (only if non‑zero) -------- */
        if (stop->soft_time > 0) {
            int64_t elapsed = get_time_ms() - stop->start_time;
            if (elapsed >= stop->soft_time)
                break;                      // stop before next depth
        }
        if (stop->soft_nodes > 0 && stop->nodes >= stop->soft_nodes)
            break;                          // stop before next depth
        /* ------------------------------------------------------ */

        if (stop->depth > 0 && depth > stop->depth)
            break;

        stop->seldepth = 0;

        PVLine pv = {0};
        searchOutput out;

        int alpha, beta;
        bool first_attempt = (depth == 1);
        if (first_attempt) {
            alpha = -MATE_SCORE;
            beta  =  MATE_SCORE;
        } else {
            alpha = prev_score - aspiration_delta;
            beta  = prev_score + aspiration_delta;
        }

        int delta = aspiration_delta;
        int research_count = 0;
        const int MAX_RESEARCH = 5;

        while (1) {
            pv.length = 0;
            out = search(board, depth, 0, alpha, beta, stop, &pv);

            if (stop->stop)          // hard stop triggered inside search
                break;

            if (out.score > alpha && out.score < beta)
                break;

            if (out.score <= alpha) {
                alpha = out.score - delta;
                if (alpha < -MATE_SCORE) alpha = -MATE_SCORE;
            } else if (out.score >= beta) {
                beta = out.score + delta;
                if (beta > MATE_SCORE) beta = MATE_SCORE;
            }

            delta *= 2;
            if (delta > ASPIRATION_MAX_DELTA) {
                alpha = -MATE_SCORE;
                beta  =  MATE_SCORE;
            }

            research_count++;
            if (research_count >= MAX_RESEARCH) {
                alpha = -MATE_SCORE;
                beta  =  MATE_SCORE;
                pv.length = 0;
                out = search(board, depth, 0, alpha, beta, stop, &pv);
                break;
            }
        }

        if (stop->stop)
            break;

        prev_score = out.score;

        if (out.move != 0) {
            best_move_so_far = out.move;
            best_pv = pv;
        }

        int64_t elapsed = get_time_ms() - search_start;
        long long nps = elapsed > 0 ? (stop->nodes * 1000LL) / elapsed : 0;

        char score_str[32];
        if (out.score > 31000)
            snprintf(score_str, sizeof(score_str), "mate %d",
                     (MATE_SCORE - out.score + 1) / 2);
        else if (out.score < -31000)
            snprintf(score_str, sizeof(score_str), "mate -%d",
                     (MATE_SCORE + out.score + 1) / 2);
        else
            snprintf(score_str, sizeof(score_str), "cp %d", out.score);

        char pv_str[1024] = {0};
        int pos = 0;
        for (int i = 0; i < best_pv.length && i < depth &&
                        pos < (int)sizeof(pv_str) - 7;
             i++)
        {
            char mv[6];
            move_to_uci(best_pv.moves[i], mv);
            pos += snprintf(pv_str + pos, sizeof(pv_str) - pos, "%s ", mv);
        }

        if (pos > 0 && pv_str[pos - 1] == ' ')
            pv_str[pos - 1] = '\0';

        printf("info depth %d seldepth %d score %s nodes %llu nps %lld time %lld pv %s\n",
               depth, stop->seldepth, score_str,
               (unsigned long long)stop->nodes,
               nps, elapsed,
               pv_str);
        fflush(stdout);
    }

    return best_move_so_far;
}