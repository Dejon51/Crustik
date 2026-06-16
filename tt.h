#ifndef TT_H
#define TT_H

#include <stdint.h>

#define TT_SIZE (1 << 22)
#define TT_MASK (TT_SIZE - 1)

#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2

typedef struct {
    uint64_t key;
    int16_t  score;
    uint16_t move;
    uint8_t  depth;
    uint8_t  flag;
} TTEntry;

/* malloc-based transposition table */
extern TTEntry *tt;

int tt_init(void);
void tt_free(void);

void tt_clear(void);
void tt_store(uint64_t key, int score, uint16_t move, int depth, int flag);
TTEntry *tt_probe(uint64_t key);

#endif