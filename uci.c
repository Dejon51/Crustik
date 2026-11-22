#include "fen.h"
#include "lmath.h"
#include "play.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>

void movestring(uint16_t move)
{
    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int flag = (move >> 12) & 0xF;

    int x1 = from & 7;
    int y1 = 8 - (from >> 3);
    int x2 = to & 7;
    int y2 = 8 - (to >> 3);

    char promotion = 0;

    switch (flag)
    {
    case 5: promotion = 'b'; break;
    case 6: promotion = 'n'; break;
    case 7: promotion = 'r'; break;
    case 8: promotion = 'q'; break;
    }

    if (promotion)
        printf("bestmove %c%d%c%d%c\n", 'a' + x1, y1, 'a' + x2, y2, promotion);
    else
        printf("bestmove %c%d%c%d\n", 'a' + x1, y1, 'a' + x2, y2);
}

uint16_t parsemove(Position *board, char *move)
{
    int x1 = move[0] - 'a';
    int y1 = move[1] - '1';
    int x2 = move[2] - 'a';
    int y2 = move[3] - '1';
    int flag = 0;

    switch (move[4])
    {
    case 'b': flag = 5U; break;
    case 'n': flag = 6U; break;
    case 'r': flag = 7U; break;
    case 'q': flag = 8U; break;
    default: break;
    }

    int from = x1 + ((7 - y1) << 3);
    int to = x2 + ((7 - y2) << 3);

    if (board->mailbox[from] == 5)
    {
        if (board->turn == 0 && from == E1)
        {
            if (to == G1) flag = 1U;
            else if (to == C1) flag = 2U;
        }
        else if (board->turn == 1 && from == E8)
        {
            if (to == G8) flag = 4U;
            else if (to == C8) flag = 3U;
        }
    }

    return (flag << 12) | ((from & 63) << 6) | (to & 63);
}

void d(Position *board)
{
    char piecelowercase[] = {'p', 'b', 'n', 'r', 'q', 'k'};
    char pieceuppercase[] = {'P', 'B', 'N', 'R', 'Q', 'K'};

    char board8x8[8][8] = {0};

    for (int sq = 0; sq < 64; sq++)
    {
        board8x8[sq / 8][sq % 8] = '.';

        for (int piece = 0; piece < 6; piece++)
        {
            if ((board->pieces[piece] >> sq) & 1)
            {
                if ((board->color[1] >> sq) & 1)
                    board8x8[sq / 8][sq % 8] = piecelowercase[piece];
                else if ((board->color[0] >> sq) & 1)
                    board8x8[sq / 8][sq % 8] = pieceuppercase[piece];

                break;
            }
        }
    }

    for (int y = 0; y < 8; y++)
    {
        printf("     ");
        for (int x = 0; x < 8; x++)
            printf("%c ", board8x8[y][x]);

        printf("\n");
    }

}

static int parse_depth(const char *s)
{
    if (!s || !*s)
        return -1;

    int depth = 0;

    for (int i = 0; s[i] != '\0'; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return -1;

        if (depth > (INT_MAX - 9) / 10)
            return -1;

        depth = depth * 10 + (s[i] - '0');
    }

    return depth;
}

