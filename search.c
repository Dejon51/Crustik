#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "zobrist.h"
#include "tt.h"
#include "precomputed.h"
#include "rook_table.h"
#include "bishop_table.h"

#define MATE_SCORE 32000
#define MAX_DEPTH 200
#define MAX_GAME_PLY 2048

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

static const int SEE_VAL[6] = {100, 320, 330, 500, 900, 20000};

int SEE(Position *pos, uint16_t move)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;

    int gain[64];
    int depth = 0;

    uint64_t occ = pos->color[0] | pos->color[1];

    int target = pos->mailbox[to];
    int attacker_piece = pos->mailbox[from];

    if (target == 6)
        return 0;

    gain[0] = SEE_VAL[target];
    occ ^= (1ULL << from);

    int stm = pos->turn ^ 1; // opponent recaptures first
    int next_victim = SEE_VAL[attacker_piece];

    while (1)
    {
        uint64_t attackers = 0ULL;

        attackers |= (stm == 0 ? white_pawn_attacks[to] : black_pawn_attacks[to]) &
                     (pos->pieces[0] & pos->color[stm]);
        attackers |= knighttable[to] & (pos->pieces[2] & pos->color[stm]);
        attackers |= kingtable[to] & (pos->pieces[5] & pos->color[stm]);
        attackers |= getbishopAttacks(to, occ) &
                     ((pos->pieces[1] | pos->pieces[4]) & pos->color[stm]);
        attackers |= getrookAttacks(to, occ) &
                     ((pos->pieces[3] | pos->pieces[4]) & pos->color[stm]);

        if (!attackers)
            break;

        depth++;
        if (depth >= 32)
            break;

        gain[depth] = next_victim - gain[depth - 1];

        if (-gain[depth] > gain[depth - 1]) // early cutoff
            break;

        // pick least valuable attacker
        int best_sq = -1;
        int piece_type = -1;
        for (int pt = 0; pt < 6; pt++)
        {
            uint64_t bb = attackers & pos->pieces[pt] & pos->color[stm];
            if (bb)
            {
                best_sq = __builtin_ctzll(bb);
                piece_type = pt;
                break;
            }
        }

        occ ^= (1ULL << best_sq);
        next_victim = SEE_VAL[piece_type];
        stm ^= 1;
    }

    // backpropagation
    while (--depth >= 0)
        gain[depth] = -(gain[depth + 1] > -gain[depth] ? gain[depth + 1] : -gain[depth]);

    return gain[0];
}

MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move)
{
    MoveList scored_list = *move_list;
    if (move_list->offset == 0)
        return scored_list;

    int scores[218] = {0};

    for (unsigned int i = 0; i < move_list->offset; i++)
    {
        uint16_t move = move_list->movelist[i];
        int to = move & 0x3F;
        int from = (move >> 6) & 0x3F;
        int flag = (move >> 12) & 0xF;
        int victim = board->mailbox[to];
        int attacker = board->mailbox[from];

        if (move == tt_move)
        {
            scores[i] = 20000;
            continue;
        }

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
            int see = SEE(board, move);
            if (see >= 0)
                scores[i] = 10000 + see;
            else
                scores[i] = -1000 + see;
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

        int piece_idx = (attacker >= 1 && attacker <= 6) ? attacker - 1 : 0;
        scores[i] = history[piece_idx][to];
    }

    for (unsigned int i = 1; i < move_list->offset; i++)
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

int quiesce(Position *board, int alpha, int beta, int ply, stopConditions *stop)
{
    stop->nodes++;

    if (ply > stop->seldepth)
        stop->seldepth = ply;

    int static_eval = eval(board);

    if (static_eval >= beta)
        return static_eval;
    if (static_eval > alpha)
        alpha = static_eval;

    MoveList move_list = {0};
    captureMoves(board, &move_list, board->turn);
    move_list = ordermoves(board, &move_list, 0, 0);

    int best_score = static_eval;

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        uint16_t move = move_list.movelist[i];

        int to = move & 0x3F;

        int victim = board->mailbox[to];
        int is_cap = (victim != 6);

        Position copy = *board;
        if (is_cap)
        {
            int margin = SEE_VAL[victim] + 200; // piece value + safety margin
            if (static_eval + margin < alpha)
                continue;
            int see = SEE(board, move);
            if (see < 0)
                continue;
        }

        makeMove(&copy, &move_list, i);

        uint64_t king_bb = copy.pieces[5] & copy.color[board->turn];
        if (!king_bb || squareAttacked(&copy, __builtin_ctzll(king_bb), !board->turn))
            continue;

        int score = -quiesce(&copy, -beta, -alpha, ply + 1, stop);

        if (score > best_score)
            best_score = score;
        if (score >= beta)
            return best_score;
        if (score > alpha)
            alpha = score;
    }

    return best_score;
}

