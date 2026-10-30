#include "tt.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#if defined(__SIZEOF_INT128__)
#define HAVE_INT128 1
#endif

static TTCluster *tt = NULL;
static uint64_t tt_num_clusters = 0;
static size_t tt_mb = 0;

static uint64_t tt_cluster_index(uint64_t key)
{
#ifdef HAVE_INT128
    return (uint64_t)(((__uint128_t)key * (__uint128_t)tt_num_clusters) >> 64);
#else
    uint64_t a_lo = (uint32_t)key, a_hi = key >> 32;
    uint64_t b_lo = (uint32_t)tt_num_clusters, b_hi = tt_num_clusters >> 32;

    uint64_t t = a_lo * b_lo;
    uint64_t k = t >> 32;

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
    if (mb < TT_MIN_MB)
        mb = TT_MIN_MB;
    if (mb > TT_MAX_MB)
        mb = TT_MAX_MB;

    uint64_t bytes = (uint64_t)mb * 1024ULL * 1024ULL;
    uint64_t n = bytes / sizeof(TTCluster);
    if (n < 1)
        n = 1;

    TTCluster *new_tt = calloc(n, sizeof(TTCluster));
    if (!new_tt)
        return;

    free(tt);
    tt = new_tt;
    tt_num_clusters = n;
    tt_mb = mb;
}

void tt_resize(size_t mb) { tt_init(mb); }

void tt_free(void)
{
    free(tt);
    tt = NULL;
    tt_num_clusters = 0;
    tt_mb = 0;
}

size_t tt_size_mb(void) { return tt_mb; }

void tt_clear(void)
{
    if (!tt || !tt_num_clusters)
        return;
    memset(tt, 0, tt_num_clusters * sizeof(TTCluster));
}

void tt_store(uint64_t key, int score, uint16_t move, int depth,
              int flag, int is_qsearch, int eval)
{
    if (!tt || !tt_num_clusters)
        return;

    TTCluster *c = &tt[tt_cluster_index(key)];

    TTEntry *match = NULL;
    TTEntry *empty = NULL;
    TTEntry *shallow_qs = NULL;
    TTEntry *shallow_any = NULL;
    int min_d_qs = INT_MAX;
    int min_d_any = INT_MAX;

    for (int i = 0; i < TT_CLUSTER_SIZE; i++)
    {
        TTEntry *e = &c->entries[i];
        if (!e->valid)
        {
            if (!empty)
                empty = e;
            continue;
        }
        if (e->key == key)
        {
            match = e;
            break;
        }

        if (e->is_qsearch && e->depth < min_d_qs)
        {
            min_d_qs = e->depth;
            shallow_qs = e;
        }
        if (e->depth < min_d_any)
        {
            min_d_any = e->depth;
            shallow_any = e;
        }
    }

    if (match)
    {
        if (depth >= match->depth)
        {
            match->score = (int16_t)score;
            match->move = move;
            match->depth = (uint8_t)depth;
            match->flag = (uint8_t)flag;
            match->is_qsearch = (uint8_t)is_qsearch;
        }
        match->eval = (int16_t)eval;
        return;
    }

    TTEntry *slot = empty;
    if (!slot)
    {
        if (is_qsearch)
        {
            slot = shallow_qs;
            if (!slot)
                return;
        }
        else
        {
            slot = shallow_any;
        }
    }
    if (!slot)
        return;

    slot->key = key;
    slot->score = (int16_t)score;
    slot->move = move;
    slot->depth = (uint8_t)depth;
    slot->flag = (uint8_t)flag;
    slot->is_qsearch = (uint8_t)is_qsearch;
    slot->valid = 1;
    slot->eval = (int16_t)eval;
}

TTEntry *tt_probe(uint64_t key)
{
    if (!tt || !tt_num_clusters)
        return NULL;

    TTCluster *c = &tt[tt_cluster_index(key)];
    for (int i = 0; i < TT_CLUSTER_SIZE; i++)
    {
        TTEntry *e = &c->entries[i];
        if (e->valid && e->key == key)
            return e;
    }
    return NULL;
}