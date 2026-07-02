#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
#define MAX_DEPTH 400
#define MAX_GAME_PLY 2048

#define REPETITION_AVOID_SCORE 32
#define REPETITION_THREEFOLD_SCORE 96

#define HISTORY_SCORE_CAP 16384

// Pawn history table size must be power of two.
#define PAWN_HISTORY_SIZE 4096
#define PAWN_HISTORY_MASK (PAWN_HISTORY_SIZE - 1)

// Search-line history. This is pushed/popped inside search.
uint64_t position_history[MAX_GAME_PLY];
int history_ply = 0;

static uint64_t root_position_history[MAX_GAME_PLY];
static int root_history_ply = 0;
static uint64_t last_recorded_root_hash = 0;
static int have_last_recorded_root = 0;

static uint16_t killers[MAX_DEPTH][2];
static uint16_t counter_moves[64][64];
static uint16_t search_stack_moves[MAX_DEPTH + 1];
static int static_eval_stack[MAX_DEPTH + 1];

static int history[2][64][64];
static int capture_history[2][64][64];

// NEW: pawn-history heuristic.
// Indexed by side, pawn-structure bucket, moving piece type, destination square.
static int pawn_history[2][PAWN_HISTORY_SIZE][6][64];

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}


static uint64_t mix_u64(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static int pawn_history_index(Position *board)
{
    uint64_t white_pawns = board->pieces[0] & board->color[0];
    uint64_t black_pawns = board->pieces[0] & board->color[1];

    uint64_t key = mix_u64(white_pawns) ^ (mix_u64(black_pawns) >> 1);
    return (int)(key & PAWN_HISTORY_MASK);
}

void clear_ordering_tables(void)
{
    memset(killers, 0, sizeof(killers));
    memset(counter_moves, 0, sizeof(counter_moves));
    memset(search_stack_moves, 0, sizeof(search_stack_moves));
    memset(static_eval_stack, 0, sizeof(static_eval_stack));
    memset(history, 0, sizeof(history));
    memset(capture_history, 0, sizeof(capture_history));
    memset(pawn_history, 0, sizeof(pawn_history));

    memset(position_history, 0, sizeof(position_history));
    history_ply = 0;
    memset(root_position_history, 0, sizeof(root_position_history));
    root_history_ply = 0;
    last_recorded_root_hash = 0;
    have_last_recorded_root = 0;
}

static void append_root_history(uint64_t hash)
{
    if (root_history_ply < MAX_GAME_PLY)
    {
        root_position_history[root_history_ply++] = hash;
        return;
    }

    memmove(root_position_history, root_position_history + 1,
            (MAX_GAME_PLY - 1) * sizeof(root_position_history[0]));
    root_position_history[MAX_GAME_PLY - 1] = hash;
}

static void trim_root_history_to_keep(int keep)
{
    if (keep <= 0)
    {
        root_history_ply = 0;
        return;
    }

    if (root_history_ply > keep)
    {
        memmove(root_position_history,
                root_position_history + root_history_ply - keep,
                keep * sizeof(root_position_history[0]));
        root_history_ply = keep;
    }
}

static void reset_root_history(void)
{
    root_history_ply = 0;
    last_recorded_root_hash = 0;
    have_last_recorded_root = 0;
}

static void sync_root_history(Position *board)
{
    if (board->halfmoves <= 0)
    {
        reset_root_history();
        return;
    }

    trim_root_history_to_keep(board->halfmoves);
}

static void remember_root_move(Position *board, uint16_t best_move)
{
    if (!best_move)
        return;

    if (have_last_recorded_root && last_recorded_root_hash == board->hash)
        return;

    MoveList move_list;
    move_list.offset = 0;
    legalMoveGen(board, &move_list);

    int move_index = -1;
    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        if (move_list.movelist[i] == best_move)
        {
            move_index = (int)i;
            break;
        }
    }

    if (move_index < 0)
        return;

    append_root_history(board->hash);

    Position copy = *board;
    makeMove(&copy, &move_list, (unsigned int)move_index);
    append_root_history(copy.hash);

    trim_root_history_to_keep(copy.halfmoves + 1);

    last_recorded_root_hash = board->hash;
    have_last_recorded_root = 1;
}

