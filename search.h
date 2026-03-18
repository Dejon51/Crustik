#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"

typedef struct
{
    int16_t score;
    uint16_t move;
} searchOutput;

typedef struct {
    bool stop;
    uint64_t nodes;
    uint64_t start_time;
    uint64_t max_time;
    uint64_t max_nodes;
} stopConditions;

typedef struct {
    uint64_t key;
    int depth;
    int score;
    uint8_t flag; 
    uint16_t best_move;
} TTEntry;

uint16_t iterative_deepening(Position *board, stopConditions *stop);

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop);


#endif