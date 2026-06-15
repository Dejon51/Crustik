#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "tt.h"

#define MATE_SCORE 32000
#define MAX_DEPTH 200
#define MAX_GAME_PLY 2048

void clear_ordering_tables(void)
{
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

MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move)
{
    (void)board;
    (void)ply;

    MoveList ordered = *move_list;

    if (!tt_move)
        return ordered;

    for (unsigned int i = 0; i < ordered.offset; i++)
    {
        if (ordered.movelist[i] == tt_move)
        {
            uint16_t tmp = ordered.movelist[0];
            ordered.movelist[0] = ordered.movelist[i];
            ordered.movelist[i] = tmp;
            break;
        }
    }

    return ordered;
}

// Compatibility stub. Basic negamax evaluates leaves directly.
int quiesce(Position *board, int alpha, int beta, int ply, stopConditions *stop)
{
    (void)alpha;
    (void)beta;
    (void)ply;
    (void)stop;
    return eval(board);
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

    uint64_t king_bb = board->pieces[5] & board->color[board->turn];
    int in_check = (!king_bb ||
                    squareAttacked(board, __builtin_ctzll(king_bb), !board->turn));

    if (depth <= 0)
        return (searchOutput){.score = eval(board), .move = 0};

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

        Position copy = *board;
        makeMove(&copy, &move_list, i);

        PVLine child_pv = {0};
        int score = -search(&copy, depth - 1, ply + 1,
                            -beta, -alpha, stop, &child_pv)
                         .score;

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

        if (alpha >= beta)
            break;
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

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        if (stop->depth > 0 && depth > stop->depth)
            break;

        stop->seldepth = 0;

        PVLine pv = {0};
        searchOutput out = search(board, depth, 0, -MATE_SCORE, MATE_SCORE, stop, &pv);

        if (stop->stop)
            break;

        if (out.move != 0)
        {
            best_move_so_far = out.move;
            best_pv = pv;
        }

        long long elapsed = get_time_ms() - search_start;
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