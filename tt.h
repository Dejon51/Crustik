#ifndef TT_H
#define TT_H

#include <stdint.h>
#include <stddef.h>

#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2

#define TT_MIN_MB     1
#define TT_MAX_MB     33554432
#define TT_DEFAULT_MB 16

typedef struct {
    uint64_t key;
    int16_t  score;
    int16_t  eval;
    uint16_t move;
    uint8_t  depth;
    uint8_t  flag;
    uint8_t  is_qsearch; 
    uint8_t  valid;
} TTEntry;

void     tt_init(size_t mb);
void     tt_resize(size_t mb);
void     tt_free(void);
void     tt_clear(void);
void     tt_store(uint64_t key, int score, uint16_t move, int depth, int flag, int is_qsearch, int eval);
TTEntry *tt_probe(uint64_t key);
size_t   tt_size_mb(void);

#endif