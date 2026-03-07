#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lmath.h"
#include "play.h"
#include "eval.h"
#include "fen.h"
#include "search.h"

char *movestring(uint16_t movex)
{
    static char buf[32]; // Enough to hold a move string like "e2e4q"

    int from = (movex >> 6) & 0x3F;
    int to = movex & 0x3F;
    int flag = (movex >> 12) & 0xF;

    int x1 = from % 8;
    int y1 = 8 - (from / 8);
    int x2 = to % 8;
    int y2 = 8 - (to / 8);

    char promotion = 0;
    if (flag == 5)
        promotion = 'b';
    else if (flag == 6)
        promotion = 'n';
    else if (flag == 7)
        promotion = 'r';
    else if (flag == 8)
        promotion = 'q';

    if (promotion)
        snprintf(buf, sizeof(buf), "%c%i%c%i%c", 'a' + x1, y1, 'a' + x2, y2, promotion);
    else
        snprintf(buf, sizeof(buf), "%c%i%c%i", 'a' + x1, y1, 'a' + x2, y2);

    return buf;
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
    case 'b':
        flag = 5U;

        break;
    case 'n':
        flag = 6U;

        break;
    case 'r':
        flag = 7U;

        break;
    case 'q':
        flag = 8U;
        break;

    default:
        break;
    }
    int from = x1 + (y1 << 3);
    int to = x2 + (y2 << 3);
    if (board->mailbox[from] == 5)
    {
        switch (board->turn)
        {
        case 0:
            switch (from)
            {
            case E1:
                switch (to)
                {
                case G1:
                    flag = 1U;
                    break;
                case C1:
                    flag = 2U;
                    break;
                }
                break;
            }
            break;
        case 1:
            switch (from)
            {
            case E8:
                switch (to)
                {
                case G8:
                    flag = 4U;
                    break;
                case C8:
                    flag = 3U;
                    break;
                }
                break;
            }
            break;
        }
    }

    return (flag << 12) | ((from & 63) << 6) | (to & 63);
}

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
                fenRead(&board, tokens[2], tokens[3], tokens[4], tokens[5], tokens[6], tokens[7]);

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
                    searchOutput result = search(&board, tokens[2][0] - '0', 0, -32000, 32000);

                    if (result.move == 0)
                    {
                        uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                        if (!king_bb)
                            break;
                        int king_pos = __builtin_ctzll(king_bb);
                        if (squareAttacked(&board, king_pos, !board.turn)){
                            puts("bestmove 0000");
                            printf("%s is checkmated\n", board.turn ? "Black" : "White");
                        }
                        else{
                            puts("bestmove 0000");
                            printf("Stalemate\n");
                        }
                    }

                    moveint(&board, result.move);
                    printf("bestmove %s\n", movestring(result.move));
                    // d(&board);
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