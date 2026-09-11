#include "search_params.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int RFP_MAX_DEPTH = 6;
int RFP_MARGIN = 100;

int NMP_MIN_DEPTH = 3;
int NMP_BASE = 3;
int NMP_DIVISOR = 6;
int NMP_EVAL_THRESHOLD = 300;
int NMP_EVAL_BONUS = 1;

int LMP_MAX_DEPTH = 3;
int LMP_BASE_NOT_IMPROVING = 16;
int LMP_BASE_IMPROVING = 24;

int SEE_MAX_DEPTH = 8;
int SEE_CAPTURE_MULT = 90;
int SEE_QUIET_MULT = 50;

int HP_MAX_DEPTH = 3;
int HP_MIN_MOVE_INDEX = 4;
int HP_MULT = 4000;

int FP_MAX_DEPTH = 3;
int FP_BASE = 120;
int FP_MULT = 90;

int SE_MIN_DEPTH = 8;
int SE_TT_DEPTH_MARGIN = 3;
int SE_BETA_MULT = 2;

int LMR_DIVISOR_X100 = 200;
int LMR_MAX_REDUCTION = 8;
int LMR_HIST_THRESHOLD = 4000;

int HIST_MALUS_MULT = 160;
int HIST_MALUS_BASE = 200;
int HIST_BONUS_MULT = 320;
int HIST_BONUS_BASE = 400;

int ASP_DELTA = 25;
int ASP_MAX_DELTA = 500;

int QS_DELTA_MARGIN = 200;

SearchParam search_params[] = {
    {"RFPMaxDepth", &RFP_MAX_DEPTH, 6, 1, 12},
    {"RFPMargin", &RFP_MARGIN, 100, 10, 300},

    {"NMPMinDepth", &NMP_MIN_DEPTH, 3, 1, 8},
    {"NMPBase", &NMP_BASE, 3, 1, 6},
    {"NMPDivisor", &NMP_DIVISOR, 6, 1, 12},
    {"NMPEvalThreshold", &NMP_EVAL_THRESHOLD, 300, 50, 600},
    {"NMPEvalBonus", &NMP_EVAL_BONUS, 1, 0, 3},

    {"LMPMaxDepth", &LMP_MAX_DEPTH, 3, 1, 8},
    {"LMPBaseNotImproving", &LMP_BASE_NOT_IMPROVING, 16, 4, 40},
    {"LMPBaseImproving", &LMP_BASE_IMPROVING, 24, 8, 60},

    {"SEEMaxDepth", &SEE_MAX_DEPTH, 8, 1, 16},
    {"SEECaptureMult", &SEE_CAPTURE_MULT, 90, 20, 200},
    {"SEEQuietMult", &SEE_QUIET_MULT, 50, 10, 150},

    {"HPMaxDepth", &HP_MAX_DEPTH, 3, 1, 8},
    {"HPMinMoveIndex", &HP_MIN_MOVE_INDEX, 4, 1, 12},
    {"HPMult", &HP_MULT, 4000, 500, 10000},

    {"FPMaxDepth", &FP_MAX_DEPTH, 3, 1, 8},
    {"FPBase", &FP_BASE, 120, 20, 400},
    {"FPMult", &FP_MULT, 90, 20, 300},

    {"SEMinDepth", &SE_MIN_DEPTH, 8, 4, 16},
    {"SETTDepthMargin", &SE_TT_DEPTH_MARGIN, 3, 1, 8},
    {"SEBetaMult", &SE_BETA_MULT, 2, 1, 6},

    {"LMRDivisorX100", &LMR_DIVISOR_X100, 200, 100, 400},
    {"LMRMaxReduction", &LMR_MAX_REDUCTION, 8, 3, 16},
    {"LMRHistThreshold", &LMR_HIST_THRESHOLD, 4000, 500, 10000},

    {"HistMalusMult", &HIST_MALUS_MULT, 160, 20, 400},
    {"HistMalusBase", &HIST_MALUS_BASE, 200, 20, 600},
    {"HistBonusMult", &HIST_BONUS_MULT, 320, 40, 800},
    {"HistBonusBase", &HIST_BONUS_BASE, 400, 40, 1000},

    {"AspDelta", &ASP_DELTA, 25, 5, 100},
    {"AspMaxDelta", &ASP_MAX_DELTA, 500, 100, 1200},

    {"QSDeltaMargin", &QS_DELTA_MARGIN, 200, 50, 500},
};

const int search_param_count = sizeof(search_params) / sizeof(search_params[0]);

void search_params_init(void)
{
    for (int i = 0; i < search_param_count; i++)
        *(search_params[i].value) = search_params[i].default_value;
}

void search_params_print_uci_options(void)
{
    for (int i = 0; i < search_param_count; i++)
    {
        printf("option name %s type spin default %d min %d max %d\n",
               search_params[i].name,
               search_params[i].default_value,
               search_params[i].min,
               search_params[i].max);
    }
}

void search_params_print_spsa(void)
{
    for (int i = 0; i < search_param_count; i++)
    {
        double range = (double)(search_params[i].max - search_params[i].min);
        double c_end = range / 20.0;
        if (c_end < 0.5)
            c_end = 0.5;

        printf("%s, int, %d, %d, %d, %.2f, 0.002\n",
               search_params[i].name,
               search_params[i].default_value,
               search_params[i].min,
               search_params[i].max,
               c_end);
    }
}

static int str_eq_ci_local(const char *a, const char *b)
{
    while (*a && *b)
    {
        unsigned char ca = (unsigned char)tolower((unsigned char)*a);
        unsigned char cb = (unsigned char)tolower((unsigned char)*b);
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int search_params_set(const char *name, int value)
{
    for (int i = 0; i < search_param_count; i++)
    {
        if (str_eq_ci_local(search_params[i].name, name))
        {
            if (value < search_params[i].min)
                value = search_params[i].min;
            if (value > search_params[i].max)
                value = search_params[i].max;

            *(search_params[i].value) = value;
            return 1;
        }
    }
    return 0;
}