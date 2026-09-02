#include "tt.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(__SIZEOF_INT128__)
#define HAVE_INT128 1
#endif

static TTEntry *tt             = NULL;
static uint64_t tt_num_entries = 0;
static size_t   tt_mb          = 0;

static uint64_t entries_for_mb(size_t mb)
{
    uint64_t bytes      = (uint64_t)mb * 1024ULL * 1024ULL;
    uint64_t entry_size = (uint64_t)sizeof(TTEntry);
    uint64_t n = bytes / entry_size;
    return n < 1 ? 1 : n;
}

static uint64_t tt_index(uint64_t key)
{
#ifdef HAVE_INT128
    return (uint64_t)(((__uint128_t)key * (__uint128_t)tt_num_entries) >> 64);
#else
    uint64_t a_lo = (uint32_t)key,        a_hi = key >> 32;
    uint64_t b_lo = (uint32_t)tt_num_entries, b_hi = tt_num_entries >> 32;

    uint64_t t  = a_lo * b_lo;
    uint64_t k  = t >> 32;

    t = a_hi * b_lo + k;
    uint64_t w1 = (uint32_t)t;
    uint64_t w2 = t >> 32;

    t = a_lo * b_hi + w1;
    k = t >> 32;

    return a_hi * b_hi + w2 + k;
#endif
}

void tt_init(size_t mb)
{
    if (mb < TT_MIN_MB) mb = TT_MIN_MB;
    if (mb > TT_MAX_MB) mb = TT_MAX_MB;

    uint64_t n = entries_for_mb(mb);
    TTEntry *new_tt = calloc(n, sizeof(TTEntry));
    if (!new_tt)
        return; 

    free(tt);
    tt = new_tt;
    tt_num_entries = n;
    tt_mb = mb;
}

void tt_resize(size_t mb) { tt_init(mb); }

void tt_free(void)
{
    free(tt);
    tt = NULL;
    tt_num_entries = 0;
    tt_mb = 0;
}

size_t tt_size_mb(void) { return tt_mb; }

void tt_clear(void)
{
    if (!tt || !tt_num_entries) return;
    memset(tt, 0, tt_num_entries * sizeof(TTEntry));
}

void tt_store(uint64_t key, int score, uint16_t move, int depth, int flag, int is_qsearch, int eval)
{
    if (!tt || !tt_num_entries) return;

    TTEntry *e = &tt[tt_index(key)];

    if (e->valid && e->key != key && is_qsearch && !e->is_qsearch)
        return;

    bool same_key = e->valid && e->key == key;

    if (!e->valid || !same_key || depth >= e->depth)
    {
        e->key        = key;
        e->score      = (int16_t)score;
        e->move       = move;
        e->depth      = (uint8_t)depth;
        e->flag       = (uint8_t)flag;
        e->is_qsearch = (uint8_t)is_qsearch;
        e->valid      = 1;
        e->eval       = (int16_t)eval;
    }
    else if (same_key)
    {
        e->eval = (int16_t)eval;
    }
}

TTEntry *tt_probe(uint64_t key)
{
    if (!tt || !tt_num_entries) return NULL;

    TTEntry *e = &tt[tt_index(key)];
    return (e->valid && e->key == key) ? e : NULL;
}