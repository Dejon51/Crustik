#include <stdio.h>
#include <string.h>
#include "lmath.h"
#include "play.h"
#include "eval.h"
#include "fen.h"

char uciStart(void)
{
    char run = 1;
    char uciok = 0;

    while (run)
    {
        char line[270] = {0};
        char arg[15] = {0};
        char arg1[200] = {0};
        char arg2[10] = {0};
        char arg3[10] = {0};
        char arg4[10] = {0};
        char arg5[10] = {0};
        char arg6[10] = {0};


        if (!fgets(line, sizeof(line), stdin))
            continue;

        int n = sscanf(line, "%14s %199s %9s %9s %9s %9s %9s", arg, arg1    ,arg2,arg3,arg4,arg5,arg6);
        if (n <= 0)
            continue;

        if (strcmp(arg, "quit") == 0)
        {
            printf("bye\n");
            break;
        }
        else if (strcmp(arg, "uci") == 0)
        {
            printf("uciok\n");
            uciok = 1;
        }
        else if (strcmp(arg, "isready") == 0)
        {
            printf("readyok\n");
        }
        else if (strcmp(arg, "pos") == 0)
        {
            fenRead(arg1,arg2,arg3,arg4,arg5,arg6);
            uciok = 1;
        }
        else
        {
            printf("Invalid arg: %s\n", arg);
        }
        fflush(stdout);
    }

    return 0;
}