static int repetition_count(Position *board, uint64_t hash)
{
  
    int occurrences = 1;

    int search_limit = history_ply - board->halfmoves;
    if (search_limit < 0)
        search_limit = 0;

    for (int i = history_ply - 1; i >= search_limit; i--)
        if (position_history[i] == hash)
            occurrences++;

    int root_limit = root_history_ply - board->halfmoves;
    if (root_limit < 0)
        root_limit = 0;

    for (int i = root_history_ply - 1; i >= root_limit; i--)
        if (root_position_history[i] == hash)
            occurrences++;

    return occurrences;
}

static int is_threefold_repetition(Position *board, uint64_t hash)
{
    if (board->halfmoves < 4)
        return 0;

    return repetition_count(board, hash) >= 3;
}

static int repetition_avoid_penalty(Position *board, uint64_t hash)
{
    if (board->halfmoves < 4)
        return 0;

    int count = repetition_count(board, hash);

    if (count >= 3)
        return REPETITION_THREEFOLD_SCORE;
    if (count == 2)
        return REPETITION_AVOID_SCORE;

    return 0;
}

static int count_root_non_threefold_moves(Position *board, MoveList *move_list)
{
    int count = 0;

    for (unsigned int i = 0; i < move_list->offset; i++)
    {
        Position copy = *board;
        makeMove(&copy, move_list, i);

        if (repetition_count(&copy, copy.hash) < 3)
            count++;
    }

    return count;
}

static void store_killer(uint16_t move, int ply)
{
    if (ply < 0 || ply >= MAX_DEPTH)
        return;

    if (killers[ply][0] != move)
    {
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = move;
    }
}

static int quiet_history_score(Position *board, uint16_t move)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    return history[board->turn & 1][from][to];
}

static int capture_history_score(Position *board, uint16_t move)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    return capture_history[board->turn & 1][from][to];
}

static int pawn_history_score(Position *board, uint16_t move)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int piece = board->mailbox[from];

    if (piece < 0 || piece >= 6)
        return 0;

    int side = board->turn & 1;
    int idx = pawn_history_index(board);

    return pawn_history[side][idx][piece][to];
}

static void update_quiet_history(Position *board, uint16_t move, int bonus)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int *entry = &history[board->turn & 1][from][to];

    *entry += bonus - ((*entry * abs(bonus)) / HISTORY_SCORE_CAP);
    *entry = clamp_int(*entry, -HISTORY_SCORE_CAP, HISTORY_SCORE_CAP);
}

static void update_capture_history(Position *board, uint16_t move, int bonus)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int *entry = &capture_history[board->turn & 1][from][to];

    *entry += bonus - ((*entry * abs(bonus)) / HISTORY_SCORE_CAP);
    *entry = clamp_int(*entry, -HISTORY_SCORE_CAP, HISTORY_SCORE_CAP);
}

static void update_pawn_history(Position *board, uint16_t move, int bonus)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int piece = board->mailbox[from];

    if (piece < 0 || piece >= 6)
        return;

    int side = board->turn & 1;
    int idx = pawn_history_index(board);
    int *entry = &pawn_history[side][idx][piece][to];

    *entry += bonus - ((*entry * abs(bonus)) / HISTORY_SCORE_CAP);
    *entry = clamp_int(*entry, -HISTORY_SCORE_CAP, HISTORY_SCORE_CAP);
}

static int is_counter_move(uint16_t move, int ply)
{
    if (ply <= 0 || ply > MAX_DEPTH)
        return 0;

    uint16_t prev = search_stack_moves[ply - 1];
    if (!prev)
        return 0;

    int prev_from = (prev >> 6) & 0x3F;
    int prev_to = prev & 0x3F;
    return counter_moves[prev_from][prev_to] == move;
}

static void store_counter_move(uint16_t move, int ply)
{
    if (ply <= 0 || ply > MAX_DEPTH)
        return;

    uint16_t prev = search_stack_moves[ply - 1];
    if (!prev)
        return;

    int prev_from = (prev >> 6) & 0x3F;
    int prev_to = prev & 0x3F;
    counter_moves[prev_from][prev_to] = move;
}

