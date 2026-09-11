#ifndef SEARCH_PARAMS_H
#define SEARCH_PARAMS_H

extern int RFP_MAX_DEPTH;
extern int RFP_MARGIN;

extern int NMP_MIN_DEPTH;
extern int NMP_BASE;
extern int NMP_DIVISOR;
extern int NMP_EVAL_THRESHOLD;
extern int NMP_EVAL_BONUS;

extern int LMP_MAX_DEPTH;
extern int LMP_BASE_NOT_IMPROVING;
extern int LMP_BASE_IMPROVING;

extern int SEE_MAX_DEPTH;
extern int SEE_CAPTURE_MULT;
extern int SEE_QUIET_MULT;

extern int HP_MAX_DEPTH;
extern int HP_MIN_MOVE_INDEX;
extern int HP_MULT;

extern int FP_MAX_DEPTH;
extern int FP_BASE;
extern int FP_MULT;

extern int SE_MIN_DEPTH;
extern int SE_TT_DEPTH_MARGIN;
extern int SE_BETA_MULT;

extern int LMR_DIVISOR_X100;
extern int LMR_MAX_REDUCTION;
extern int LMR_HIST_THRESHOLD;

extern int HIST_MALUS_MULT;
extern int HIST_MALUS_BASE;
extern int HIST_BONUS_MULT;
extern int HIST_BONUS_BASE;

extern int ASP_DELTA;
extern int ASP_MAX_DELTA;

extern int QS_DELTA_MARGIN;

typedef struct
{
    const char *name;
    int *value;
    int default_value;
    int min;
    int max;
} SearchParam;

extern SearchParam search_params[];
extern const int search_param_count;

void search_params_init(void);
void search_params_print_uci_options(void);
void search_params_print_spsa(void);
int search_params_set(const char *name, int value);

#endif