#include "tt.h"
#include <string.h>
#include <stdlib.h>

TTEntry *tt = NULL;

int tt_init(void) {
    tt = malloc(sizeof(TTEntry) * TT_SIZE);
    if (!tt) {
        return 0;
    }

    tt_clear();
    return 1;
}

void tt_free(void) {
    free(tt);
    tt = NULL;
}

void tt_clear(void) {
    if (tt) {
        memset(tt, 0, sizeof(TTEntry) * TT_SIZE);
    }
}

void tt_store(uint64_t key, int score, uint16_t move, int depth, int flag) {
    if (!tt) return;

    TTEntry *e = &tt[key & TT_MASK];

    e->key   = key;
    e->score = (int16_t)score;
    e->move  = move;
    e->depth = (uint8_t)depth;
    e->flag  = (uint8_t)flag;
}

TTEntry *tt_probe(uint64_t key) {
    if (!tt) return NULL;

    TTEntry *e = &tt[key & TT_MASK];
    return (e->key == key) ? e : NULL;
}