#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lmath.h"
#include "play.h"
#include "eval.h"
#include "fen.h"
#include "search.h"

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
            if ((board->pieces[piece] >> sq) & 1)
            {
                if ((board->color[1] >> sq) & 1)
                {
                    board8x8[sq / 8][sq % 8] = piecelowercase[piece];
                }
                else if ((board->color[0] >> sq) & 1)
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
    Position copyboard = {0};

    char run = 1;
    char uciok = 0;

    fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1");

    char line[20000];

    char *tokens[64];
    while (run)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("Bye\n");
            return 1;
        }
        line[strcspn(line, "\n")] = '\0';

        int t = 0;
        for (char *tok = strtok(line, " \n"); tok != NULL && t < 63; tok = strtok(NULL, " \n"))
            tokens[t++] = tok;
        tokens[t] = NULL;

        if (tokens[0] == NULL)
            continue;

        if (strcmp(tokens[0], "quit") == 0)
        {
            printf("bye\n");
            break;
        }
        else if (strcmp(tokens[0], "uci") == 0)
        {
            printf("uciok\n");
            uciok = 1;
        }
        else if (strcmp(tokens[0], "isready") == 0)
        {
            printf("readyok\n");
        }
        else if (strcmp(tokens[0], "position") == 0)
        {
            if (tokens[1] == NULL)
            {
                printf("position: missing argument\n");
            }
            else if (strcmp(tokens[1], "startpos") == 0)
            {
                fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1");
                d(&board);
            }
            else
            {
                fenRead(&board, tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],tokens[6]);
                d(&board);
            }
        }
        else if (strcmp(tokens[0], "go") == 0)
        {
            if (tokens[1] == NULL)
            {
                printf("go: missing argument\n");
            }
            else if (strcmp(tokens[1], "depth") == 0)
            {
                if (tokens[2] == NULL)
                {
                    printf("go depth: missing depth value\n");
                }
                else
                {
                    for (int i = 0; i < 120; i++)
                    {
                        searchOutput result = search(&board, tokens[2][0] - '0', 0, -32000, 32000);

                        if (result.move == 0)
                        {
                            uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                            if (!king_bb)
                                break;
                            int king_pos = __builtin_ctzll(king_bb);
                            if (squareAttacked(&board, king_pos, !board.turn))
                                printf("%s is checkmated\n", board.turn ? "Black" : "White");
                            else
                                printf("Stalemate\n");
                            break;
                        }

                        moveint(&board, result.move);
                        d(&board);
                    }
                }
            }
            else if (strcmp(tokens[1], "perft") == 0)
            {
                if (tokens[2] == NULL)
                {
                    printf("go perft: missing depth value\n");
                }
                else
                {
                    int depth = 0;
                    for (int i = 0; tokens[2][i] != '\0'; i++)
                        depth = depth * 10 + (tokens[2][i] - '0');

                    struct timespec start, stop;
#ifdef __linux__
                    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                    uint64_t total_nodes = perft(&board, depth);
                    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
#else
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    uint64_t total_nodes = perft(&board, depth);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
#endif
                    long sec = stop.tv_sec - start.tv_sec;
                    long nsec = stop.tv_nsec - start.tv_nsec;
                    if (nsec < 0) { sec -= 1; nsec += 1000000000L; }

                    long elapsed_ms = sec * 1000 + nsec / 1000000;
                    double nps = total_nodes / (elapsed_ms / 1000.0);

                    printf("Total Nodes: %llu\n", (unsigned long long)total_nodes);
                    printf("Elapsed time: %ld ms\n", elapsed_ms);
                    printf("N/S: %.0f\n", nps);
                }
            }
            else
            {
                printf("go: unknown argument: %s\n", tokens[1]);
            }
        }
        else if (strcmp(tokens[0], "d") == 0)
        {
            d(&board);
        }
        else if (strcmp(tokens[0], "pml") == 0)
        {
            if (tokens[1] == NULL)
            {
                printf("pml: missing move index\n");
            }
            else
            {
                copyboard = board;
                MoveList move_list = {0};
                legalMoveGen(&copyboard, &move_list);
                printf("%i\n", move_list.offset);
                int result = 0;
                for (int i = 0; tokens[1][i] != '\0'; i++)
                    result = result * 10 + (tokens[1][i] - '0');
                makeMove(&copyboard, &move_list, result);
                d(&copyboard);
            }
        }
        else
        {
            printf("Invalid arg: %s\n", tokens[0]);
        }
        fflush(stdout);
    }
}