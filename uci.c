#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lmath.h"
#include "play.h"
#include "eval.h"
#include "fen.h"

void d(Position *board) // Displays board or something
{
    char piecelowercase[] = {
        'p',
        'b',
        'n',
        'r',
        'q',
        'k'};
    char pieceuppercase[] = {
        'P',
        'B',
        'N',
        'R',
        'Q',
        'K'};

    char board8x8[8][8] = {0};
    for (int sq = 0; sq < 64; sq++)
    {
        board8x8[sq / 8][sq % 8] = '.';
        for (int piece = 0; piece < 6; piece++)
        {
            if (is_set(board->pieces[piece], sq))
            {
                if (is_set(board->color[1], sq))
                {
                    board8x8[sq / 8][sq % 8] = piecelowercase[piece];
                }
                else if (is_set(board->color[0], sq))
                {
                    board8x8[sq / 8][sq % 8] = pieceuppercase[piece];
                }
                break;
            }
        }
    }
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {

            printf("%c ", board8x8[y][x]);
        }
        printf("\n");
    }
    printf("\n"); 
}

char uciStart(void)
{
    Position board = {0};
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
        char arg7[10] = {0};

        if (!fgets(line, sizeof(line), stdin))
            continue;

        int n = sscanf(line, "%14s %199s %9s %9s %9s %9s %9s %9s", arg, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
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
        else if (strcmp(arg, "position") == 0)
        {
            MoveList list = {0};

            if (strcmp(arg1, "startpos") == 0)
            {
                fenRead(&board, "8/8/8/3pP3/8/8/8/8", "w", "-", "d6", " 0", " 1");
            }
            else
            {
                fenRead(&board, arg1, arg2, arg3, arg4, arg5, arg6);
            }
            legalMoveGen(&board, &list, board.turn);
            d(&board);
            makeMove(&board, &list, 0);
            d(&board);
        }
        else if (strcmp(arg, "go") == 0)
        {
            if (strcmp(arg1, "perft") == 0)
            {
                fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1");
                struct timespec start, stop;
                #ifdef __linux__
                    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

                    uint64_t total_nodes = perft(&board, arg2[0] - '0');

                    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
                #elif __WIN32__
                    clock_gettime(CLOCK_MONOTONIC, &start);

                    uint64_t total_nodes = perft(&board, arg2[0] - '0');

                    clock_gettime(CLOCK_MONOTONIC, &stop);
                #endif
                long sec = stop.tv_sec - start.tv_sec;
                long nsec = stop.tv_nsec - start.tv_nsec;
                if (nsec < 0)
                {
                    sec -= 1;
                    nsec += 1000000000L;
                }

                long elapsed_ms = sec * 1000 + nsec / 1000000;

                double nps = total_nodes / (elapsed_ms / 1000.0);

                printf("Total Nodes: %llu\n", (unsigned long long)total_nodes);
                printf("Elapsed time: %ld ms\n", elapsed_ms);
                printf("N/S: %.0f\n", nps);
            }
        }
        else if (strcmp(arg, "d") == 0)
        {
            d(&board);
        }
        else
        {
            printf("Invalid arg: %s\n", arg);
        }
        fflush(stdout);
    }

    return 0;
}
