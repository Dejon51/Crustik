#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"


typedef struct
{
    int score;
    uint16_t move;
} searchOutput;

searchOutput search(Position *board, int depth,int ply,int alpha,int beta);

#endif