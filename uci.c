#include "eval.h"
#include "fen.h"
#include "lmath.h"
#include "play.h"
#include "search.h"
#include "tt.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#include <stdlib.h>
#include "text.h"

#define MAX_GAME_PLY 2048

extern uint64_t game_history[MAX_GAME_PLY];
extern int game_history_count;

#define SOFT_NODES_HARD_MULTIPLIER 200

static bool use_soft_nodes = false;

static int str_eq_ci(const char *a, const char *b)
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

static int setoption_int_value(char **tokens, int from)
{
    char *value_tok = NULL;
    for (int i = from; tokens[i] != NULL; i++)
    {
        if (str_eq_ci(tokens[i], "value") && tokens[i + 1])
        {
            value_tok = tokens[i + 1];
            break;
        }
    }

    if (!value_tok)
        return -1;

    int v = 0;
    for (int i = 0; value_tok[i] != '\0'; i++)
    {
        if (value_tok[i] < '0' || value_tok[i] > '9')
            return -1;
        v = v * 10 + (value_tok[i] - '0');
    }
    return v;
}

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

static uint64_t genfens_rng_state = 0x9E3779B97F4A7C15ULL;

static uint64_t genfens_rand64(void)
{
    uint64_t z = (genfens_rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

#define GENFENS_RANDOM_PLIES 8
#define GENFENS_MAX_BOOK_LINES 100000

static void boardToFen(Position *board, char *out)
{
    int idx = 0;

    for (int row = 0; row < 8; row++)
    {
        int empty = 0;
        for (int file = 0; file < 8; file++)
        {
            int sq = row * 8 + file;
            int piece = board->mailbox[sq];

            if (piece == 6) // empty square
            {
                empty++;
                continue;
            }

            if (empty)
            {
                out[idx++] = (char)('0' + empty);
                empty = 0;
            }

            static const char letters[6] = {'p', 'b', 'n', 'r', 'q', 'k'};
            char c = letters[piece];

            if ((board->color[0] >> sq) & 1) // white
                c = (char)toupper((unsigned char)c);

            out[idx++] = c;
        }

        if (empty)
            out[idx++] = (char)('0' + empty);

        if (row != 7)
            out[idx++] = '/';
    }

    out[idx++] = ' ';
    out[idx++] = board->turn == 0 ? 'w' : 'b';
    out[idx++] = ' ';

    int castle_start = idx;
    if (board->castling & (1U << WHITE_KINGSIDE))
        out[idx++] = 'K';
    if (board->castling & (1U << WHITE_QUEENSIDE))
        out[idx++] = 'Q';
    if (board->castling & (1U << BLACK_KINGSIDE))
        out[idx++] = 'k';
    if (board->castling & (1U << BLACK_QUEENSIDE))
        out[idx++] = 'q';
    if (idx == castle_start)
        out[idx++] = '-';
    out[idx++] = ' ';

    if (board->epsquare != -1)
    {
        int file = board->epsquare % 8;
        int rank = 8 - (board->epsquare / 8);
        out[idx++] = (char)('a' + file);
        out[idx++] = (char)('0' + rank);
    }
    else
    {
        out[idx++] = '-';
    }
    out[idx++] = ' ';

    idx += sprintf(out + idx, "%d %d", board->halfmoves, board->fullmoves);
    out[idx] = '\0';
}

static int genfens_play_random(Position *board, int plies)
{
    for (int i = 0; i < plies; i++)
    {
        MoveList list = {0};
        legalMoveGen(board, &list);

        if (list.offset == 0)
            return 0;

        unsigned int pick = (unsigned int)(genfens_rand64() % list.offset);
        makeMove(board, &list, (int)pick);
    }

    MoveList final_list = {0};
    legalMoveGen(board, &final_list);
    return final_list.offset != 0;
}

static char **genfens_load_book(const char *path, int *count_out)
{
    *count_out = 0;

    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    int cap = 1024;
    char **lines = (char **)malloc(cap * sizeof(char *));
    char linebuf[256];
    int count = 0;

    while (fgets(linebuf, sizeof(linebuf), f))
    {
        linebuf[strcspn(linebuf, "\r\n")] = '\0';
        if (strlen(linebuf) < 6)
            continue;

        if (count >= cap)
        {
            cap *= 2;
            lines = (char **)realloc(lines, cap * sizeof(char *));
        }

        lines[count] = (char *)malloc(strlen(linebuf) + 1);
        strcpy(lines[count], linebuf);
        count++;

        if (count >= GENFENS_MAX_BOOK_LINES)
            break;
    }

    fclose(f);
    *count_out = count;
    return lines;
}

void genfensRun(int argc, char **argv)
{
    char cmdbuf[4096];
    cmdbuf[0] = '\0';
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            strncat(cmdbuf, " ", sizeof(cmdbuf) - strlen(cmdbuf) - 1);
        strncat(cmdbuf, argv[i], sizeof(cmdbuf) - strlen(cmdbuf) - 1);
    }

    char *new_argv[64];
    int new_argc = 0;
    new_argv[new_argc++] = argv[0];

    for (char *tok = strtok(cmdbuf, " \t");
         tok && new_argc < 63;
         tok = strtok(NULL, " \t"))
    {
        new_argv[new_argc++] = tok;
    }
    new_argv[new_argc] = NULL;

    argc = new_argc;
    argv = new_argv;

    if (argc < 3)
    {
        fprintf(stderr, "genfens: usage: genfens <count> [seed <n>] [book <path|None>]\n");
        return;
    }

    uint64_t how_many = 0;
    int valid = 1;
    for (int i = 0; argv[2][i] != '\0'; i++)
    {
        if (argv[2][i] < '0' || argv[2][i] > '9')
        {
            valid = 0;
            break;
        }
        how_many = how_many * 10 + (argv[2][i] - '0');
    }

    if (!valid)
    {
        fprintf(stderr, "genfens: invalid count '%s'\n", argv[2]);
        return;
    }

    uint64_t seed = 0;
    char *book_path = NULL;

    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "seed") == 0 && i + 1 < argc)
        {
            seed = 0;
            for (int j = 0; argv[i + 1][j] != '\0'; j++)
                seed = seed * 10 + (argv[i + 1][j] - '0');
            i++;
        }
        else if (strcmp(argv[i], "book") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[i + 1], "None") != 0)
                book_path = argv[i + 1];
            i++;
        }
    }

    genfens_rng_state = seed ? seed : 0x9E3779B97F4A7C15ULL;

    int book_count = 0;
    char **book_lines = book_path ? genfens_load_book(book_path, &book_count) : NULL;

    if (book_path && !book_lines)
        fprintf(stderr, "genfens: could not open book '%s', using startpos\n", book_path);

    for (uint64_t n = 0; n < how_many; n++)
    {
        Position genboard = {0};
        int success = 0;

        while (!success)
        {
            if (book_count > 0)
            {
                int idx = (int)(genfens_rand64() % (uint64_t)book_count);
                char parts[6][256] = {{0}};
                char linecopy[256];
                strncpy(linecopy, book_lines[idx], sizeof(linecopy) - 1);
                linecopy[sizeof(linecopy) - 1] = '\0';

                char *tok = strtok(linecopy, " ");
                int p = 0;
                while (tok && p < 6)
                {
                    strncpy(parts[p], tok, sizeof(parts[p]) - 1);
                    tok = strtok(NULL, " ");
                    p++;
                }

                fenRead(&genboard,
                        parts[0],
                        p > 1 ? parts[1] : "w",
                        p > 2 ? parts[2] : "KQkq",
                        p > 3 ? parts[3] : "-",
                        p > 4 ? parts[4] : "0",
                        p > 5 ? parts[5] : "1");
            }
            else
            {
                fenRead(&genboard,
                        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
                        "w", "KQkq", "-", "0", "1");
            }

            success = genfens_play_random(&genboard, GENFENS_RANDOM_PLIES);
        }

        char fenbuf[128];
        boardToFen(&genboard, fenbuf);
        printf("info string genfens %s\n", fenbuf);
        fflush(stdout);
    }

    if (book_lines)
    {
        for (int i = 0; i < book_count; i++)
            free(book_lines[i]);
        free(book_lines);
    }

    fflush(stdout);
}

