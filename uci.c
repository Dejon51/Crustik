#include "eval.h"
#include "fen.h"
#include "lmath.h"
#include "play.h"
#include "search.h"
#include "tt.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "text.h"

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
    case 5:
        promotion = 'b';
        break;
    case 6:
        promotion = 'n';
        break;
    case 7:
        promotion = 'r';
        break;
    case 8:
        promotion = 'q';
        break;
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

    int from = x1 + ((7 - y1) << 3);
    int to = x2 + ((7 - y2) << 3);

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
        printf("     ");
        for (int x = 0; x < 8; x++)
        {

            printf("%c ", board8x8[y][x]);
        }
        printf("\n");
    }
    printf("HASH: 0x%" PRIx64 "\n", board->hash);
}

void uciStart()
{
    Position board = {0};
    Position copyboard = {0};

    char run = 1;

    fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq",
            "-", "0", "1");

    char line[20000];

    char *tokens[8850];
    while (run)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("Bye\n");
            return;
        }
        line[strcspn(line, "\n")] = '\0';

        int t = 0;
        for (char *tok = strtok(line, " \n"); tok != NULL && t < 8849;
             tok = strtok(NULL, " \n"))
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
            printf("id name Crustik 0.2.0\nid author Dejon Eltahan\n");
            printf("uciok\n");
        }
        else if (strcmp(tokens[0], "isready") == 0)
        {
            printf("readyok\n");
        }
        else if (strcmp(tokens[0], "ucinewgame") == 0)
        {
            tt_clear();
            clear_ordering_tables();
            fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w",
                    "KQkq", "-", "0", "1");
        }
        else if (strcmp(tokens[0], "position") == 0)
        {
            if (tokens[1] == NULL)
            {
                printf("position: missing argument\n");
            }
            else if (strcmp(tokens[1], "startpos") == 0)
            {
                fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w",
                        "KQkq", "-", "0", "1");

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
            else if (strcmp(tokens[1], "nodes") == 0)
            {
                if (tokens[2] == NULL)
                {
                    printf("go nodes: missing node count\n");
                }
                else
                {
                    uint64_t max_nodes = 0;
                    for (int i = 0; tokens[2][i] != '\0'; i++)
                        max_nodes = max_nodes * 10 + (tokens[2][i] - '0');

                    if (max_nodes <= 0)
                        max_nodes = 1000;

                    stopConditions stop = {};
                    stop.start_time = get_time_ms();
                    stop.max_time = 0;
                    stop.max_nodes = max_nodes;
                    stop.nodes = 0;
                    stop.stop = 0;

                    uint16_t result = iterative_deepening(&board, &stop);

                    if (result == 0)
                    {
                        uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                        if (!king_bb)
                            break;
                        int king_pos = __builtin_ctzll(king_bb);
                        if (squareAttacked(&board, king_pos, !board.turn))
                        {
                            printf("%s is checkmated\n", board.turn ? "Black" : "White");
                        }
                        else
                        {
                            printf("Stalemate\n");
                        }
                    }
                    else
                    {
                        movestring(result);
                    }
                }
            }
            else if (strcmp(tokens[1], "movetime") == 0)
            {
                if (tokens[2] == NULL)
                {
                    printf("go movetime: missing move time\n");
                }
                else
                {

                    int movetime = 0;
                    for (int i = 0; tokens[2][i] != '\0'; i++)
                    {
                        movetime = movetime * 10 + (tokens[2][i] - '0');
                    }
                    if (movetime <= 0)
                        movetime = 100;

                    stopConditions stop = {0};
                    stop.start_time = get_time_ms();
                    stop.max_time = movetime;

                    uint16_t result = iterative_deepening(&board, &stop);

                    if (result == 0)
                    {
                        uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                        if (!king_bb)
                            break;
                        int king_pos = __builtin_ctzll(king_bb);
                        if (squareAttacked(&board, king_pos, !board.turn))
                        {
                            printf("%s is checkmated\n", board.turn ? "Black" : "White");
                        }
                        else
                        {
                            printf("Stalemate\n");
                        }
                    }
                    else
                    {
                        movestring(result);
                    }
                }
            }

            else if (strcmp(tokens[1], "depth") == 0)
            {

                if (tokens[2] == NULL)
                {
                    printf("go depth: missing depth value\n");
                }
                else
                {
                    int depth = 0;
                    for (int i = 0; tokens[2][i] != '\0'; i++) depth = depth * 10 + (tokens[2][i] - '0');

                    stopConditions stop = {0};
                    stop.depth = depth;
                    

                    uint16_t result = iterative_deepening(&board, &stop);

                    if (result == 0)
                    {
                        uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                        if (!king_bb) break;
                        int king_pos = __builtin_ctzll(king_bb);
                        if (squareAttacked(&board, king_pos, !board.turn))
                        {
                            printf("%s is checkmated\n", board.turn ? "Black" : "White");
                        }
                        else
                        {
                            printf("Stalemate\n");
                        }
                    }
                    else
                    {
                        movestring(result);
                    }
            }
            }
            else if (strcmp(tokens[1], "perft") == 0)
            {
                if (!tokens[2])
                {
                    printf("go perft: missing depth value\n");
                }
                else
                {
                    int divide = 0;


                    if (tokens[3] && strcmp(tokens[3], "divide") == 0 && tokens[4])
                    {
                        divide = 0;

                        for (int i = 0; tokens[4][i] != '\0'; i++)
                        {
                            if (tokens[4][i] < '0' || tokens[4][i] > '9')
                            {
                                printf("Invalid divide value\n");
                                divide = 0;
                                break;
                            }
                            divide = divide * 10 + (tokens[4][i] - '0');
                        }
                    }

                    int depth = 0;
                    for (int i = 0; tokens[2][i] != '\0'; i++)
                    {
                        if (tokens[2][i] < '0' || tokens[2][i] > '9')
                        {
                            printf("Invalid depth value\n");
                            depth = 0;
                            break;
                        }
                        depth = depth * 10 + (tokens[2][i] - '0');
                    }

                    struct timespec start, stop;

#ifdef __linux__
                    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                    uint64_t total_nodes = perft(&board, depth, divide);
                    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
#else
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    uint64_t total_nodes = perft(&board, depth, divide);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
#endif


                    long sec = stop.tv_sec - start.tv_sec;
                    long nsec = stop.tv_nsec - start.tv_nsec;

                    if (nsec < 0)
                    {
                        sec -= 1;
                        nsec += 1000000000L;
                    }

                    double seconds = (double)sec + (double)nsec / 1000000000.0;
                    double nps = (seconds > 0.0) ? (double)total_nodes / seconds : 0.0;
                    double elapsed_ms = seconds * 1000.0;

                    printf("Total Nodes: %" PRIu64 "\n", total_nodes);
                    printf("Elapsed time: %.3f ms\n", elapsed_ms);
                    printf("N/S: %.0f\n", nps);
                }
            }
            else
            {
                int white_time = 0;
                int black_time = 0;
                int white_increment = 0;
                int black_increment = 0;

                for (int i = 1; tokens[i] != NULL; i++)
                {
                    if (strcmp(tokens[i], "wtime") == 0 && tokens[i + 1])
                    {
                        white_time = matoi(tokens[i + 1]);
                        i++;
                    }
                    else if (strcmp(tokens[i], "btime") == 0 && tokens[i + 1])
                    {
                        black_time = matoi(tokens[i + 1]);
                        i++;
                    }
                    else if (strcmp(tokens[i], "winc") == 0 && tokens[i + 1])
                    {
                        white_increment = matoi(tokens[i + 1]);
                        i++;
                    }
                    else if (strcmp(tokens[i], "binc") == 0 && tokens[i + 1])
                    {
                        black_increment = matoi(tokens[i + 1]);
                        i++;
                    }
                    else
                    {
                        printf("go: unknown argument: %s\n", tokens[1]);
                    }
                }
                int increment = 0;
                int time_move = 0;
                if (board.turn == 0)
                {
                    increment = white_increment * 0.7;
                    time_move = white_time / 30 + increment;
                }
                else if (board.turn == 1)
                {
                    increment = black_increment * 0.7;
                    time_move = black_time / 30 + increment;
                }

                int movetime = time_move;
                if (movetime <= 0)
                    movetime = 100;

                stopConditions stop = {};
                stop.start_time = get_time_ms();
                stop.max_time = movetime;
                stop.max_nodes = 0;
                stop.nodes = 0;
                stop.stop = 0;

                uint16_t result = iterative_deepening(&board, &stop);

                if (result == 0)
                {
                    uint64_t king_bb = board.pieces[5] & board.color[board.turn];
                    if (!king_bb)
                        break;
                    int king_pos = __builtin_ctzll(king_bb);
                    if (squareAttacked(&board, king_pos, !board.turn))
                    {
                        printf("%s is checkmated\n", board.turn ? "Black" : "White");
                    }
                    else
                    {
                        printf("Stalemate\n");
                    }
                }
                else
                {
                    movestring(result);
                }
            }
        }
        else if (strcmp(tokens[0], "d") == 0)
        {
            d(&board);
        }
        else if (strcmp(tokens[0], "help") == 0)
        {
            printf("%s", HELP_TEXT);
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
            printf("unknown argument: %s\n", tokens[0]);
        }
        fflush(stdout);
    }
}