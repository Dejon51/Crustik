#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"

typedef struct
{
    int score;
    uint16_t move;
} searchOutput;

typedef struct {
    bool stop;
    uint64_t nodes;
    uint64_t start_time;
    uint64_t max_time;
    uint64_t max_nodes;
} stopConditions;

uint16_t iterative_deepening(Position *board, stopConditions *stop);

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop);


#endif