static uint64_t parse_u64(const char *s)
{
    uint64_t v = 0;
    for (int i = 0; s[i] != '\0'; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        v = v * 10ULL + (uint64_t)(s[i] - '0');
    }
    return v;
}

static bool parse_go(char **tokens, Position *board, stopConditions *stop)
{
    long long wtime = 0, btime = 0, winc = 0, binc = 0;
    long long movetime = 0;
    uint64_t max_nodes = 0;
    int depth = 0;
    bool infinite = false;

    memset(stop, 0, sizeof *stop);

    for (int i = 1; tokens[i] != NULL; i++)
    {
        const char *t = tokens[i];

        if (strcmp(t, "infinite") == 0)
        {
            infinite = true;
            continue;
        }

        if (strcmp(t, "mate") == 0 || strcmp(t, "searchmoves") == 0)
        {
            printf("info string ignoring command\n");
            return false;
        }

        if (strcmp(t, "wtime") != 0 && strcmp(t, "btime") != 0 &&
            strcmp(t, "winc") != 0 && strcmp(t, "binc") != 0 &&
            strcmp(t, "movetime") != 0 && strcmp(t, "depth") != 0 &&
            strcmp(t, "nodes") != 0)
        {
            printf("info string ignoring command\n");
            return false;
        }

        if (tokens[i + 1] == NULL)
        {
            printf("info string ignoring command\n");
            return false;
        }

        const char *value = tokens[++i];

        if (strcmp(t, "wtime") == 0)
            wtime = matoi(value);
        else if (strcmp(t, "btime") == 0)
            btime = matoi(value);
        else if (strcmp(t, "winc") == 0)
            winc = matoi(value);
        else if (strcmp(t, "binc") == 0)
            binc = matoi(value);
        else if (strcmp(t, "movetime") == 0)
        {
            movetime = matoi(value);
            if (movetime < 1)
                movetime = 1;
        }
        else if (strcmp(t, "depth") == 0)
        {
            depth = matoi(value);
            if (depth < 1)
                depth = 1;
            if (depth > MAX_DEPTH)
                depth = MAX_DEPTH;
        }
        else if (strcmp(t, "nodes") == 0)
        {
            max_nodes = parse_u64(value);
            if (max_nodes < 1)
                max_nodes = 1;
        }
    }

    stop->start_time = get_time_ms();
    stop->depth = depth;

    if (infinite)
    {
    }
    else if (movetime > 0)
    {
        stop->max_time = movetime;
    }
    else if (max_nodes > 0)
    {
        if (use_soft_nodes)
        {
            stop->soft_nodes = max_nodes;
            stop->max_nodes = max_nodes * SOFT_NODES_HARD_MULTIPLIER;
        }
        else
        {
            stop->max_nodes = max_nodes;
        }
    }
    else if (wtime > 0 || btime > 0)
    {
        int increment = 0;
        int time_left = 0;

        if (board->turn == 0)
        {
            increment = (int)(winc * 0.7);
            time_left = (int)wtime;
        }
        else
        {
            increment = (int)(binc * 0.7);
            time_left = (int)btime;
        }

        int overhead = 100;

        int soft = time_left / 30 + increment;
        int hard = time_left / 3 + increment;

        if (soft < 10)
            soft = 10;
        if (hard < soft)
            hard = soft;
        if (hard > time_left - overhead)
            hard = time_left - overhead;
        if (soft > hard)
            soft = hard;
        if (hard <= 0)
            hard = 50;

        stop->soft_time = soft;
        stop->max_time = hard;
    }

    return true;
}

