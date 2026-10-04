#include <stdbool.h>
#include <stdlib.h>
#include "lmath.h"
#include "search.h"
#include "params.h"
#include "play.h"
#include "magics.h"
#include "precomputed.h"
#include "ordermoves.h"

#if defined(__GNUC__) || defined(__clang__)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

void quietMoves(Position *board, MoveList *list, bool color);

MoveList ordermoves(Position *board, MoveList *move_list, int ply, uint16_t tt_move)
{
    MoveList ordered = *move_list;
    int scores[256] = {0};

    const int TT_SCORE = 100000000;
    const int CAPTURE_BASE = 90000000;
    const int KILLER_BASE = 80000000;

    bool have_cont = (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid);
    int cont_piece = have_cont ? cont_stack[ply - 1].piece : 0;
    int cont_to = have_cont ? cont_stack[ply - 1].to : 0;

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
        int attacker = piece_on_square(board, from);
        int victim = piece_on_square(board, to);
        bool is_capture = is_capture_move(board, move);

        if (is_capture && attacker != -1)
        {
            if (victim == -1)
                victim = 0;

            int mvv_lva = piece_value_lva(victim) * MVV_LVA_VICTIM_MULT - piece_value_lva(attacker);
            int history_score = capture_history[board->turn][attacker][to][victim];
            scores[i] = CAPTURE_BASE + mvv_lva + history_score;
            continue;
        }

        bool is_killer = false;
        if (ply < MAX_GAME_PLY)
        {
            if (move == killer_moves[ply][0] || move == killer_moves[ply][1])
            {
                int bonus = (move == killer_moves[ply][0]) ? 1 : 0;
                scores[i] = KILLER_BASE + bonus;
                is_killer = true;
            }
        }

        if (!is_killer)
        {
            int score = butterfly_hist[board->turn][from][to];

            if (have_cont && attacker != -1)
                score += cont_hist[board->turn][cont_piece][cont_to][attacker][to];

            scores[i] = score;
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

#define PK_CAPTURE_BASE 90000000
#define PK_KILLER_BASE 80000000

bool move_is_pseudolegal(Position *b, uint16_t m)
{
    int from = move_from(m), to = move_to(m), flag = move_flag(m);
    int us = b->turn;
    if (from == to)
        return false;

    int pc = b->mailbox[from];
    uint64_t from_bb = 1ULL << from, to_bb = 1ULL << to;
    if (pc >= 6 || !(b->color[us] & from_bb))
        return false;
    if (b->color[us] & to_bb)
        return false;

    uint64_t occ = b->color[WHITE] | b->color[BLACK];
    bool promo = flag >= 5 && flag <= 8;

    if (pc == PAWNNUMBER)
    {
        if (flag >= 1 && flag <= 4)
            return false;
        int dir = (us == 0) ? -8 : 8;
        int start_row = (us == 0) ? 6 : 1;
        int promo_row = (us == 0) ? 1 : 6;
        if (promo != ((from >> 3) == promo_row))
            return false;

        int diff = to - from;
        if (diff == dir)
            return !(occ & to_bb);
        if (diff == 2 * dir)
            return (from >> 3) == start_row && !(occ & to_bb) &&
                   !(occ & (1ULL << (from + dir)));
        if ((diff == dir - 1 || diff == dir + 1) &&
            abs((to & 7) - (from & 7)) == 1)
            return (b->color[!us] & to_bb) || (to == b->epsquare && b->epsquare != -1);
        return false;
    }

    if (promo)
        return false;

    if (pc == KINGNUMBER)
    {
        if (flag >= 1 && flag <= 4)
        {
            MoveList t = {0};
            quietMoves(b, &t, us);
            for (unsigned i = 0; i < t.offset; i++)
                if (t.movelist[i] == m)
                    return true;
            return false;
        }
        return flag == 0 && (kingtable[from] & to_bb);
    }

    if (flag)
        return false;
    if (pc == HORSENUMBER)
        return (knighttable[from] & to_bb) != 0;

    uint64_t att = 0;
    if (pc == BISHOPNUMBER || pc == QUEENNUMBER)
        att |= getBishopAttacks(from, occ);
    if (pc == ROOKNUMBER || pc == QUEENNUMBER)
        att |= getRookAttacks(from, occ);
    return (att & to_bb) != 0;
}

static int score_noisy(Position *b, uint16_t m)
{
    int from = move_from(m), to = move_to(m);
    int att = piece_on_square(b, from);
    int victim = piece_on_square(b, to);
    if (victim == -1 && att == 0 && (from & 7) != (to & 7))
        victim = 0;

    int s = 0;
    if (victim != -1 && att != -1)
        s = piece_value_lva(victim) * MVV_LVA_VICTIM_MULT - piece_value_lva(att) +
            capture_history[b->turn][att][to][victim];
    if (is_promotion_move(m))
        s += 1000000;
    return s;
}

static int score_quiet(Position *b, int ply, uint16_t m)
{
    if (ply < MAX_GAME_PLY)
    {
        if (m == killer_moves[ply][0])
            return PK_KILLER_BASE + 1;
        if (m == killer_moves[ply][1])
            return PK_KILLER_BASE;
    }

    int from = move_from(m), to = move_to(m);
    int piece = piece_on_square(b, from);
    int s = butterfly_hist[b->turn][from][to];

    if (ply > 0 && ply - 1 < MAX_SEARCH_PLY && cont_stack[ply - 1].valid && piece != -1)
        s += cont_hist[b->turn][cont_stack[ply - 1].piece][cont_stack[ply - 1].to][piece][to];
    return s;
}

static uint16_t pick_best(uint16_t *mv, int *sc, int i, int n)
{
    int best = i;
    for (int j = i + 1; j < n; j++)
        if (sc[j] > sc[best])
            best = j;
    uint16_t m = mv[best];
    int s = sc[best];
    mv[best] = mv[i];
    sc[best] = sc[i];
    mv[i] = m;
    sc[i] = s;
    return m;
}

void movepicker_init(MovePicker *mp, Position *b, int ply, uint16_t tt_move, bool in_check)
{
    mp->board = b;
    mp->ply = ply;
    mp->in_check = in_check;
    mp->tt_move = (tt_move && move_is_pseudolegal(b, tt_move)) ? tt_move : 0;
    mp->caps.offset = 0;
    mp->quiets.offset = 0;
    mp->cap_i = 0;
    mp->quiet_i = 0;
    mp->n_bad = 0;
    mp->bad_i = 0;
    mp->skip_quiets = false;
    mp->stage = mp->tt_move ? ST_TT : (in_check ? ST_GEN_EVASIONS : ST_GEN_CAPS);
}

uint16_t movepicker_next(MovePicker *mp)
{
    Position *b = mp->board;

    switch (mp->stage)
    {
    case ST_TT:
        mp->stage = mp->in_check ? ST_GEN_EVASIONS : ST_GEN_CAPS;
        return mp->tt_move;

    case ST_GEN_CAPS:
        mp->caps.offset = 0;
        qsearchMoves(b, &mp->caps, b->turn);
        for (unsigned i = 0; i < mp->caps.offset; i++)
            mp->cap_scores[i] = score_noisy(b, mp->caps.movelist[i]);
        mp->stage = ST_GOOD_CAPS;
        FALLTHROUGH;

    case ST_GOOD_CAPS:
        while (mp->cap_i < (int)mp->caps.offset)
        {
            uint16_t m = pick_best(mp->caps.movelist, mp->cap_scores,
                                   mp->cap_i, mp->caps.offset);
            mp->cap_i++;
            if (m == mp->tt_move)
                continue;
            if (!see_ge(b, m, 0))
            {
                if (mp->n_bad < 64)
                    mp->bad[mp->n_bad++] = m;
                continue;
            }
            return m;
        }
        mp->stage = ST_GEN_QUIETS;
        FALLTHROUGH;

    case ST_GEN_QUIETS:
        if (mp->skip_quiets)
        {
            mp->stage = ST_BAD_CAPS;
            goto bad_caps;
        }
        mp->quiets.offset = 0;
        quietMoves(b, &mp->quiets, b->turn);
        for (unsigned i = 0; i < mp->quiets.offset; i++)
            mp->quiet_scores[i] = score_quiet(b, mp->ply, mp->quiets.movelist[i]);
        mp->stage = ST_QUIETS;
        FALLTHROUGH;

    case ST_QUIETS:
        while (!mp->skip_quiets && mp->quiet_i < (int)mp->quiets.offset)
        {
            uint16_t m = pick_best(mp->quiets.movelist, mp->quiet_scores,
                                   mp->quiet_i, mp->quiets.offset);
            mp->quiet_i++;
            if (m == mp->tt_move)
                continue;
            return m;
        }
        mp->stage = ST_BAD_CAPS;
        FALLTHROUGH;

    case ST_BAD_CAPS:
    bad_caps:
        if (mp->bad_i < mp->n_bad)
            return mp->bad[mp->bad_i++];
        mp->stage = ST_DONE;
        return 0;

    case ST_GEN_EVASIONS:
        mp->caps.offset = 0;
        legalMoveGen(b, &mp->caps);
        for (unsigned i = 0; i < mp->caps.offset; i++)
        {
            uint16_t m = mp->caps.movelist[i];
            mp->cap_scores[i] = (is_capture_move(b, m) || is_promotion_move(m))
                                    ? PK_CAPTURE_BASE + score_noisy(b, m)
                                    : score_quiet(b, mp->ply, m);
        }
        mp->stage = ST_EVASIONS;
        FALLTHROUGH;

    case ST_EVASIONS:
        while (mp->cap_i < (int)mp->caps.offset)
        {
            uint16_t m = pick_best(mp->caps.movelist, mp->cap_scores,
                                   mp->cap_i, mp->caps.offset);
            mp->cap_i++;
            if (m == mp->tt_move)
                continue;
            return m;
        }
        mp->stage = ST_DONE;
        return 0;

    case ST_DONE:
    default:
        return 0;
    }
}