#include <stdio.h>
#include "lmath.h"

char uciStart(void)
{
    char run = 1;
    char uciok = 0;

    while (run)
    {
        char line[250] = {0};
        char command[15] = {0};
        char secondcommand[200] = {0};

        if (!fgets(line, sizeof(line), stdin))
            continue;

        int n = sscanf(line, "%14s %199s", command,secondcommand);
        if (n <= 0)
            continue;

        if (mstrcmp(command, "quit") == 0)
        {
            printf("bye\n");
            fflush(stdout);
            break;
        }

        if (uciok == 0)
        {
            if (mstrcmp(command, "uci") == 0)
            {
                printf("uciok\n");
                uciok = 1;
            }
            else
            {
                printf("Invalid Command: %s\n", command);
            }
            fflush(stdout);
            continue;
        }

        if (uciok == 1)
        {
            if (mstrcmp(command, "isready") == 0)
            {
                printf("readyok\n");
            }
            else
            {
                printf("Invalid Command: %s\n", command);
            }
            fflush(stdout);
        }
    }

    return 0;
}
