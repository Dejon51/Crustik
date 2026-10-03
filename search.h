#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"

#define MAX_PV_LENGTH 200

#define MATE_SCORE 32000
#define MAX_DEPTH 200
#define MAX_GAME_PLY 2048
#define MAX_LMR_MOVES 50

#define NO_EVAL (-32001)

#define MAX_SEARCH_PLY 128

#define CORRHIST_SIZE 16384
#define CORRHIST_MASK (CORRHIST_SIZE - 1)

extern int butterfly_hist[2][64][64];
extern uint16_t killer_moves[MAX_GAME_PLY][2];
extern int eval_stack[MAX_GAME_PLY];

extern int cont_hist[2][6][64][6][64];
extern int capture_history[2][6][64][6];

typedef struct
{
    int piece;
    int to;
    bool valid;
} ContRecord;

extern ContRecord cont_stack[MAX_SEARCH_PLY];

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
    uint64_t soft_nodes;
    int64_t soft_time;
    uint8_t depth;
    int seldepth;
    int print_info; 
} stopConditions;

typedef struct {
    uint16_t moves[MAX_PV_LENGTH];
    int length;
} PVLine;

typedef struct {
    uint16_t excluded_move;
} SearchStack;


void init_lmr();

void reset_history(void);

searchOutput search(Position *board, int depth, int ply, int alpha, int beta, stopConditions *stop, PVLine *pv, SearchStack *stack);

uint16_t iterative_deepening(Position *board, stopConditions *stop);

#endif