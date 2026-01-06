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
        char line[250] = {0};
        char command[15] = {0};
        char secondarugment[200] = {0};
        char thirdarugment[200] = {0};

        if (!fgets(line, sizeof(line), stdin))
            continue;

        int n = sscanf(line, "%14s %199s %199s", command, secondarugment, thirdarugment);
        if (n <= 0)
            continue;

        if (strcmp(command, "quit") == 0)
        {
            printf("bye\n");
            break;
        }
        else if (strcmp(command, "uci") == 0)
        {
            printf("uciok\n");
            uciok = 1;
        }
        else if (strcmp(command, "isready") == 0)
        {
            printf("readyok\n");
        }
        else if (strcmp(command, "pos") == 0)
        {
            fenRead(secondarugment);
            uciok = 1;
        }
        else
        {
            printf("Invalid Command: %s\n", command);
        }
        fflush(stdout);
    }

    return 0;
}
