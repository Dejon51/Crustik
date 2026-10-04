#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#include "play.h"
#include "lmath.h"
#include "eval.h"
#include "search.h"
#include "tt.h"
#include "zobrist.h"
#include "params.h"
#include "ordermoves.h"

uint64_t game_history[MAX_GAME_PLY];
int game_history_count = 0;

static uint64_t search_path_hash[MAX_SEARCH_PLY];

int butterfly_hist[2][64][64];
uint16_t killer_moves[MAX_GAME_PLY][2];
int eval_stack[MAX_GAME_PLY];

int cont_hist[2][6][64][6][64];
int capture_history[2][6][64][6];

static int pawn_corrhist[2][CORRHIST_SIZE];
static int nonpawn_corrhist[2][CORRHIST_SIZE];
static uint64_t pawn_corrhist_keys[2][64];
static bool corrhist_initialized = false;


#ifndef LMRCORR_SIZE
#define LMRCORR_SIZE 16384
#endif
#define LMRCORR_MASK (LMRCORR_SIZE - 1)

#ifndef LMRCORR_LIMIT
#define LMRCORR_LIMIT 16384 
#endif

#ifndef LMRCORR_GRAIN
#define LMRCORR_GRAIN 16
#endif

#ifndef LMRC_FAIL_HIGH_MULT
#define LMRC_FAIL_HIGH_MULT 96
#endif

#ifndef LMRC_FAIL_LOW_MULT
#define LMRC_FAIL_LOW_MULT 6
#endif

static int lmr_corrhist[2][LMRCORR_SIZE];

ContRecord cont_stack[MAX_SEARCH_PLY];

static void init_corrhist(void)
{
    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    for (int c = 0; c < 2; c++)
    {
        for (int sq = 0; sq < 64; sq++)
        {
            seed += 0x9E3779B97F4A7C15ULL;
            uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            z = z ^ (z >> 31);
            pawn_corrhist_keys[c][sq] = z;
        }
    }
    corrhist_initialized = true;
}

void reset_history(void)
{
    memset(butterfly_hist, 0, sizeof butterfly_hist);
    memset(killer_moves, 0, sizeof killer_moves);
    memset(cont_hist, 0, sizeof cont_hist);
    memset(capture_history, 0, sizeof capture_history);
    memset(cont_stack, 0, sizeof cont_stack);
    memset(pawn_corrhist, 0, sizeof pawn_corrhist);
    memset(nonpawn_corrhist, 0, sizeof nonpawn_corrhist);
    memset(lmr_corrhist, 0, sizeof lmr_corrhist);
    if (!corrhist_initialized)
        init_corrhist();
    for (int i = 0; i < MAX_GAME_PLY; i++)
        eval_stack[i] = NO_EVAL;
}

int lmr_table[MAX_DEPTH + 1][MAX_LMR_MOVES + 1];

void init_lmr()
{
    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        for (int move = 1; move <= MAX_LMR_MOVES; move++)
        {
            int r = (int)(log((double)depth) *
                          log((double)move) / LMR_DIVISOR);

            if (r < 1)
                r = 1;

            if (r > depth - 2)
                r = depth - 2;

            if (r > LMR_MAX)
                r = LMR_MAX;

            lmr_table[depth][move] = r;
        }
    }
}

static inline int quiet_history_score(Position *board, int ply, int move)
{
    int from = move_from(move);
    int to = move_to(move);
    int piece = piece_on_square(board, from);

    int score = butterfly_hist[board->turn][from][to];

    if (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid && piece != -1)
    {
        int prev_piece = cont_stack[ply - 1].piece;
        int prev_to = cont_stack[ply - 1].to;
        score += cont_hist[board->turn][prev_piece][prev_to][piece][to];
    }

    return score;
}