void uciStart()
{
    init_lmr();
    tt_init(TT_DEFAULT_MB);

    Position board = {0};
    Position copyboard = {0};

    char run = 1;

    fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq",
            "-", "0", "1");
    game_history_count = 0;

    char line[20000];

    char *tokens[8850];
    while (run)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            search_request_stop();
            search_wait();
            printf("Bye\n");
            tt_free();
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
            search_request_stop();
            search_wait();
            printf("bye\n");
            tt_free();
            break;
        }
        else if (strcmp(tokens[0], "uci") == 0)
        {
            printf("id name Crustik 0.4.0\nid author Dejon Eltahan\n");
            printf("option name Hash type spin default %d min %d max %d\n", TT_DEFAULT_MB, TT_MIN_MB, TT_MAX_MB);
            printf("option name Threads type spin default %d min 1 max %d\n", DEFAULT_THREADS, MAX_THREADS);
            printf("option name SoftNodes type check default false\n");
            printf("uciok\n");
        }
        else if (strcmp(tokens[0], "isready") == 0)
        {
            printf("readyok\n");
        }
        else if (strcmp(tokens[0], "stop") == 0)
        {
            search_request_stop();
        }
        else if (strcmp(tokens[0], "setoption") == 0)
        {
            search_wait();

            if (tokens[1] && str_eq_ci(tokens[1], "name") && tokens[2] &&
                str_eq_ci(tokens[2], "Hash"))
            {
                int mb = setoption_int_value(tokens, 3);

                if (mb > 0)
                    tt_resize((size_t)mb);
                else
                    printf("setoption Hash: missing or invalid value\n");
            }
            else if (tokens[1] && str_eq_ci(tokens[1], "name") && tokens[2] &&
                     str_eq_ci(tokens[2], "Threads"))
            {
                int n = setoption_int_value(tokens, 3);

                if (n > 0)
                {
                    if (n > MAX_THREADS)
                        n = MAX_THREADS;
                    search_set_threads(n);
                }
                else
                {
                    printf("setoption Threads: missing or invalid value\n");
                }
            }
            else if (tokens[1] && str_eq_ci(tokens[1], "name") && tokens[2] &&
                     str_eq_ci(tokens[2], "SoftNodes"))
            {
                char *value_tok = NULL;
                for (int i = 3; tokens[i] != NULL; i++)
                {
                    if (str_eq_ci(tokens[i], "value") && tokens[i + 1])
                    {
                        value_tok = tokens[i + 1];
                        break;
                    }
                }

                if (value_tok)
                {
                    if (str_eq_ci(value_tok, "true"))
                        use_soft_nodes = true;
                    else if (str_eq_ci(value_tok, "false"))
                        use_soft_nodes = false;
                    else
                        printf("setoption SoftNodes: invalid value '%s'\n", value_tok);
                }
                else
                {
                    printf("setoption SoftNodes: missing value\n");
                }
            }
        }
        else if (strcmp(tokens[0], "ucinewgame") == 0)
        {
            search_wait();

            tt_clear();
            reset_history();
            fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w",
                    "KQkq", "-", "0", "1");
            game_history_count = 0;
        }
        else if (strcmp(tokens[0], "position") == 0)
        {
            search_wait();

            if (tokens[1] == NULL)
            {
                printf("position: missing argument\n");
            }
            else if (strcmp(tokens[1], "startpos") == 0)
            {
                fenRead(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w",
                        "KQkq", "-", "0", "1");
                game_history_count = 0;

                int i = 2;
                if (tokens[i] && strcmp(tokens[i], "moves") == 0)
                    i++;
                while (tokens[i])
                {
                    uint16_t move = parsemove(&board, tokens[i]);
                    moveint(&board, move);
                    if (game_history_count < MAX_GAME_PLY)
                        game_history[game_history_count++] = board.hash;
                    i++;
                }
            }
            else if (strcmp(tokens[1], "fen") == 0)
            {
                if (tokens[2] == NULL)
                {
                    printf("position: missing fen\n");
                }
                else
                {

                    fenRead(&board, tokens[2], tokens[3], tokens[4], tokens[5], tokens[6], tokens[7]);
                    game_history_count = 0;

                    int i = 8;
                    if (tokens[i] && strcmp(tokens[i], "moves") == 0)
                        i++;
                    while (tokens[i])
                    {
                        uint16_t move = parsemove(&board, tokens[i]);
                        moveint(&board, move);
                        if (game_history_count < MAX_GAME_PLY)
                            game_history[game_history_count++] = board.hash;
                        i++;
                    }
                }
            }
            else
            {
                printf("position: unknown argument '%s'\n", tokens[1]);
            }
        }
        else if (strcmp(tokens[0], "go") == 0)
        {
            if (tokens[1] && strcmp(tokens[1], "perft") == 0)
            {
                if (!tokens[2])
                {
                    printf("go perft: missing depth value\n");
                }
                else
                {
                    int depth = 0;
                    int divide = 0;
                    bool bulk = 0;
                    int valid = 1;

                    for (int i = 0; tokens[2][i] != '\0'; i++)
                    {
                        if (tokens[2][i] < '0' || tokens[2][i] > '9')
                        {
                            printf("Invalid depth value\n");
                            valid = 0;
                            break;
                        }
                        depth = depth * 10 + (tokens[2][i] - '0');
                    }

                    for (int i = 3; valid && tokens[i] != NULL; i++)
                    {
                        if (strcmp(tokens[i], "bulk") == 0)
                        {
                            bulk = 1;
                        }
                        else if (strcmp(tokens[i], "divide") == 0)
                        {
                            i++;
                            if (tokens[i] == NULL)
                            {
                                printf("go perft divide: missing divide depth/value\n");
                                valid = 0;
                                break;
                            }

                            divide = 0;
                            for (int j = 0; tokens[i][j] != '\0'; j++)
                            {
                                if (tokens[i][j] < '0' || tokens[i][j] > '9')
                                {
                                    printf("Invalid divide value\n");
                                    valid = 0;
                                    break;
                                }
                                divide = divide * 10 + (tokens[i][j] - '0');
                            }
                        }
                        else
                        {
                            printf("go perft: unknown argument: %s\n", tokens[i]);
                            valid = 0;
                        }
                    }

                    if (valid)
                    {
                        uint64_t total_nodes = 0;
                        struct timespec start_time, stop_time;

#ifdef CLOCK_MONOTONIC_RAW
                        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
#else
                        clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif
                        if (bulk)
                        {
                            total_nodes = perftbulk(&board, depth);
                        }
                        else
                        {
                            total_nodes = perft(&board, depth, divide);
                        }

#ifdef CLOCK_MONOTONIC_RAW
                        clock_gettime(CLOCK_MONOTONIC_RAW, &stop_time);
#else
                        clock_gettime(CLOCK_MONOTONIC, &stop_time);
#endif

                        long sec = stop_time.tv_sec - start_time.tv_sec;
                        long nsec = stop_time.tv_nsec - start_time.tv_nsec;

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
            }
            else if (search_is_running())
            {
                printf("info string ignoring command\n");
            }
            else
            {
                stopConditions stop = {0};
                if (parse_go(tokens, &board, &stop))
                    search_start_async(&board, &stop);
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
        else if (strcmp(tokens[0], "clear") == 0)
        {
            printf("\e[1;1H\e[2J"); // Clear screen
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