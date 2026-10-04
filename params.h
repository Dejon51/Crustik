#ifndef PARAMS_H
#define PARAMS_H

/* ---------- LMR ---------- */
extern int LMR_MIN_DEPTH;
extern int LMR_MIN_MOVES;
extern double LMR_DIVISOR;
extern int LMR_MAX;
extern int LMR_PV_NODE_SCALAR;
extern int LMR_BFHIST_BONUS_SCALAR;
extern int LMR_BFHIST_PENALTY_SCALAR;
extern int LMR_BFHIST_THRESHOLD_BONUS_SCALAR;
extern int LMR_BFHIST_THRESHOLD_PENALTY_SCALAR;

/* ---------- PVS ---------- */
extern int PVS_MIN_DEPTH;

/* ---------- Reverse futility pruning ---------- */
extern int RFP_MAX_DEPTH;
extern int RFP_MARGIN;
extern int RFP_CORRPLEXITY_MULT;
extern int RFP_CORRPLEXITY_DIVISOR;

/* ---------- Null move pruning ---------- */
extern int NMP_MIN_DEPTH;
extern int NMP_BASE_R;
extern int NMP_DEPTH_DIV;
extern int NMP_EVAL_MARGIN;

/* ---------- ProbCut ---------- */
extern int PROBCUT_MIN_DEPTH;
extern int PROBCUT_MARGIN;
extern int PROBCUT_DEPTH_REDUCTION;
extern int PROBCUT_SEE_THRESHOLD;
extern int PROBCUT_TT_DEPTH_MARGIN;

/* ---------- Internal iterative reduction ---------- */
extern int IIR_MIN_DEPTH;

/* ---------- Late move pruning ---------- */
extern int LMP_MAX_DEPTH;
extern int LMP_IMPROVING_COUNT;
extern int LMP_NONIMPROVING_COUNT;

/* ---------- SEE pruning ---------- */
extern int SEE_PRUNE_MAX_DEPTH;
extern int SEE_PRUNE_CAPTURE_MARGIN;
extern int SEE_PRUNE_QUIET_MARGIN;

/* ---------- History pruning ---------- */
extern int HISTPRUNE_MAX_DEPTH;
extern int HISTPRUNE_MIN_MOVES;
extern int HISTPRUNE_MARGIN;

/* ---------- Futility pruning ---------- */
extern int FUTILITY_MAX_DEPTH;
extern int FUTILITY_BASE;
extern int FUTILITY_DEPTH_MARGIN;

/* ---------- Singular extensions ---------- */
extern int SE_MIN_DEPTH;
extern int SE_TT_DEPTH_MARGIN;
extern int SE_BETA_MARGIN;
extern int SE_DEPTH_SUB;
extern int SE_DEPTH_DIV;
extern int SE_EXTENSION;
extern int SE_NEG_EXTENSION;

/* ---------- History updates ---------- */
extern int HIST_MAX;
extern int HIST_BONUS_MULT;
extern int HIST_BONUS_BASE;
extern int HIST_MALUS_MULT;
extern int HIST_MALUS_BASE;

/* ---------- Correction history ---------- */
extern int CORRHIST_GRAIN;
extern int CORRHIST_LIMIT;
extern int CORRHIST_MAX_APPLY;

/* ---------- Move ordering ---------- */
extern int MVV_LVA_VICTIM_MULT;

/* ---------- Piece values (MVV-LVA / qsearch gain) ---------- */
extern int PIECE_VALUE_PAWN;
extern int PIECE_VALUE_KNIGHT;
extern int PIECE_VALUE_BISHOP;
extern int PIECE_VALUE_ROOK;
extern int PIECE_VALUE_QUEEN;
extern int PIECE_VALUE_KING;

/* ---------- Quiescence ---------- */
extern int QS_FUTILITY_MARGIN;
extern int QS_FUTILITY_SEE_THRESHOLD;
extern int QS_SEE_THRESHOLD;

/* ---------- Aspiration windows ---------- */
extern int ASP_MIN_DEPTH;
extern int ASP_DELTA;
extern int ASP_MAX_DELTA;
extern double ASP_DELTA_GROWTH;
extern int ASP_REDUCTION_MAX;
extern int ASP_MAX_RESEARCH;

/* ---------- Time management ---------- */
extern double TM_BM_INST_SCALE;
extern double TM_BM_DECAY;
extern double TM_SCORE_SWING_SCALE;
extern int TM_SCORE_DROP_DEPTH;
extern double TM_STABLE_BASE;
extern double TM_STABLE_STEP;
extern double TM_STABLE_MIN;
extern double TM_STABLE_MAX;

#endif