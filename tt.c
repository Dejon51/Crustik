#include "tt.h"
#include <string.h>

TTEntry tt[TT_SIZE];

void tt_clear(void) {
    memset(tt, 0, sizeof(tt));
}

void tt_store(uint64_t key, int score, uint16_t move, int depth, int flag) {
    TTEntry *e = &tt[key & TT_MASK];
    e->key   = key;
    e->score = (int16_t)score;
    e->move  = move;
    e->depth = (uint8_t)depth;
    e->flag  = (uint8_t)flag;
}

TTEntry *tt_probe(uint64_t key) {
    TTEntry *e = &tt[key & TT_MASK];
    return (e->key == key) ? e : NULL;
}