static void move_to_uci(uint16_t move, char *buf)
{
    int from = move_from(move);
    int to = move_to(move);
    int flag = move_flag(move);

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

static bool is_repetition_or_fifty(Position *board, int ply)
{
    if (board->halfmoves >= 100)
        return true;

    int reversible_plies = board->halfmoves;

    int base = game_history_count > 0 ? game_history_count - 1 : 0;
    int total_ply = base + ply;

    for (int i = 4; i <= reversible_plies && i <= total_ply; i += 2)
    {
        uint64_t past_hash;
        int idx = total_ply - i;

        if (idx >= base)
        {
            int local_idx = idx - base;
            if (local_idx < 0 || local_idx >= MAX_SEARCH_PLY)
                continue;
            past_hash = search_path_hash[local_idx];
        }
        else
        {
            past_hash = game_history[idx];
        }

        if (past_hash == board->hash)
            return true;
    }

    return false;
}

static uint64_t compute_pawn_key(Position *board)
{
    uint64_t key = 0;
    uint64_t bb;

    bb = board->pieces[0] & board->color[0];
    while (bb)
    {
        int sq = __builtin_ctzll(bb);
        key ^= pawn_corrhist_keys[0][sq];
        bb &= bb - 1;
    }

    bb = board->pieces[0] & board->color[1];
    while (bb)
    {
        int sq = __builtin_ctzll(bb);
        key ^= pawn_corrhist_keys[1][sq];
        bb &= bb - 1;
    }

    return key;
}

static int compute_material_key(Position *board)
{
    int key = 0;
    int mult = 1;

    for (int type = 1; type <= 4; type++)
    {
        for (int c = 0; c < 2; c++)
        {
            int count = __builtin_popcountll(board->pieces[type] & board->color[c]);
            if (count > 10)
                count = 10;
            key += count * mult;
            mult *= 11;
        }
    }
    return key & CORRHIST_MASK;
}

static int clamp_int_local(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int corrected_eval(Position *board, int raw_eval)
{
    uint64_t pkey = compute_pawn_key(board) & CORRHIST_MASK;
    int mkey = compute_material_key(board);

    int correction = pawn_corrhist[board->turn][pkey] +
                     nonpawn_corrhist[board->turn][mkey];

    correction /= CORRHIST_GRAIN;
    correction = clamp_int_local(correction, -CORRHIST_MAX_APPLY, CORRHIST_MAX_APPLY);

    return raw_eval + correction;
}

static void update_corrhist(Position *board, int depth, int static_eval, int best_score)
{
    if (is_mate_score(best_score))
        return;

    int diff = best_score - static_eval;
    int bonus = clamp_int_local(diff * depth, -CORRHIST_LIMIT, CORRHIST_LIMIT);

    uint64_t pkey = compute_pawn_key(board) & CORRHIST_MASK;
    int mkey = compute_material_key(board);
    int side = board->turn;

    int *pc = &pawn_corrhist[side][pkey];
    *pc += bonus - *pc * abs(bonus) / CORRHIST_LIMIT;

    int *npc = &nonpawn_corrhist[side][mkey];
    *npc += bonus - *npc * abs(bonus) / CORRHIST_LIMIT;
}

static void update_lmr_corrhist(int side, int key, int depth, bool fail_high)
{
    int bonus;

    if (fail_high)
        bonus = -clamp_int_local(depth * LMRC_FAIL_HIGH_MULT, 0, LMRCORR_LIMIT / 4);
    else
        bonus = clamp_int_local(depth * LMRC_FAIL_LOW_MULT, 0, LMRCORR_LIMIT / 4);

    int *c = &lmr_corrhist[side][key];
    *c += bonus - *c * abs(bonus) / LMRCORR_LIMIT;
}

int quiesce(Position *board, int alpha, int beta, int ply, stopConditions *stop)
{
    stop->nodes++;

    if (ply > stop->seldepth)
        stop->seldepth = ply;

    if (stop->max_time && stop->start_time && (stop->nodes & 2047) == 0 &&
        get_time_ms() - stop->start_time >= stop->max_time)
        stop->stop = 1;

    if (stop->max_nodes && stop->nodes >= stop->max_nodes)
        stop->stop = 1;

    if (stop->stop)
        return eval(board, ply);

    if (ply >= MAX_SEARCH_PLY - 1)
        return eval(board, ply);

    int alpha_orig = alpha;
    uint16_t tt_move = 0;
    int in_check = king_in_check(board, board->turn);

    TTEntry *entry = tt_probe(board->hash);
    if (entry)
    {
        int tt_score = score_from_tt(entry->score, ply);

        if (entry->flag == TT_EXACT)
            return tt_score;
        if (entry->flag == TT_ALPHA && tt_score <= alpha)
            return tt_score;
        if (entry->flag == TT_BETA && tt_score >= beta)
            return tt_score;

        tt_move = entry->move;
    }

    int static_eval;
    int best_score;

    if (in_check)
    {
        static_eval = NO_EVAL;
        best_score = -MATE_SCORE + ply;
    }
    else
    {
        static_eval = (entry && entry->eval != NO_EVAL) ? entry->eval : eval(board, ply);

        if (static_eval >= beta)
            return static_eval;

        if (static_eval > alpha)
            alpha = static_eval;

        best_score = static_eval;
    }

    const int futility_margin = QS_FUTILITY_MARGIN;
    int futility_base = in_check ? 0 : static_eval + futility_margin;

    MoveList move_list = {0};

    if (in_check)
        legalMoveGen(board, &move_list);
    else
        qsearchMoves(board, &move_list, board->turn);

    move_list = ordermoves(board, &move_list, ply, tt_move);

    uint16_t best_move = 0;
    int legal_moves_seen = 0;

    for (unsigned int i = 0; i < move_list.offset; i++)
    {
        if (stop->stop)
            break;

        uint16_t move = move_list.movelist[i];

        if (!in_check)
        {
            if (!is_mate_score(alpha) && !is_mate_score(beta) && !is_promotion_move(move))
            {
                int from = move_from(move);
                int to = move_to(move);
                int victim = piece_on_square(board, to);
                int attacker = piece_on_square(board, from);

                int gain = (victim != -1) ? piece_value_lva(victim) : 0;

                if (victim == -1 && attacker == 0 && (from & 7) != (to & 7))
                    gain = piece_value_lva(0);

                int futility_value = futility_base + gain;

                if (futility_value <= alpha)
                {
                    if (futility_value > best_score)
                        best_score = futility_value;
                    continue;
                }

                if (futility_base <= alpha && !see_ge(board, move, QS_FUTILITY_SEE_THRESHOLD))
                {
                    if (futility_base > best_score)
                        best_score = futility_base;
                    continue;
                }
            }

            if (!see_ge(board, move, QS_SEE_THRESHOLD))
                continue;
        }

        nnue_update(board, move, ply, ply + 1);
        Position copy = *board;
        makeMove(&copy, &move_list, i);

        uint64_t king_bb = copy.pieces[5] & copy.color[board->turn];
        if (!king_bb || squareAttacked(&copy, __builtin_ctzll(king_bb), !board->turn))
            continue;

        legal_moves_seen++;

        int score = -quiesce(&copy, -beta, -alpha, ply + 1, stop);

        if (stop->stop)
            break;

        if (score > best_score)
        {
            best_score = score;
            best_move = move;
        }

        if (score >= beta)
        {
            tt_store(board->hash, score_to_tt(score, ply), move, 0, TT_BETA, 1,
                     in_check ? NO_EVAL : static_eval);
            return score;
        }

        if (score > alpha)
            alpha = score;
    }

    if (in_check && legal_moves_seen == 0 && !stop->stop)
        return -MATE_SCORE + ply;

    if (!stop->stop)
    {
        int qflag = (best_score <= alpha_orig) ? TT_ALPHA : TT_EXACT;
        tt_store(board->hash, score_to_tt(best_score, ply), best_move, 0, qflag, 1,
                 in_check ? NO_EVAL : static_eval);
    }

    return best_score;
}

searchOutput search(Position *board, int depth, int ply, int alpha, int beta,
                    stopConditions *stop, PVLine *pv, SearchStack *stack)
{
    searchOutput output = {0};
    int alpha_orig = alpha;
    uint16_t tt_move = 0;

    SearchStack no_excl = {0};
    if (!stack)
        stack = &no_excl;

    bool excluded = stack->excluded_move != 0;

    stop->nodes++;

    if (pv)
        pv->length = 0;

    if (ply > stop->seldepth)
        stop->seldepth = ply;

    if (stop->max_time && stop->start_time && (stop->nodes & 2047) == 0 &&
        get_time_ms() - stop->start_time >= stop->max_time)
        stop->stop = 1;

    if (stop->max_nodes && stop->nodes >= stop->max_nodes)
        stop->stop = 1;

    if (stop->stop)
        return output;

    if (ply >= MAX_SEARCH_PLY - 1)
        return (searchOutput){.score = eval(board, ply), .move = 0};

    if (ply < MAX_SEARCH_PLY)
        search_path_hash[ply] = board->hash;

    if (ply > 0 && is_repetition_or_fifty(board, ply))
        return (searchOutput){.score = 0, .move = 0};

    TTEntry *entry = tt_probe(board->hash);
    if (entry && !entry->is_qsearch)
    {
        tt_move = entry->move;

        if (entry->depth >= depth && !pv && stack->excluded_move == 0)
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

    if (depth <= 0)
        return (searchOutput){.score = quiesce(board, alpha, beta, ply, stop), .move = 0};

    int in_check = king_in_check(board, board->turn);
    bool root_node = (ply == 0);

    if (in_check && ply < MAX_GAME_PLY)
        eval_stack[ply] = NO_EVAL;

    if (in_check && depth < MAX_DEPTH)
        depth++;

    if (depth >= IIR_MIN_DEPTH && tt_move == 0 && !in_check)
    {
        depth--;
    }

    int static_eval = 0;
    int ceval = 0;
    bool improving = false;

    int lmr_pkey = 0;

    if (!in_check)
    {
        if (entry && entry->eval != NO_EVAL)
            static_eval = entry->eval;
        else
            static_eval = eval(board, ply);

        ceval = corrected_eval(board, static_eval);

        lmr_pkey = (int)(compute_pawn_key(board) & LMRCORR_MASK);

        if (ply < MAX_GAME_PLY)
        {
            improving = (ply >= 2 && eval_stack[ply - 2] != NO_EVAL)
                            ? ceval > eval_stack[ply - 2]
                            : true;

            eval_stack[ply] = ceval;
        }

        if (!root_node && !excluded && depth <= RFP_MAX_DEPTH && !is_mate_score(beta))
        {
            int margin = RFP_MARGIN * depth;

            if (ceval - margin >= beta)
            {
                return (searchOutput){
                    .score = (ceval + beta) / 2,
                    .move = 0};
            }
        }
        bool has_non_pawn_material = (board->color[board->turn] & ~(board->pieces[0] | board->pieces[5])) != 0;
        if (!excluded && depth >= NMP_MIN_DEPTH && !root_node && ceval >= beta && has_non_pawn_material)
        {
            int R = NMP_BASE_R + depth / NMP_DEPTH_DIV + (ceval - beta > NMP_EVAL_MARGIN ? 1 : 0);
            if (R > depth - 1)
                R = depth - 1;

            Position copy = *board;
            make_null_move(&copy);
            nnue_copy(ply, ply + 1);

            if (ply < MAX_SEARCH_PLY)
                cont_stack[ply].valid = false;

            int score = -search(&copy, depth - R - 1,
                                ply + 1, -beta, -beta + 1,
                                stop, NULL, &no_excl)
                             .score;

            if (stop->stop)
                return (searchOutput){0};

            if (score >= beta)
                return (searchOutput){.score = beta, .move = 0};
        }
    }

    if (!pv && !in_check && depth >= PROBCUT_MIN_DEPTH &&
        abs(beta) < MATE_SCORE && stack->excluded_move == 0)
    {
        int probcut_beta = beta + PROBCUT_MARGIN;
        int probcut_depth = depth - PROBCUT_DEPTH_REDUCTION;

        bool tt_hit = (entry != NULL);
        int tt_depth = tt_hit ? entry->depth : 0;
        int tt_score = tt_hit ? score_from_tt(entry->score, ply) : 0;

        if (!tt_hit || tt_depth + PROBCUT_TT_DEPTH_MARGIN < depth || tt_score >= probcut_beta)
        {
            MoveList captures = {0};
            qsearchMoves(board, &captures, board->turn);
            captures = ordermoves(board, &captures, ply, tt_move);

            for (unsigned int i = 0; i < captures.offset; i++)
            {
                uint16_t move = captures.movelist[i];

                if (!see_ge(board, move, PROBCUT_SEE_THRESHOLD))
                    continue;

                nnue_update(board, move, ply, ply + 1);
                Position copy = *board;
                makeMove(&copy, &captures, i);

                uint64_t king_bb = copy.pieces[5] & copy.color[board->turn];
                if (!king_bb ||
                    squareAttacked(&copy, __builtin_ctzll(king_bb), !board->turn))
                    continue;

                stop->nodes++;

                int probcut_value = -quiesce(&copy, -probcut_beta,
                                             -probcut_beta + 1, ply + 1, stop);

                if (!stop->stop && probcut_value >= probcut_beta)
                {
                    probcut_value = -search(&copy, probcut_depth, ply + 1,
                                            -probcut_beta, -probcut_beta + 1,
                                            stop, NULL, &no_excl)
                                         .score;
                }

                if (stop->stop)
                    return (searchOutput){0};

                if (probcut_value >= probcut_beta)
                {
                    tt_store(board->hash,
                             score_to_tt(probcut_value, ply),
                             move, probcut_depth, TT_ALPHA, 0,
                             in_check ? NO_EVAL : static_eval);

                    return (searchOutput){.score = probcut_value, .move = move};
                }
            }
        }
    }

    MovePicker mp;
    movepicker_init(&mp, board, ply, tt_move, in_check);

    int best_score = -MATE_SCORE;
    uint16_t best_move = 0;
    int searched_any = 0;
    int moves_seen = 0;

    uint16_t tried_captures[64];
    int num_tried_captures = 0;

    uint16_t move;
    int i = 0;
    for (; (move = movepicker_next(&mp)) != 0; i++)
    {
        if (move == stack->excluded_move)
            continue;

        moves_seen++;

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
            depth <= LMP_MAX_DEPTH &&
            !is_capture &&
            !is_killer &&
            i >= (improving ? LMP_IMPROVING_COUNT : LMP_NONIMPROVING_COUNT))
        {
            mp.skip_quiets = true;
            continue;
        }

        if (!root_node &&
            !in_check &&
            depth <= SEE_PRUNE_MAX_DEPTH &&
            !is_mate_score(alpha) &&
            !is_mate_score(beta))
        {
            int see_threshold = is_capture ? -SEE_PRUNE_CAPTURE_MARGIN * depth : -SEE_PRUNE_QUIET_MARGIN * depth;
            if (!see_ge(board, move, see_threshold))
                continue;
        }
        if (!root_node &&
            !in_check &&
            !is_capture &&
            !is_killer &&
            !is_promotion &&
            depth <= HISTPRUNE_MAX_DEPTH &&
            i >= HISTPRUNE_MIN_MOVES &&
            move != tt_move)
        {
            int hist_score = quiet_history_score(board, ply, move);
            int history_threshold = -HISTPRUNE_MARGIN * depth;
            if (hist_score < history_threshold)
                continue;
        }

        if (depth <= FUTILITY_MAX_DEPTH && !in_check && !is_mate_score(alpha) && !is_mate_score(beta))
        {
            int futility_margin = FUTILITY_BASE + FUTILITY_DEPTH_MARGIN * depth;
            if (ceval + futility_margin <= alpha)
            {
                if (!is_capture && !is_promotion)
                {
                    continue;
                }
            }
        }

        int extension = 0;

        if (!root_node &&
            stack->excluded_move == 0 &&
            move == tt_move &&
            entry != NULL &&
            entry->move == tt_move &&
            depth >= SE_MIN_DEPTH &&
            entry->depth >= depth - SE_TT_DEPTH_MARGIN &&
            entry->flag != TT_ALPHA &&
            !is_mate_score(entry->score))
        {
            int tt_score = score_from_tt(entry->score, ply);
            int singular_beta = tt_score - SE_BETA_MARGIN * depth;
            int singular_depth = (depth - SE_DEPTH_SUB) / SE_DEPTH_DIV;

            SearchStack singular_stack = {.excluded_move = move};

            searchOutput se_result = search(board, singular_depth, ply,
                                            singular_beta - 1, singular_beta,
                                            stop, NULL, &singular_stack);

            if (stop->stop)
                return (searchOutput){0};

            if (se_result.score < singular_beta)
            {
                extension = SE_EXTENSION;
            }
            else if (se_result.score >= beta && (beta - alpha) == 1)
            {
                return (searchOutput){.score = beta, .move = 0};
            }
            else if (tt_score >= beta)
            {
                extension = -SE_NEG_EXTENSION;
            }
        }

        int moved_piece = piece_on_square(board, move_from(move));
        int captured_piece = -1;
        if (is_capture)
        {
            captured_piece = piece_on_square(board, move_to(move));
            if (captured_piece == -1)
                captured_piece = 0;
        }

        Position copy = *board;
        moveint(&copy, move);

        uint64_t mover_king_bb = copy.pieces[5] & copy.color[board->turn];
        if (!mover_king_bb ||
            squareAttacked(&copy, __builtin_ctzll(mover_king_bb), !board->turn))
            continue;

        nnue_update(board, move, ply, ply + 1);
        searched_any = 1;
        if (best_move == 0)
            best_move = move;

        if (is_capture && captured_piece != -1 && num_tried_captures < 64)
            tried_captures[num_tried_captures++] = move;

        if (ply < MAX_SEARCH_PLY)
        {
            cont_stack[ply].piece = moved_piece;
            cont_stack[ply].to = move_to(move);
            cont_stack[ply].valid = true;
        }

        PVLine child_pv = {0};
        int score;

        if (i == 0 || depth < PVS_MIN_DEPTH)
        {
            score = -search(&copy, depth - 1 + extension, ply + 1,
                            -beta, -alpha, stop, &child_pv, &no_excl)
                         .score;
        }
        else
        {
            int reduction = 0;
            if (!root_node && !in_check && depth >= LMR_MIN_DEPTH && i >= LMR_MIN_MOVES &&
                !is_capture && !is_promotion && move != tt_move && !is_killer)
            {
                int is_pv_node = (beta - alpha) > 1;
                int from = move_from(move);
                int to = move_to(move);
                int hist = butterfly_hist[board->turn][from][to];

                reduction = lmr_reduction(depth, i + 1) * 1024;

                if (is_pv_node)
                {
                    reduction -= LMR_PV_NODE_SCALAR;
                }
                if (hist > LMR_BFHIST_THRESHOLD_PENALTY_SCALAR)
                {
                    reduction -= LMR_BFHIST_PENALTY_SCALAR;
                }
                else if (hist < -LMR_BFHIST_THRESHOLD_BONUS_SCALAR)
                {
                    reduction += LMR_BFHIST_BONUS_SCALAR;
                }

                reduction += lmr_corrhist[board->turn][lmr_pkey] / LMRCORR_GRAIN;

                reduction /= 1024;
                reduction = clamp_int(reduction, 0, depth - 1);
            }

            if (reduction > 0)
            {
                score = -search(&copy, depth - 1 - reduction, ply + 1,
                                -alpha - 1, -alpha, stop, NULL, &no_excl)
                             .score;

                if (!stop->stop)
                    update_lmr_corrhist(board->turn, lmr_pkey, depth, score > alpha);

                if (!stop->stop && score > alpha)
                {
                    score = -search(&copy, depth - 1 + extension, ply + 1,
                                    -alpha - 1, -alpha, stop, NULL, &no_excl)
                                 .score;
                }
            }
            else
            {
                score = -search(&copy, depth - 1 + extension, ply + 1,
                                -alpha - 1, -alpha, stop, NULL, &no_excl)
                             .score;
            }

            if (!stop->stop && score > alpha && score < beta)
            {
                child_pv.length = 0;
                score = -search(&copy, depth - 1 + extension, ply + 1,
                                -beta, -alpha, stop, &child_pv, &no_excl)
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
                int malus = -clamp_int(HIST_MALUS_MULT * depth - HIST_MALUS_BASE, 0, HIST_MAX);

                butterfly_hist[board->turn][from][to] +=
                    malus -
                    butterfly_hist[board->turn][from][to] *
                        abs(malus) / HIST_MAX;

                if (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid)
                {
                    int pp = cont_stack[ply - 1].piece;
                    int pt = cont_stack[ply - 1].to;
                    int *ch = &cont_hist[board->turn][pp][pt][moved_piece][to];
                    *ch += malus - *ch * abs(malus) / HIST_MAX;
                }
            }
            else if (is_capture && captured_piece != -1)
            {
                int malus = -clamp_int(HIST_MALUS_MULT * depth - HIST_MALUS_BASE, 0, HIST_MAX);
                int *history_entry = &capture_history[board->turn][moved_piece][to][captured_piece];
                *history_entry += malus - *history_entry * abs(malus) / HIST_MAX;
            }
        }

        if (alpha >= beta)
        {
            int from = move_from(move);
            int to = move_to(move);

            if (!is_capture && !is_promotion)
            {
                int clampedBonus = clamp_int(HIST_BONUS_MULT * depth - HIST_BONUS_BASE, 0, HIST_MAX);
                butterfly_hist[board->turn][from][to] += clampedBonus - butterfly_hist[board->turn][from][to] * abs(clampedBonus) / HIST_MAX;

                if (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid)
                {
                    int pp = cont_stack[ply - 1].piece;
                    int pt = cont_stack[ply - 1].to;
                    int *ch = &cont_hist[board->turn][pp][pt][moved_piece][to];
                    *ch += clampedBonus - *ch * abs(clampedBonus) / HIST_MAX;
                }

                if (ply < MAX_GAME_PLY && killer_moves[ply][0] != move)
                {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }
            }
            else if (is_capture && captured_piece != -1)
            {
                int bonus = clamp_int(HIST_BONUS_MULT * depth - HIST_BONUS_BASE, 0, HIST_MAX);

                int *cutoff_capture_entry = &capture_history[board->turn][moved_piece][to][captured_piece];
                *cutoff_capture_entry += bonus - *cutoff_capture_entry * abs(bonus) / HIST_MAX;

                for (int k = 0; k < num_tried_captures; k++)
                {
                    uint16_t tried_move = tried_captures[k];
                    if (tried_move == move)
                        continue;

                    int tried_from = move_from(tried_move);
                    int tried_to = move_to(tried_move);
                    int other_attacker = piece_on_square(board, tried_from);
                    int other_victim = piece_on_square(board, tried_to);
                    if (other_victim == -1)
                        other_victim = 0;

                    if (other_attacker < 0 || other_victim < 0)
                        continue;

                    int *other_capture_entry = &capture_history[board->turn][other_attacker][tried_to][other_victim];
                    int malus = -bonus;
                    *other_capture_entry += malus - *other_capture_entry * abs(malus) / HIST_MAX;
                }
            }

            break;
        }
    }

    if (!searched_any)
    {
        if (excluded)
            return (searchOutput){.score = alpha, .move = 0};

        bool no_legal = (moves_seen == 0);
        if (!no_legal)
        {
            MoveList tmp = {0};
            legalMoveGen(board, &tmp);
            no_legal = (tmp.offset == 0);
        }

        if (no_legal)
            return (searchOutput){.score = in_check ? -MATE_SCORE + ply : 0, .move = 0};

        return (searchOutput){.score = in_check ? eval(board, ply) : static_eval, .move = 0};
    }

    if (!stop->stop && stack->excluded_move == 0 && !in_check)
        update_corrhist(board, depth, static_eval, best_score);

    if (!stop->stop && stack->excluded_move == 0)
    {
        int flag;
        if (best_score <= alpha_orig)
            flag = TT_ALPHA;
        else if (best_score >= beta)
            flag = TT_BETA;
        else
            flag = TT_EXACT;

        int tt_depth = depth;
        if (tt_depth > 255)
            tt_depth = 255;
        if (tt_depth < 0)
            tt_depth = 0;

        tt_store(board->hash, score_to_tt(best_score, ply), best_move, tt_depth, flag, 0, in_check ? NO_EVAL : static_eval);
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
    int aspiration_delta = ASP_DELTA;

    uint16_t prev_best_move = 0;
    int last_best_move_change = 0;

    double bm_changes = 0;
    double score_factor = 1.0;

    SearchStack no_excl = {0};
    nnue_refresh(board, 0);

    for (int depth = 1; depth <= MAX_DEPTH; depth++)
    {
        if (stop->soft_time > 0)
        {
            int iterations_stable = depth - last_best_move_change;
            double factor = TM_STABLE_BASE - TM_STABLE_STEP * (double)iterations_stable;
            if (factor < TM_STABLE_MIN)
                factor = TM_STABLE_MIN;
            if (factor > TM_STABLE_MAX)
                factor = TM_STABLE_MAX;

            double instability = 1.0 + TM_BM_INST_SCALE * bm_changes;

            int64_t effective_soft = (int64_t)(stop->soft_time * factor * instability * score_factor);
            if (effective_soft > (int64_t)stop->max_time)
                effective_soft = (int64_t)stop->max_time;

            int64_t elapsed = get_time_ms() - stop->start_time;
            if (elapsed >= effective_soft)
                break;
        }
        if (stop->soft_nodes > 0 && stop->nodes >= stop->soft_nodes)
            break;

        if (stop->depth > 0 && depth > stop->depth)
            break;

        stop->seldepth = 0;

        PVLine pv = {0};
        searchOutput out;

        int alpha, beta;
        bool first_attempt = (depth < ASP_MIN_DEPTH);
        if (first_attempt)
        {
            alpha = -MATE_SCORE;
            beta = MATE_SCORE;
        }
        else
        {
            alpha = prev_score - aspiration_delta;
            beta = prev_score + aspiration_delta;
        }

        int delta = aspiration_delta;
        int research_count = 0;
        int asp_reduction = 0;

        while (1)
        {
            pv.length = 0;

            int search_depth = depth - asp_reduction;
            if (search_depth < 1)
                search_depth = 1;

            out = search(board, search_depth, 0, alpha, beta, stop, &pv, &no_excl);

            if (stop->stop)
                break;

            if (out.score > alpha && out.score < beta)
                break;

            if (out.score <= alpha)
            {
                asp_reduction = 0;
                alpha = out.score - delta;
                if (alpha < -MATE_SCORE)
                    alpha = -MATE_SCORE;
            }
            else if (out.score >= beta)
            {
                if (asp_reduction < ASP_REDUCTION_MAX)
                    asp_reduction++;
                beta = out.score + delta;
                if (beta > MATE_SCORE)
                    beta = MATE_SCORE;
            }

            delta = (int)(delta * ASP_DELTA_GROWTH);
            if (delta > ASP_MAX_DELTA)
            {
                asp_reduction = 0;
                alpha = -MATE_SCORE;
                beta = MATE_SCORE;
            }

            research_count++;
            if (research_count >= ASP_MAX_RESEARCH)
            {
                alpha = -MATE_SCORE;
                beta = MATE_SCORE;
                pv.length = 0;
                out = search(board, depth, 0, alpha, beta, stop, &pv, &no_excl);
                break;
            }
        }

        if (stop->stop)
            break;
        if (depth >= TM_SCORE_DROP_DEPTH)
        {
            double diff = (double)(prev_score - out.score);
            if (diff > TM_SCORE_SWING_SCALE)
                diff = TM_SCORE_SWING_SCALE;
            if (diff < -TM_SCORE_SWING_SCALE)
                diff = -TM_SCORE_SWING_SCALE;

            score_factor = pow(2.0, diff / TM_SCORE_SWING_SCALE);
        }
        else
        {
            score_factor = 1.0;
        }
        prev_score = out.score;

        if (out.move != 0)
        {
            if (out.move != prev_best_move)
            {
                last_best_move_change = depth;
                bm_changes += 1.0;
            }

            prev_best_move = out.move;
            best_move_so_far = out.move;
            best_pv = pv;
        }

        bm_changes *= TM_BM_DECAY;

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

        printf("info depth %d seldepth %d score %s nodes %" PRIu64 " nps %lld time %" PRId64 " pv %s\n",
               depth, stop->seldepth, score_str,
               (uint64_t)stop->nodes,
               nps, elapsed,
               pv_str);
        fflush(stdout);
    }

    return best_move_so_far;
}