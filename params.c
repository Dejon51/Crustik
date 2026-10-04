#include "params.h"

/* ---------- LMR ---------- */
int LMR_MIN_DEPTH = 3;
int LMR_MIN_MOVES = 4;
double LMR_DIVISOR = 2.0;
int LMR_MAX = 8;
int LMR_PV_NODE_SCALAR = 1024;
int LMR_BFHIST_BONUS_SCALAR = 1024;
int LMR_BFHIST_PENALTY_SCALAR = 1024;
int LMR_BFHIST_THRESHOLD_BONUS_SCALAR = 4000;
int LMR_BFHIST_THRESHOLD_PENALTY_SCALAR = 4000;

/* ---------- PVS ---------- */
int PVS_MIN_DEPTH = 3;

/* ---------- Reverse futility pruning ---------- */
int RFP_MAX_DEPTH = 6;
int RFP_MARGIN = 100;

/* ---------- Null move pruning ---------- */ 
int NMP_MIN_DEPTH = 3;
int NMP_BASE_R = 3;
int NMP_DEPTH_DIV = 6;
int NMP_EVAL_MARGIN = 300;

/* ---------- ProbCut ---------- */
int PROBCUT_MIN_DEPTH = 5;
int PROBCUT_MARGIN = 150;
int PROBCUT_DEPTH_REDUCTION = 4;
int PROBCUT_SEE_THRESHOLD = 100;
int PROBCUT_TT_DEPTH_MARGIN = 3;

/* ---------- Internal iterative reduction ---------- */
int IIR_MIN_DEPTH = 4;

/* ---------- Late move pruning ---------- */
int LMP_MAX_DEPTH = 3;
int LMP_IMPROVING_COUNT = 24;
int LMP_NONIMPROVING_COUNT = 16;

/* ---------- SEE pruning ---------- */
int SEE_PRUNE_MAX_DEPTH = 8;
int SEE_PRUNE_CAPTURE_MARGIN = 90;
int SEE_PRUNE_QUIET_MARGIN = 50;

/* ---------- History pruning ---------- */
int HISTPRUNE_MAX_DEPTH = 3;
int HISTPRUNE_MIN_MOVES = 4;
int HISTPRUNE_MARGIN = 6000;

/* ---------- Futility pruning ---------- */
int FUTILITY_MAX_DEPTH = 3;
int FUTILITY_BASE = 120;
int FUTILITY_DEPTH_MARGIN = 90;

/* ---------- Singular extensions ---------- */
int SE_MIN_DEPTH = 8;
int SE_TT_DEPTH_MARGIN = 3;
int SE_BETA_MARGIN = 2;
int SE_DEPTH_SUB = 1;
int SE_DEPTH_DIV = 2;
int SE_EXTENSION = 1;
int SE_NEG_EXTENSION = 1;

/* ---------- History updates ---------- */
int HIST_MAX = 16384;
int HIST_BONUS_MULT = 320;
int HIST_BONUS_BASE = 400;
int HIST_MALUS_MULT = 160;
int HIST_MALUS_BASE = 200;

/* ---------- Correction history ---------- */
int CORRHIST_GRAIN = 256;
int CORRHIST_LIMIT = 16384;
int CORRHIST_MAX_APPLY = 128;

/* ---------- Move ordering ---------- */
int MVV_LVA_VICTIM_MULT = 10;

/* ---------- Piece values (MVV-LVA / qsearch gain) ---------- */
int PIECE_VALUE_PAWN = 100;
int PIECE_VALUE_KNIGHT = 330;
int PIECE_VALUE_BISHOP = 320;
int PIECE_VALUE_ROOK = 500;
int PIECE_VALUE_QUEEN = 900;
int PIECE_VALUE_KING = 20000;

/* ---------- Quiescence ---------- */
int QS_FUTILITY_MARGIN = 200;
int QS_FUTILITY_SEE_THRESHOLD = 1;
int QS_SEE_THRESHOLD = 0;

/* ---------- Aspiration windows ---------- */
int ASP_MIN_DEPTH = 2;
int ASP_DELTA = 25;
int ASP_MAX_DELTA = 500;
double ASP_DELTA_GROWTH = 2.0;
int ASP_REDUCTION_MAX = 3;
int ASP_MAX_RESEARCH = 5;

/* ---------- Time management ---------- */
double TM_BM_INST_SCALE = 2.20;
double TM_BM_DECAY = 0.5;
double TM_SCORE_SWING_SCALE = 25.0;
int TM_SCORE_DROP_DEPTH = 7;
double TM_STABLE_BASE = 1.2;
double TM_STABLE_STEP = 0.05;
double TM_STABLE_MIN = 0.8;
double TM_STABLE_MAX = 1.2;