static int lmr_reduction(int depth, int moves_searched, int is_pv_node,
                         int in_check, int is_quiet, int improving,
                         int hist_score, int is_counter)
{
    int r = 1;

    if (depth >= 5)
        r++;
    if (depth >= 8)
        r++;
    if (moves_searched >= 8)
        r++;
    if (moves_searched >= 16)
        r++;

    if (is_pv_node)
        r--;
    if (in_check)
        r--;
    if (improving)
        r--;
    if (is_counter)
        r--;
    if (!is_quiet)
        r--;

    if (hist_score > 4096)
        r--;
    else if (hist_score < -4096)
        r++;

    return clamp_int(r, 0, depth - 2);
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

    int stm = pos->turn ^ 1;
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

        if (-gain[depth] > gain[depth - 1])
            break;

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

    while (--depth > 0)
    gain[depth - 1] = -(gain[depth] > -gain[depth - 1]
                        ? gain[depth]
                        : -gain[depth - 1]);

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
        int flag = (move >> 12) & 0xF;
        int victim = board->mailbox[to];

        if (move == tt_move)
        {
            scores[i] = 1000000;
            continue;
        }

        if (flag == 8)
        {
            scores[i] = 950000;
            continue;
        }

        if (flag == 7)
        {
            scores[i] = 930000;
            continue;
        }

        if (flag == 5)
        {
            scores[i] = 920000;
            continue;
        }

        if (flag == 6)
        {
            scores[i] = 910000;
            continue;
        }

        if (victim != 0 && victim != 6)
        {
            int see = SEE(board, move);
            int cap_hist = capture_history_score(board, move);

            if (see >= 0)
                scores[i] = 800000 + see * 8 + cap_hist;
            else
                scores[i] = 50000 + see * 8 + cap_hist;

            continue;
        }

        if (ply >= 0 && ply < MAX_DEPTH && move == killers[ply][0])
        {
            scores[i] = 700000;
            continue;
        }

        if (ply >= 0 && ply < MAX_DEPTH && move == killers[ply][1])
        {
            scores[i] = 690000;
            continue;
        }

        if (is_counter_move(move, ply))
        {
            scores[i] = 680000;
            continue;
        }

        scores[i] = 100000 + quiet_history_score(board, move) + pawn_history_score(board, move);
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

        if (is_threefold_repetition(board, hash))
            return (searchOutput){.score = REPETITION_THREEFOLD_SCORE, .move = 0};
    }

    uint16_t tt_move = 0;
    TTEntry *entry = tt_probe(hash);
    if (entry)
    {
        tt_move = entry->move;

        // Keep TT move ordering at root, but do not allow root TT cutoffs.
        // Otherwise iterative deepening can return before searching any ply,
        // causing seldepth 0 and an empty PV on repeated/effective depths.
        if (ply > 0 && entry->depth >= depth)
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

    uint64_t king_bb = board->pieces[5] & board->color[board->turn];
    int in_check = (!king_bb ||
                    squareAttacked(board, __builtin_ctzll(king_bb), !board->turn));

    // Mate distance pruning: once a mate is already proven, do not search
    // lines that can only produce a worse mate distance.
    if (ply > 0)
    {
        alpha = alpha > (-MATE_SCORE + ply) ? alpha : (-MATE_SCORE + ply);
        beta = beta < (MATE_SCORE - ply - 1) ? beta : (MATE_SCORE - ply - 1);
        if (alpha >= beta)
            return (searchOutput){.score = alpha, .move = 0};
    }

    // Check extension. This keeps check evasions out of qsearch and gives the
    // search one extra ply to resolve forcing positions.
    if (in_check && depth < MAX_DEPTH)
        depth++;

    if (depth <= 0)
    {
        output.score = quiesce(board, alpha, beta, ply, stop);
        return output;
    }

    int static_eval = eval(board);
    if (ply <= MAX_DEPTH)
        static_eval_stack[ply] = static_eval;

    int improving = 0;
    if (ply >= 2 && ply <= MAX_DEPTH && !in_check)
        improving = static_eval > static_eval_stack[ply - 2];

    int is_pv_node = (beta - alpha) > 1;

    // Internal iterative reduction: if we have no hash move, move ordering is
    // less reliable, so spend slightly less depth here and let the next ID
    // iteration improve the TT entry.
    if (ply > 0 && depth >= 4 && !in_check && tt_move == 0 && !is_pv_node)
        depth--;

    // Reverse futility pruning / fail-soft-ish return for obvious fail highs.
    if (ply > 0 && !is_pv_node && !in_check && depth <= 6 && abs(beta) < 31000)
    {
        int margin = 120 * depth - (improving ? 40 : 0);
        if (static_eval - margin >= beta)
            return (searchOutput){.score = beta + (static_eval - beta) / 3, .move = 0};
    }

    int NULL_MOVE_REDUCTION = 3 + (depth >= 6) + (static_eval >= beta + 200);

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

            int score = -search(&copy, depth - 1 - NULL_MOVE_REDUCTION,
                                ply + 1, -beta, -beta + 1, stop, NULL)
                             .score;

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
    uint16_t quiets_searched[218];
    int quiet_count = 0;
    int saw_history_dependent_repetition = 0;
    int root_non_threefold_moves = 0;

    if (ply == 0)
        root_non_threefold_moves = count_root_non_threefold_moves(board, &move_list);

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        uint16_t move = move_list.movelist[i];
        int to = move & 0x3F;
        int victim = board->mailbox[to];
        int flag = (move >> 12) & 0xF;
        int is_cap = (victim != 0 && victim != 6);
        int is_promo = (flag >= 5 && flag <= 8);
        int is_quiet = !is_cap && !is_promo;

        int move_is_counter = is_counter_move(move, ply);
        int hist_score = is_quiet ? quiet_history_score(board, move) : 0;

        if (!is_pv_node && depth <= 4 && !in_check && is_quiet && moves_searched > 0)
        {
            int margin = 100 * depth + (improving ? 50 : 0);
            if (static_eval + margin <= alpha)
                continue;
        }

        if (!is_pv_node && ply > 0 && depth <= 4 && !in_check && is_quiet)
        {
            int lmp_limit = 4 + depth * depth;
            if (improving)
                lmp_limit *= 2;
            if (moves_searched >= lmp_limit)
                continue;
        }

        if (!is_pv_node && !in_check && is_cap && depth <= 5 && moves_searched > 0)
        {
            int see_margin = -50 * depth;
            if (SEE(board, move) < see_margin)
                continue;
        }

        Position copy = *board;
        makeMove(&copy, &move_list, i);

        if (ply == 0 && root_non_threefold_moves > 0 &&
            repetition_count(&copy, copy.hash) >= 3)
        {
            saw_history_dependent_repetition = 1;
            continue;
        }

        if (ply == 0 && stop->print_info)
        {
            char mv[6];
            move_to_uci(move, mv);
            printf("info depth %d currmove %s currmovenumber %d\n",
                   depth, mv, i + 1);
            fflush(stdout);
        }

        int pushed_history = 0;
        if (history_ply < MAX_GAME_PLY)
        {
            position_history[history_ply++] = hash;
            pushed_history = 1;
        }

        if (ply <= MAX_DEPTH)
            search_stack_moves[ply] = move;

        int score;
        PVLine child_pv = {0};
        int rep_penalty = repetition_avoid_penalty(&copy, copy.hash);

        if (rep_penalty > 0)
        {
            score = -rep_penalty;
            saw_history_dependent_repetition = 1;
        }
        else if (moves_searched >= (is_pv_node ? 4 : 2) && depth >= 3 && !in_check && is_quiet)
        {
            int reduction = lmr_reduction(depth, moves_searched, is_pv_node,
                                          in_check, is_quiet, improving,
                                          hist_score, move_is_counter);
            int reduced_depth = depth - 1 - reduction;
            if (reduced_depth < 0)
                reduced_depth = 0;

            score = -search(&copy, reduced_depth, ply + 1,
                            -alpha - 1, -alpha, stop, NULL)
                         .score;
            if (!stop->stop && score > alpha && reduction > 0)
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

        if (pushed_history)
            history_ply--;
        moves_searched++;

        if (stop->stop)
            break;

        if (is_quiet && quiet_count < (int)(sizeof(quiets_searched) / sizeof(quiets_searched[0])))
            quiets_searched[quiet_count++] = move;

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
            int bonus = depth * depth;
            if (bonus > 4096)
                bonus = 4096;

            if (is_quiet)
            {
                store_killer(move, ply);
                store_counter_move(move, ply);

                update_quiet_history(board, move, bonus);
                update_pawn_history(board, move, bonus);

                for (int q = 0; q < quiet_count; q++)
                {
                    if (quiets_searched[q] != move)
                    {
                        update_quiet_history(board, quiets_searched[q], -bonus / 2);
                        update_pawn_history(board, quiets_searched[q], -bonus / 2);
                    }
                }
            }
            else if (is_cap)
            {
                update_capture_history(board, move, bonus);
            }
            break;
        }
    }

    if (!stop->stop && !saw_history_dependent_repetition)
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
    int prev_score = 0;
    uint16_t best_move_so_far = 0;
    PVLine best_pv = {0};
    long long search_start = get_time_ms();

    sync_root_history(board);
    history_ply = 0;

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

    remember_root_move(board, best_move_so_far);

    return best_move_so_far;
}