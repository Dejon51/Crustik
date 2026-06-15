#ifndef TT_H
#define TT_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    TT_NONE = 0,
    TT_EXACT = 1,
    TT_LOWERBOUND = 2,
    TT_UPPERBOUND = 3
} TTFlag;

typedef struct {
    uint64_t hash;
    uint16_t move;
    int16_t score;
    uint16_t depth;
    uint8_t flag;
} TTEntry;

extern size_t TTSizeMB;

int initializeTT(size_t entries);
int initializeTTFromMB(size_t mb);
int resizeTTFromMB(size_t mb);
int setTTSizeMB(size_t mb);

void storeTTEntry(uint64_t hash,int16_t score,uint16_t move,uint16_t depth,TTFlag flag);

TTEntry *probeTT(uint64_t hash);

void clearTT(void);
void freeTT(void);

size_t getTTSize(void);
size_t getTTSizeMB(void);

#endif