searchOutput search(Position *board, int depth, int ply, int alpha, int beta,
                    stopConditions *stop, PVLine *pv)
{
    searchOutput output = {0};
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
        return (searchOutput){.score = 0, .move = 0};

    uint64_t hash = board->hash;
    int alpha_orig = alpha;

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

    uint16_t tt_move = 0;
    TTEntry *entry = tt_probe(hash);
    if (entry)
    {
        tt_move = entry->move;

        if (entry->depth >= depth)
        {
            int tt_score = entry->score;

            if (tt_score > 31000)
                tt_score -= ply;
            else if (tt_score < -31000)
                tt_score += ply;

            if (entry->flag == TT_EXACT)
                return (searchOutput){.score = tt_score, .move = tt_move};
            if (entry->flag == TT_ALPHA && tt_score <= alpha)
                return (searchOutput){.score = tt_score, .move = tt_move};
            if (entry->flag == TT_BETA && tt_score >= beta)
                return (searchOutput){.score = tt_score, .move = tt_move};
        }
    }

    if (depth <= 0)
    {
        output.score = quiesce(board, alpha, beta, ply, stop);
        return output;
    }

    uint64_t king_bb = board->pieces[5] & board->color[board->turn];
    int in_check = (!king_bb ||
                    squareAttacked(board, __builtin_ctzll(king_bb), !board->turn));

    const int NULL_MOVE_REDUCTION = 3;

    if (depth >= 3 && ply > 0 && !in_check && abs(beta) < 31000)
    {
        if ((board->pieces[1] | board->pieces[2] |
             board->pieces[3] | board->pieces[4]) &
            board->color[board->turn])
        {
            Position copy = *board;
            copy.turn ^= 1;
            copy.hash ^= zobrist_table[768];

            if (copy.epsquare != -1)
            {
                copy.hash ^= zobrist_table[785 + (copy.epsquare & 7)];
                copy.epsquare = -1;
            }

            if (history_ply < MAX_GAME_PLY)
                position_history[history_ply++] = copy.hash;
            int score = -search(&copy, depth - 1 - NULL_MOVE_REDUCTION,
                                ply + 1, -beta, -beta + 1, stop, NULL)
                             .score;
            history_ply--;

            if (stop->stop)
                return (searchOutput){0};
            if (score >= beta)
                return (searchOutput){.score = beta, .move = 0};
        }
    }

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);

    move_list = ordermoves(board, &move_list, ply, tt_move);

    if (move_list.offset == 0)
    {
        if (in_check)
            output.score = -MATE_SCORE + ply;
        else
            output.score = 0;
        return output;
    }

    int best_score = -MATE_SCORE;
    uint16_t best_move = 0;
    int moves_searched = 0;

    int static_eval = eval(board);

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        uint16_t move = move_list.movelist[i];
        int to = move & 0x3F;
        int from = (move >> 6) & 0x3F;
        int victim = board->mailbox[to];
        int piece = board->mailbox[from];
        int flag = (move >> 12) & 0xF;
        int is_cap = (victim != 0 && victim != 6);
        int is_promo = (flag >= 5 && flag <= 8);
        int is_quiet = !is_cap && !is_promo;

        if (depth <= 3 && !in_check && is_quiet && moves_searched > 0)
        {
            int margin = 100 * depth;
            if (static_eval + margin <= alpha)
                continue;
        }

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

        if (history_ply < MAX_GAME_PLY)
            position_history[history_ply++] = hash;

        int score;
        PVLine child_pv = {0};

        if (moves_searched >= 4 && depth >= 3 && !in_check && !is_cap)
        {
            int reduction = 1 + (moves_searched >= 8) + (depth >= 6);
            score = -search(&copy, depth - 1 - reduction, ply + 1,
                            -alpha - 1, -alpha, stop, NULL)
                         .score;
            if (!stop->stop && score > alpha)
                score = -search(&copy, depth - 1, ply + 1,
                                -alpha - 1, -alpha, stop, NULL)
                             .score;
            if (!stop->stop && score > alpha)
                score = -search(&copy, depth - 1, ply + 1,
                                -beta, -alpha, stop, &child_pv)
                             .score;
        }
        else if (moves_searched > 0)
        {
            score = -search(&copy, depth - 1, ply + 1,
                            -alpha - 1, -alpha, stop, NULL)
                         .score;
            if (!stop->stop && score > alpha)
                score = -search(&copy, depth - 1, ply + 1,
                                -beta, -alpha, stop, &child_pv)
                             .score;
        }
        else
        {
            score = -search(&copy, depth - 1, ply + 1,
                            -beta, -alpha, stop, &child_pv)
                         .score;
        }

        history_ply--;
        moves_searched++;

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

    if (!stop->stop)
    {
        int flag;
        if (best_score <= alpha_orig)
            flag = TT_ALPHA;
        else if (best_score >= beta)
            flag = TT_BETA;
        else
            flag = TT_EXACT;

        int store_score = best_score;
        if (store_score > 31000)
            store_score += ply;
        else if (store_score < -31000)
            store_score -= ply;

        tt_store(hash, store_score, best_move, depth, flag);
    }

    output.score = best_score;
    output.move = best_move;
    return output;
}

uint16_t iterative_deepening(Position *board, stopConditions *stop)
{
    clear_ordering_tables();
    int prev_score = 0;
    uint16_t best_move_so_far = 0;
    PVLine best_pv = {0};
    long long search_start = get_time_ms();

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        if (stop->depth > 0 && depth > stop->depth)
            break;
        stop->seldepth = 0;
        int window = 50;

        int alpha = prev_score - window;
        int beta = prev_score + window;

        PVLine pv = {0};
        searchOutput out = search(board, depth, 0, alpha, beta, stop, &pv);

        if (!stop->stop && out.score <= alpha)
        {
            pv = (PVLine){0};
            out = search(board, depth, 0, -32000, beta, stop, &pv);
        }
        else if (!stop->stop && out.score >= beta)
        {
            pv = (PVLine){0};
            out = search(board, depth, 0, alpha, 32000, stop, &pv);
        }

        if (stop->stop)
            break;

        if (out.move != 0)
        {
            best_move_so_far = out.move;
            best_pv = pv;
        }
        prev_score = out.score;

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

        char pv_str[1024] = {0};
        int pos = 0;
        for (int i = 0; i < best_pv.length && i < depth && pos < (int)sizeof(pv_str) - 7; i++)
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