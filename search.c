#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "zobrist.h"

#define MAX_DEPTH 70

TTEntry *transposition_table = NULL;
size_t tt_size = 0;

void tt_init(int mb)
{
    size_t bytes = (size_t)mb * 1024 * 1024;
    tt_size = 1;
    while (tt_size * 2 * sizeof(TTEntry) <= bytes)
        tt_size *= 2;

    free(transposition_table);
    transposition_table = calloc(tt_size, sizeof(TTEntry));
}

void tt_clear(void)
{
    if (transposition_table)
        memset(transposition_table, 0, tt_size * sizeof(TTEntry));
}

TTEntry *tt_probe(uint64_t key)
{
    TTEntry *entry = &transposition_table[key & (tt_size - 1)];
    if (entry->key == key)
        return entry;
    return NULL;
}

void tt_store(uint64_t key, int depth, int score, uint8_t flag, uint16_t best_move)
{
    TTEntry *entry = &transposition_table[key & (tt_size - 1)];
    entry->key = key;
    entry->depth = depth;
    entry->score = score;
    entry->flag = flag;
    entry->best_move = best_move;
}

static int mvv_lva[] = {
    105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,
    103, 203, 303, 403, 503, 603,
    102, 202, 302, 402, 502, 602,
    101, 201, 301, 401, 501, 601,
    100, 200, 300, 400, 500, 600};

static void move_to_uci(uint16_t move, char *buf)
{
    int from = (move >> 6) & 0x3F;
    int to   =  move       & 0x3F;
    int flag = (move >> 12) & 0xF;

    buf[0] = 'a' + (from & 7);
    buf[1] = '0' + (8 - (from >> 3));
    buf[2] = 'a' + (to & 7);
    buf[3] = '0' + (8 - (to >> 3));

    switch (flag) {
        case 5: buf[4] = 'b'; buf[5] = '\0'; return;
        case 6: buf[4] = 'n'; buf[5] = '\0'; return;
        case 7: buf[4] = 'r'; buf[5] = '\0'; return;
        case 8: buf[4] = 'q'; buf[5] = '\0'; return;
    }

    buf[4] = '\0';
}

MoveList ordermoves(Position *board, MoveList *move_list)
{
    MoveList scored_list = *move_list;
    if (move_list->offset == 0)
        return scored_list;

    int scores[218] = {0};

    for (int i = 0; i < move_list->offset; i++)
    {
        int to = move_list->movelist[i] & 0x3F;
        int from = (move_list->movelist[i] >> 6) & 0x3F;
        int victim = board->mailbox[to];
        int attacker = board->mailbox[from];

        if (victim == 6)
            scores[i] = 0;
        else
            scores[i] = 10000 + mvv_lva[victim * 6 + attacker];
    }

    for (int i = 1; i < move_list->offset; i++)
    {
        int score = scores[i];
        uint16_t move = scored_list.movelist[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < score)
        {
            scores[j + 1] = scores[j];
            scored_list.movelist[j + 1] = scored_list.movelist[j];
            j--;
        }
        scores[j + 1] = score;
        scored_list.movelist[j + 1] = move;
    }

    return scored_list;
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
    move_list = ordermoves(board, &move_list);

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

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop)
{
    searchOutput output = {0};
    stop->nodes++;

    if (stop->start_time != 0 && (stop->nodes & 2047) == 0)
        if (get_time_ms() - stop->start_time >= stop->max_time)
            stop->stop = 1;
    if (stop->max_nodes != 0 && stop->nodes >= stop->max_nodes)
        stop->stop = 1;
    if (stop->stop)
    {
        output.score = eval(board, stop->nodes);
        return output;
    }

    int original_alpha = alpha;

    uint64_t key = zobrist(board);
    TTEntry *tt = tt_probe(key);
    uint16_t tt_move = 0;
    if (tt && tt->depth >= depth)
    {
        if (tt->flag == 0)
        {
            if (tt->score >= beta)
                return (searchOutput){.score = beta, .move = tt->best_move};
            if (tt->score <= alpha)
                return (searchOutput){.score = alpha, .move = tt->best_move};
            return (searchOutput){.score = tt->score, .move = tt->best_move};
        }
        if (tt->flag == 1 && tt->score >= beta)
            return (searchOutput){.score = beta};
        if (tt->flag == 2 && tt->score <= alpha)
            return (searchOutput){.score = alpha};
        tt_move = tt->best_move;
    }

    if (depth <= 0)
    {
        output.score = quiesce(board, alpha, beta, stop->nodes);
        return output;
    }

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);
    move_list = ordermoves(board, &move_list);

    if (move_list.offset == 0)
    {
        uint64_t king_bb = board->pieces[5] & board->color[board->turn];
        if (!king_bb || squareAttacked(board, __builtin_ctzll(king_bb), !board->turn))
            output.score = -32000 + ply;
        else
            output.score = 0;
        return output;
    }

    int best_score = -32000;
    uint16_t best_move = 0;

    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        if (ply == 0 && stop->print_info)
        {
            char mv[6];
            move_to_uci(move_list.movelist[i], mv);
            printf("info depth %d currmove %s currmovenumber %d\n",
                   depth, mv, i + 1);
            fflush(stdout);
        }

        int score = -search(&copy, depth - 1, ply + 1, -beta, -alpha, stop).score;

        if (score > best_score)
        {
            best_score = score;
            best_move = move_list.movelist[i];
        }
        if (score > alpha)
            alpha = score;
        if (score >= beta)
            break;
    }

    uint8_t flag;
    if (best_score <= original_alpha)
        flag = 2;
    else if (best_score >= beta)
        flag = 1;
    else
        flag = 0;

    tt_store(key, depth, best_score, flag, best_move);

    output.score = best_score;
    output.move = best_move;
    return output;
}

uint16_t iterative_deepening(Position *board, stopConditions *stop)
{
    uint16_t best_move_so_far = 0;
    long long search_start = get_time_ms();

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        searchOutput out = search(board, depth, 0, -32000, 32000, stop);

        if (stop->stop)
            break;
        if (out.move != 0)
            best_move_so_far = out.move;

        long long elapsed = get_time_ms() - search_start;
        long long nps = elapsed > 0 ? (stop->nodes * 1000LL) / elapsed : 0;

        int hashfull = 0;
        size_t sample = tt_size < 1000 ? tt_size : 1000;
        for (size_t i = 0; i < sample; i++)
            if (transposition_table[i].key != 0)
                hashfull++;

        char score_str[32];
        if (out.score > 31000)
            snprintf(score_str, sizeof(score_str), "mate %d", (32000 - out.score + 1) / 2);
        else if (out.score < -31000)
            snprintf(score_str, sizeof(score_str), "mate -%d", (32000 + out.score + 1) / 2);
        else
            snprintf(score_str, sizeof(score_str), "cp %d", out.score);

        char best_uci[6];
        move_to_uci(best_move_so_far, best_uci);

        printf("info depth %d score %s nodes %llu nps %lld time %lld hashfull %d pv %s\n",
               depth, score_str,
               (unsigned long long)stop->nodes,
               nps, elapsed, hashfull,
               best_uci);
        fflush(stdout);
    }

    return best_move_so_far;
}