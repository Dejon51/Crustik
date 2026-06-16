#include "tt.h"
#include <stdlib.h>
#include <string.h>

TTEntry *tt = NULL;
size_t tt_size = 0;

static inline size_t tt_index(uint64_t key) {
    return (size_t)(((__uint128_t)key * tt_size) >> 64);
}

int tt_init(size_t size) {
    if (size == 0) {
        return 0;
    }

    tt = malloc(sizeof(TTEntry) * size);
    if (!tt) {
        tt_size = 0;
        return 0;
    }

    tt_size = size;
    tt_clear();

    return 1;
}

void tt_free(void) {
    free(tt);
    tt = NULL;
    tt_size = 0;
}

void tt_clear(void) {
    if (tt && tt_size > 0) {
        memset(tt, 0, sizeof(TTEntry) * tt_size);
    }
}

void tt_store(uint64_t key, int score, uint16_t move, int depth, int flag) {
    if (!tt || tt_size == 0) return;

    TTEntry *e = &tt[tt_index(key)];

    e->key   = key;
    e->score = (int16_t)score;
    e->move  = move;
    e->depth = (uint8_t)depth;
    e->flag  = (uint8_t)flag;
}

TTEntry *tt_probe(uint64_t key) {
    if (!tt || tt_size == 0) return NULL;

    TTEntry *e = &tt[tt_index(key)];

    return (e->key == key) ? e : NULL;
}