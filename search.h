#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"

#define MAX_PV_LENGTH 200

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
    uint8_t depth;
    int seldepth;
    int print_info; 
} stopConditions;

typedef struct {
    uint16_t moves[MAX_PV_LENGTH];
    int length;
} PVLine;

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop, PVLine *);

uint16_t iterative_deepening(Position *board, stopConditions *stop);

#endif