void uciStart(void)
{
    Position board = {0};

    fenRead(&board,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
            "w", "KQkq", "-");

    char line[20000];
    char *tokens[8850];

    while (1)
    {
        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\r\n")] = '\0';

        int t = 0;

        for (char *tok = strtok(line, " \t");
             tok != NULL && t < 8849;
             tok = strtok(NULL, " \t"))
        {
            tokens[t++] = tok;
        }

        tokens[t] = NULL;

        if (!tokens[0])
            continue;

        if (strcmp(tokens[0], "quit") == 0)
        {
            break;
        }
        else if (strcmp(tokens[0], "position") == 0)
        {
            if (!tokens[1])
            {
                printf("position: missing argument\n");
            }
            else if (strcmp(tokens[1], "startpos") == 0)
            {
                fenRead(&board,
                        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                        "w", "KQkq", "-");

                int i = 2;

                if (tokens[i] && strcmp(tokens[i], "moves") == 0)
                    i++;

                while (tokens[i])
                {
                    uint16_t move = parsemove(&board, tokens[i]);
                    moveint(&board, move);
                    i++;
                }
            }
            else if (strcmp(tokens[1], "fen") == 0)
            {
                if (!tokens[2] || !tokens[3] || !tokens[4] ||
                    !tokens[5] || !tokens[6] || !tokens[7])
                {
                    printf("position: incomplete fen\n");
                    continue;
                }

                fenRead(&board,
                        tokens[2], tokens[3], tokens[4],
                        tokens[5]);

                int i = 8;

                if (tokens[i] && strcmp(tokens[i], "moves") == 0)
                    i++;

                while (tokens[i])
                {
                    uint16_t move = parsemove(&board, tokens[i]);
                    moveint(&board, move);
                    i++;
                }
            }
            else
            {
                printf("position: unknown argument '%s'\n", tokens[1]);
            }
        }
        else if (strcmp(tokens[0], "go") == 0)
        {
            if (!tokens[1] || strcmp(tokens[1], "perft") != 0)
            {
                printf("go: only perft is supported\n");
                continue;
            }

            int depth = parse_depth(tokens[2]);

            if (depth < 0)
            {
                printf("go perft: invalid or missing depth\n");
                continue;
            }

            int divide = 0;
            int bulk = 0;
            int popcnt = 0;
            int valid = 1;

            for (int i = 3; tokens[i] != NULL; i++)
            {
                if (strcmp(tokens[i], "bulk") == 0)
                {
                    bulk = 1;
                }
                else if (strcmp(tokens[i], "popcnt") == 0)
                {
                    popcnt = 1;
                }
                else if (strcmp(tokens[i], "divide") == 0)
                {
                    if (!tokens[i + 1])
                    {
                        printf("go perft divide: missing value\n");
                        valid = 0;
                        break;
                    }

                    divide = parse_depth(tokens[++i]);

                    if (divide < 0)
                    {
                        printf("go perft: invalid divide value\n");
                        valid = 0;
                        break;
                    }
                }
                else
                {
                    printf("go perft: unknown argument '%s'\n", tokens[i]);
                    valid = 0;
                    break;
                }
            }

            if (!valid)
                continue;

            uint64_t total_nodes = 0;

            struct timespec start_time, stop_time;

#ifdef CLOCK_MONOTONIC_RAW
            clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
#else
            clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

            if (popcnt)
                total_nodes = perftpopcnt(&board, depth);
            else if (bulk)
                total_nodes = perftbulk(&board, depth);
            else
                total_nodes = perft(&board, depth, divide);

#ifdef CLOCK_MONOTONIC_RAW
            clock_gettime(CLOCK_MONOTONIC_RAW, &stop_time);
#else
            clock_gettime(CLOCK_MONOTONIC, &stop_time);
#endif

            long sec = stop_time.tv_sec - start_time.tv_sec;
            long nsec = stop_time.tv_nsec - start_time.tv_nsec;

            if (nsec < 0)
            {
                sec--;
                nsec += 1000000000L;
            }

            double seconds = (double)sec +
                             (double)nsec / 1000000000.0;

            double nps = seconds > 0.0
                             ? (double)total_nodes / seconds
                             : 0.0;

            printf("Total Nodes: %" PRIu64 "\n", total_nodes);
            printf("Elapsed time: %.3f ms\n", seconds * 1000.0);
            printf("N/S: %.0f\n", nps);
        }
        else if (strcmp(tokens[0], "d") == 0)
        {
            d(&board);
        }
        else
        {
            printf("unknown argument: %s\n", tokens[0]);
        }

        fflush(stdout);
    }
}
