#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "zobrist.h"

#define MATE_SCORE 32000

#define MAX_DEPTH 200

#define MAX_GAME_PLY 2048

// Holds the hashes of positions leading up to the current node
uint64_t position_history[MAX_GAME_PLY];
int history_ply = 0;

static uint16_t killers[MAX_DEPTH][2];

static int history[6][64];

static void clear_ordering_tables(void)
{
    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));
}

static void store_killer(uint16_t move, int ply)
{
    if (killers[ply][0] != move)
    {
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = move;
    }
}

static void scale_history(void)
{
    for (int p = 0; p < 6; p++)
        for (int sq = 0; sq < 64; sq++)
            history[p][sq] /= 2;
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

MoveList ordermoves(Position *board, MoveList *move_list, int ply)
{
    MoveList scored_list = *move_list;
    if (move_list->offset == 0)
        return scored_list;

    int scores[218] = {0};

    for (int i = 0; i < move_list->offset; i++)
    {
        uint16_t move = move_list->movelist[i];
        int to = move & 0x3F;
        int from = (move >> 6) & 0x3F;
        int flag = (move >> 12) & 0xF;
        int victim = board->mailbox[to];
        int attacker = board->mailbox[from];

        if (flag == 8)
        {
            scores[i] = 9500;
            continue;
        }
        if (flag == 7)
        {
            scores[i] = 9300;
            continue;
        }
        if (flag == 5)
        {
            scores[i] = 9200;
            continue;
        }
        if (flag == 6)
        {
            scores[i] = 9100;
            continue;
        }
        if (victim != 0 && victim != 6)
        {
            int att_idx = (attacker > 0 ? attacker - 1 : 0);
            int vic_idx = victim - 1;
            scores[i] = 10000 + mvv_lva[vic_idx * 6 + att_idx];
            continue;
        }

        if (move == killers[ply][0])
        {
            scores[i] = 9000;
            continue;
        }
        if (move == killers[ply][1])
        {
            scores[i] = 8000;
            continue;
        }

        {
            Position copy = *board;
            moveint(&copy, move);
            uint64_t their_king = copy.pieces[5] & copy.color[!board->turn];
            if (their_king && squareAttacked(&copy, __builtin_ctzll(their_king), board->turn))
            {
                scores[i] = 7000;
                continue;
            }
        }

        int piece_idx = (attacker >= 1 && attacker <= 6) ? attacker - 1 : 0;
        scores[i] = history[piece_idx][to];
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

int quiesce(Position *board, int alpha, int beta, stopConditions *stop)
{
    stop->nodes++;

    int static_eval = eval(board, stop->nodes);

    if (static_eval >= beta)
        return static_eval;
    if (static_eval > alpha)
        alpha = static_eval;

    MoveList move_list = {};
    captureMoves(board, &move_list, board->turn);
    move_list = ordermoves(board, &move_list, 0);

    int best_score = static_eval;

    for (int i = 0; i < move_list.offset; i++)
    {
        Position copy = *board;
        moveint(&copy, move_list.movelist[i]);

        uint64_t our_king = copy.pieces[5] & copy.color[board->turn];
        if (!our_king || squareAttacked(&copy, __builtin_ctzll(our_king), !board->turn))
            continue;

        int score = -quiesce(&copy, -beta, -alpha, stop);

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
                    stopConditions *stop)
{
    searchOutput output = {0};
    stop->nodes++;

    // Stop conditions
    if (stop->start_time && (stop->nodes & 2047) == 0 &&
        get_time_ms() - stop->start_time >= stop->max_time)
        stop->stop = 1;
    if (stop->max_nodes && stop->nodes >= stop->max_nodes)
        stop->stop = 1;
    if (stop->stop)
        return (searchOutput){.score = 0, .move = 0};

    uint64_t hash = zobrist(board);

    // Draw detection (50-move rule and repetition)
    if (ply > 0)
    {
        if (board->halfmoves >= 100)
            return (searchOutput){.score = 0, .move = 0};

        int limit = history_ply - board->halfmoves;
        if (limit < 0)
            limit = 0;

        for (int i = history_ply - 2; i >= limit; i -= 2)
            if (position_history[i] == hash)
                return (searchOutput){.score = 0, .move = 0};
    }

    // Null move pruning
    const int NULL_MOVE_REDUCTION = 2;
    const int SAFE_MATE_PLY = 20;

    if (depth >= 3 && ply > 0)
    {
        uint64_t king_bb = board->pieces[5] & board->color[board->turn];
        int in_check = (!king_bb ||
                        squareAttacked(board, __builtin_ctzll(king_bb), !board->turn));

        if (!in_check &&
            ((board->pieces[1] | board->pieces[2] |
              board->pieces[3] | board->pieces[4]) &
             board->color[board->turn]))
        {
            if (beta < MATE_SCORE - SAFE_MATE_PLY)
            {
                Position copy = *board;
                copy.turn ^= 1;

                position_history[history_ply++] = hash;
                int score = -search(&copy, depth - 1 - NULL_MOVE_REDUCTION,
                                    ply + 1, -beta, -beta + 1, stop)
                                 .score;
                history_ply--;

                if (stop->stop)
                    return (searchOutput){0};
                if (score >= beta)
                    return (searchOutput){.score = beta, .move = 0};
            }
        }
    }

    if (depth <= 0)
    {
        output.score = quiesce(board, alpha, beta, stop);
        return output;
    }

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);
    move_list = ordermoves(board, &move_list, ply);

    if (move_list.offset == 0)
    {
        uint64_t king_bb = board->pieces[5] & board->color[board->turn];
        if (!king_bb || squareAttacked(board, __builtin_ctzll(king_bb), !board->turn))
            output.score = -MATE_SCORE + ply;
        else
            output.score = 0;
        return output;
    }

    int best_score = -MATE_SCORE;
    uint16_t best_move = 0;

    for (int i = 0; i < move_list.offset; i++)
    {
        uint16_t move = move_list.movelist[i];
        int to = move & 0x3F;
        int from = (move >> 6) & 0x3F;
        int victim = board->mailbox[to];
        int piece = board->mailbox[from];
        int is_cap = (victim != 0 && victim != 6);

        Position copy = *board;
        makeMove(&copy, &move_list, i);

        if (ply == 0 && stop->print_info)
        {
            char mv[6];
            move_to_uci(move, mv);
            printf("info depth %d currmove %s currmovenumber %d\n",
                   depth, mv, i + 1);
            fflush(stdout);
        }

        position_history[history_ply++] = hash;
        int score = -search(&copy, depth - 1, ply + 1,
                            -beta, -alpha, stop)
                         .score;
        history_ply--;

        if (stop->stop)
            break;

        if (score > best_score)
        {
            best_score = score;
            best_move = move;
        }

        if (score > alpha)
            alpha = score;

        if (alpha >= beta)
        {
            if (!is_cap)
            {
                store_killer(move, ply);

                int piece_idx = (piece >= 1 && piece <= 6) ? piece - 1 : 0;
                history[piece_idx][to] += depth * depth;
                if (history[piece_idx][to] > 1000000)
                    scale_history();
            }
            break;
        }
    }

    output.score = best_score;
    output.move = best_move;
    return output;
}

uint16_t iterative_deepening(Position *board, stopConditions *stop)
{
    clear_ordering_tables();

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

        char score_str[32];
        if (out.score > 31000)
            snprintf(score_str, sizeof(score_str), "mate %d",
                     (32000 - out.score + 1) / 2);
        else if (out.score < -31000)
            snprintf(score_str, sizeof(score_str), "mate -%d",
                     (32000 + out.score + 1) / 2);
        else
            snprintf(score_str, sizeof(score_str), "cp %d", out.score);

        char best_uci[6];
        move_to_uci(best_move_so_far, best_uci);

        printf("info depth %d score %s nodes %llu nps %lld time %lld pv %s\n",
               depth, score_str,
               (unsigned long long)stop->nodes,
               nps, elapsed,
               best_uci);
        fflush(stdout);
    }

    return best_move_so_far;
}