#ifndef SEARCH_H
#define SEARCH_H

#include "lmath.h"
#include "eval.h"

#define MAX_PV_LENGTH 200

#define MATE_SCORE 32000
#define MAX_DEPTH 200
#define MAX_GAME_PLY 2048
#define MAX_LMR_MOVES 50

#define NO_EVAL (-32001)

#define MAX_SEARCH_PLY 128

#define CORRHIST_SIZE 16384
#define CORRHIST_MASK (CORRHIST_SIZE - 1)

#define MAX_THREADS 256
#define DEFAULT_THREADS 1

typedef struct ThreadPool ThreadPool;

typedef struct
{
    int piece;
    int to;
    bool valid;
} ContRecord;

typedef struct
{
    int16_t score;
    uint16_t move;
} searchOutput;

#define MAX_SEARCHMOVES 256

typedef struct
{
    long long start_time;
    int depth;
    int soft_time;
    int max_time;
    uint64_t soft_nodes;
    uint64_t max_nodes;
    uint64_t nodes;
    int seldepth;
    volatile int stop;
    volatile int print_info;

    uint16_t searchmoves[MAX_SEARCHMOVES];
    int num_searchmoves;  
    int mate_target;      
} stopConditions;

typedef struct
{
    uint16_t moves[MAX_PV_LENGTH];
    int length;
} PVLine;

typedef struct
{
    uint16_t excluded_move;
} SearchStack;

struct ThreadData
{
    int thread_id;
    ThreadPool *pool;

    stopConditions stop;

    int butterfly_hist[2][64][64];
    int eval_stack[MAX_GAME_PLY];
    int cont_hist[2][6][64][6][64];
    int capture_history[2][6][64][6];
    uint16_t counter_moves[2][6][64];
    int pawn_corrhist[2][CORRHIST_SIZE];
    int nonpawn_corrhist[2][CORRHIST_SIZE];

    ContRecord cont_stack[MAX_SEARCH_PLY];
    uint64_t search_path_hash[MAX_SEARCH_PLY];

    NnueAccumulator nnue_stack[NNUE_MAX_PLY];

    PVLine pv;
    uint16_t best_move;
    int best_score;
    int completed_depth;
};

struct ThreadPool
{
    int count;
    volatile int stop_flag; 
    ThreadData *threads[MAX_THREADS];
};

void init_lmr();

void reset_history(void);

void search_set_threads(int n);
int search_get_threads(void);

searchOutput search(ThreadData *td, Position *board, int depth, int ply,
                    int alpha, int beta, PVLine *pv, SearchStack *stack);

searchOutput search_bench(Position *board, int depth, stopConditions *stop);

uint16_t iterative_deepening(Position *board, stopConditions *stop);

void search_request_stop(void);
void search_start_async(const Position *board, const stopConditions *stop);
void search_wait(void);
bool search_is_running(void);

#endif